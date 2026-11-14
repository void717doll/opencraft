/*
 * =====================================================================================
 *
 *  OpenMiner
 *
 *  Copyright (C) 2018-2020 Unarelith, Quentin Bazin <openminer@unarelith.net>
 *  Copyright (C) 2019-2020 the OpenMiner contributors (see CONTRIBUTORS.md)
 *
 *  This file is part of OpenMiner.
 *
 *  OpenMiner is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  OpenMiner is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * =====================================================================================
 */
#include <glm/gtx/quaternion.hpp>

#include "common/core/Registry.hpp"
#include "client/graphics/Camera.hpp"
#include "client/graphics/RenderTarget.hpp"
#include "client/core/Config.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/AnimationComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"
#include "common/world/World.hpp"

#include "client/gui/InventoryCube.hpp"
#include "common/core/GameClock.hpp"
#include "client/scene/component/HurtComponent.hpp"
#include "client/graphics/MobModel.hpp"
#include "client/graphics/ItemBillboard.hpp"
#include "client/scene/component/DrawableComponent.hpp"
#include "client/scene/component/ItemVisualComponent.hpp"
#include "client/scene/controller/RenderingController.hpp"

void RenderingController::update(entt::registry &registry) {
	registry.view<DrawableDef>().each([&](auto entity, auto &definition) {
		DrawableComponent &drawable = registry.get_or_emplace<DrawableComponent>(entity);
		if (definition.mobModel()) {
			if (auto *mob = dynamic_cast<MobModel *>(drawable.drawable()))
				mob->configure(definition.mobModel(), definition.mobScale(), definition.mobAppearance());
			else
				drawable.setDrawable<MobModel>(definition.mobModel(), definition.mobScale(), definition.mobAppearance());
		}
		else {
			const InventoryCubeDef &cubeDef = definition.getInventoryCubeDef();
			Registry &definitions = Registry::getInstance();
			bool sprite = false;
			if (definitions.hasBlock(cubeDef.blockID)) {
				const BlockState &state = definitions.getBlockFromStringID(cubeDef.blockID).getState(0);
				sprite = state.drawType() == BlockDrawType::XShape || !state.inventoryImage().empty();
			}
			if (definitions.hasBlock(cubeDef.blockID) && !sprite) {
				InventoryCube &cube = drawable.setDrawable<InventoryCube>(cubeDef.size, true);
				cube.setOrigin(cubeDef.origin);
				cube.updateVertexBuffer(definitions.getBlockFromStringID(cubeDef.blockID));
			}
			else if (definitions.hasItem(cubeDef.blockID))
				drawable.setDrawable<ItemBillboard>(cubeDef.blockID, cubeDef.size * 2);
		}
		registry.remove<DrawableDef>(entity);
	});
}

void RenderingController::draw(entt::registry &registry, RenderTarget &target, RenderStates states) {
	const auto *camera = dynamic_cast<const Camera *>(target.getView());
	registry.view<DrawableComponent, PositionComponent, RotationComponent>().each([&](auto entity, auto &drawable, auto &position, auto &rotation) {
		if (camera) {
			Vector3d center{position.x, position.y, position.z};
			double radius = 1;
			if (const auto *box = registry.try_get<DoubleBox>(entity)) {
				center += Vector3d{box->x + box->sizeX/2, box->y + box->sizeY/2, box->z + box->sizeZ/2};
				radius += std::sqrt(box->sizeX*box->sizeX + box->sizeY*box->sizeY + box->sizeZ*box->sizeZ)/2;
			}
			Vector3d delta = center - camera->getDPosition();
			double range = double(Config::renderDistance * CHUNK_WIDTH) + radius;
			if (delta.x*delta.x + delta.y*delta.y + delta.z*delta.z > range*range) return;
			const auto &forward = camera->getDirection();
			const auto &up = camera->getUpVector();
			glm::dvec3 direction{forward.x,forward.y,forward.z}, vertical{up.x,up.y,up.z};
			direction = glm::normalize(direction);
			auto right = glm::normalize(glm::cross(direction,vertical));
			vertical = glm::cross(right,direction);
			glm::dvec3 relative{delta.x,delta.y,delta.z};
			double depth = glm::dot(relative,direction);
			double height = std::tan(glm::radians(double(camera->getFieldOfView()))/2);
			double width = height * camera->getAspectRatio();
			if (depth < -radius || std::abs(glm::dot(relative,vertical)) > depth*height + radius*std::sqrt(1+height*height)
			 || std::abs(glm::dot(relative,right)) > depth*width + radius*std::sqrt(1+width*width)) return;
		}
		if (m_world) {
			int x = int(std::floor(position.x)), y = int(std::floor(position.y)), z = int(std::floor(position.z));
			const Chunk *chunk = m_world->getChunkAtBlockPos(x, y, z);
			u8 sun = 0, torch = 0;
			if (chunk && chunk->isInitialized()) {
				sun = chunk->lightmap().getSunlight(x & (CHUNK_WIDTH - 1), y & (CHUNK_DEPTH - 1), z & (CHUNK_HEIGHT - 1));
				torch = chunk->lightmap().getTorchlight(x & (CHUNK_WIDTH - 1), y & (CHUNK_DEPTH - 1), z & (CHUNK_HEIGHT - 1));
			}
			if (auto *cube = dynamic_cast<InventoryCube *>(drawable.drawable())) cube->setLight(sun, torch);
			else if (auto *item = dynamic_cast<ItemBillboard *>(drawable.drawable())) item->setLight(sun, torch);
			else if (auto *mob = dynamic_cast<MobModel *>(drawable.drawable())) mob->setLight(sun, torch);
		}
		if (auto *mob = dynamic_cast<MobModel *>(drawable.drawable())) {
			mob->updateMotion({position.x, position.y, position.z});
			const auto *hurt = registry.try_get<HurtComponent>(entity);
			mob->setHurt(hurt && GameClock::getInstance().getTicks() < hurt->until);
		}
		Transformable transformable;
		if (!dynamic_cast<MobModel *>(drawable.drawable()) && registry.all_of<AnimationComponent>(entity)) {
			auto &visual = registry.get_or_emplace<ItemVisualComponent>(entity);
			u64 now = GameClock::getInstance().getTicks();
			Vector3d displayed = visual.update({position.x, position.y, position.z}, now);
			double phase = double(now) / 1000.0 + double(u32(entity) % 32);
			float lift = dynamic_cast<ItemBillboard *>(drawable.drawable()) ? 0.125f : 0;
			displayed.z += lift + 0.04 * (1 + std::sin(phase * 2));
			transformable.setPosition(float(displayed.x), float(displayed.y), float(displayed.z));
			transformable.getRotationTransform().rotateZ(float(now % 4000) * 0.09f);
		}
		else {
			transformable.setPosition((float)position.x, (float)position.y, (float)position.z);
			transformable.getRotationTransform().getMatrix() = glm::toMat4(rotation.quat);
		}

		RenderStates drawStates = states;
		drawStates.transform *= transformable.getTransform();
		drawable.draw(target, drawStates);
	});
}
