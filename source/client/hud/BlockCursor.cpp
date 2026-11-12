#include "common/world/BetaPiston.hpp"
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
#include "common/world/BlockSupport.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include "common/core/GameClock.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/world/WorldCollision.hpp"
#include "common/world/BetaMining.hpp"
#include "common/world/BetaToolRules.hpp"
#include "common/world/LiquidState.hpp"
#include "common/core/Registry.hpp"
#include "common/core/input/GameKey.hpp"
#include "common/resource/ResourceHandler.hpp"

#include "client/audio/SoundManager.hpp"
#include "client/core/Config.hpp"
#include "client/graphics/Camera.hpp"
#include "client/graphics/MobModel.hpp"
#include "client/scene/component/DrawableComponent.hpp"
#include "client/core/input/GamePad.hpp"
#include "client/hud/BlockCursor.hpp"
#include "client/hud/Hotbar.hpp"
#include "client/math/BlockCursorRaycast.hpp"
#include "client/network/ClientCommandHandler.hpp"
#include "client/world/ClientPlayer.hpp"

BlockCursor::BlockCursor(ClientPlayer &player, ClientWorld &world, ClientCommandHandler &client)
	: m_player(player), m_world(world), m_client(client)
{
	m_blockDestroyTexture = &ResourceHandler::getInstance().get<Texture>("texture-block_destroy");

	m_vbo.setupDefaultLayout();

	m_animationVBO.setupDefaultLayout();

	const u16 indices[] = {
		0, 3, 4, 7,
		8, 11, 12, 15,

		16, 17, 16, 19,
		18, 17, 18, 19,

		20, 21, 20, 23,
		22, 21, 22, 23,
	};

	m_ibo.init(indices, sizeof(indices));
}

bool BlockCursor::hasSelectedEntity() const {
	Vector3d origin = m_player.camera().getDPosition();
	Vector3d direction{m_player.dirTargetedX(), m_player.dirTargetedY(), m_player.dirTargetedZ()};
	double limit = 3;
	if (m_selectedBlock.w != -1) {
		const BlockState *state = m_world.getBlockState(m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z);
		if (state) {
			double blockDistance = collision::rayDistance(origin, direction,
				collision::blockBox(*state, m_world.getData(m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z),
					m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z), limit);
			if (blockDistance >= 0) limit = blockDistance;
		}
	}
	bool selected = false;
	m_world.scene().registry().view<PositionComponent, DoubleBox>().each([&](auto entity, const auto &position, const auto &hitbox) {
		auto *drawable = m_world.scene().registry().try_get<DrawableComponent>(entity);
		auto *mob = drawable ? dynamic_cast<MobModel *>(drawable->drawable()) : nullptr;
		if (!mob || !mob->isTargetable()) return;
		if (position.dimension != m_player.dimension()) return;
		DoubleBox box = hitbox;
		box.move(position.x, position.y, position.z);
		double distance = collision::rayDistance(origin, direction, box, limit);
		if (distance >= 0 && distance < limit) selected = true;
	});
	return selected;
}

void BlockCursor::onEvent(const SDL_Event &event, const Hotbar &hotbar) {
	if (event.type == SDL_MOUSEBUTTONDOWN && hasSelectedEntity()) {
		if (event.button.button == SDL_BUTTON_LEFT) m_client.sendPlayerAttack();
		else if (event.button.button == SDL_BUTTON_RIGHT) m_client.sendPlayerInteract();
		reset();
		return;
	}
	if (hotbar.cursorPos() == -1) return;

	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT
	 && m_selectedBlock.w == -1 && hotbar.currentItem().canBeActivated()) {
		m_client.sendItemActivated({int(std::floor(m_player.x())), int(std::floor(m_player.y())), int(std::floor(m_player.z())), -1});
		return;
	}
	if (event.type == SDL_MOUSEBUTTONDOWN && m_selectedBlock.w != -1) {
		if (event.button.button == SDL_BUTTON_LEFT) {
			m_animationStart = GameClock::getInstance().getTicks();
			m_currentTool = &m_player.inventory().getStack((u16)hotbar.cursorPos(), 0);
			if (m_currentBlock && m_currentBlock->block().hasGroup("group:om_note_block")) m_client.sendNotePreview(m_selectedBlock);
		}
		else if (event.button.button == SDL_BUTTON_RIGHT)
			activateBlock(hotbar);
		else if (event.button.button == SDL_BUTTON_MIDDLE) {
			const auto *state = m_world.getBlockState(m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z);
			if (!state || !state->block().id() || state->drawType() == BlockDrawType::Liquid
			 || state->block().stringID() == "default:portal") return;
			const auto &block = state->block();
			std::string itemID = block.stringID();
			if (block.hasGroup("group:om_crop") || block.hasGroup("group:ci_ignore"))
				itemID = block.getState(0).itemDrop();
			if (m_player.pickItem(itemID)) {
				m_client.sendPlayerInvUpdate();
				m_client.sendPlayerHeldItemChanged(u8(m_player.heldItemSlot()), m_player.heldItemStack().item().id());
			}
			reset();
		}
	}
	else if (event.type == SDL_MOUSEBUTTONUP) {
		if (event.button.button == SDL_BUTTON_LEFT)
			reset();
		else if (event.button.button == SDL_BUTTON_RIGHT)
			m_lastActivationTime.reset();
	}
}

void BlockCursor::update(const Hotbar &hotbar) {
	bool selectedBlockChanged = false;
	glm::ivec4 selectedBlock = findSelectedBlock();
	if (selectedBlock.x != m_selectedBlock.x || selectedBlock.y != m_selectedBlock.y || selectedBlock.z != m_selectedBlock.z) {
		selectedBlockChanged = true;
	}

	m_selectedBlock = selectedBlock;

	m_currentBlock = m_world.getBlockState(m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z);
	// if (block.boundingBox().intersects(FloatBox{m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z, 1, 1, 1})) {
	// 	selectedBlockChanged = false;
	// 	m_selectedBlock.w = -1;
	// }

	if (!m_currentBlock) return;

	u32 ticks = GameClock::getInstance().getTicks();

	if (selectedBlockChanged)
		m_animationStart = (m_animationStart) ? ticks : 0;

	// FIXME: This should use block param
	u8f orientation = m_currentBlock->block().isRotatable() ? m_world.getData(selectedBlock.x, selectedBlock.y, selectedBlock.z) & 0x1F : 0;

	int cursorPos = hotbar.cursorPos();
	if (cursorPos != -1) {
		float timeToBreak = 0;
		if (m_animationStart) {
			const ItemStack &currentStack = m_player.inventory().getStack((u16)cursorPos, 0);
			if (m_currentTool->item().id() != currentStack.item().id()) {
				m_animationStart = ticks;
				m_currentTool = &currentStack;
			}
			else {
				const Vector3d &position = m_player.physicsPosition();
				int eyeX = int(std::floor(position.x)), eyeY = int(std::floor(position.y));
				int eyeZ = int(std::floor(position.z + 1.625));
				const BlockState *eye = m_world.getBlockState(eyeX, eyeY, eyeZ);
				bool submerged = eye && liquid::kind(eye->block()) == 1;
				if (submerged) {
					const BlockState *above = m_world.getBlockState(eyeX, eyeY, eyeZ + 1);
					bool covered = above && liquid::kind(above->block()) == 1;
					double height = liquid::height(eye->block(), m_world.getData(eyeX, eyeY, eyeZ), covered);
					submerged = position.z + 1.625 < eyeZ + height;
				}
				timeToBreak = m_player.isCreative() ? 0 : retro::survivalBreakTime(*m_currentBlock, currentStack.item(), submerged, m_player.onGround());

				if (ticks >= m_animationStart && ticks >= m_nextDigSound) {
					if (auto *sound = SoundManager::instance())
						sound->playBlock(*m_currentBlock, BlockSound::Dig, {m_selectedBlock.x + 0.5, m_selectedBlock.y + 0.5, m_selectedBlock.z + 0.5});
					m_nextDigSound = ticks + 200;
				}

				if ((float)ticks > (float)m_animationStart + timeToBreak * 1000.f) {
					if (auto *sound = SoundManager::instance())
						sound->playBlock(*m_currentBlock, BlockSound::Dug, {m_selectedBlock.x + 0.5, m_selectedBlock.y + 0.5, m_selectedBlock.z + 0.5});
					m_world.setBlock(m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z, 0);
					m_animationStart = ticks + 250;

					m_client.sendPlayerDigBlock(m_selectedBlock);
				}
			}
		}

		if (m_animationStart && m_currentBlock) {
			u32 frame = timeToBreak == 0 ? 9 : 0;
			if (timeToBreak > 0 && std::isfinite(timeToBreak))
				frame = u32(std::clamp((float(ticks) - float(m_animationStart)) / (timeToBreak * 100.f), 0.f, 9.f));
			updateAnimationVertexBuffer(*m_currentBlock, orientation, frame);
		}
	}

	if (m_selectedBlock.w != -1)
		updateVertexBuffer(*m_currentBlock, orientation);
	else
		m_currentBlock = nullptr;

	// Repeat block activation when right click is held
	u64 now = GameClock::getInstance().getTicks();
	u64 last = m_lastActivationTime.value_or(now);
	if (now - last > m_activationRepeatDelay && !hotbar.currentItem().hasGroup("group:om_treasure_note"))
		activateBlock(hotbar);
}

void BlockCursor::activateBlock(const Hotbar &hotbar) {
	if (m_animationStart != 0)
		m_animationStart = 0;

	m_lastActivationTime = GameClock::getInstance().getTicks();

	u16 blockId = m_world.getBlock(m_selectedBlock.x, m_selectedBlock.y, m_selectedBlock.z);
	const Block &block = Registry::getInstance().getBlock(blockId);
	const Item &item = hotbar.currentItem();

	if (item.hasGroup("group:om_map") && block.canBeActivated() && !GamePad::isKeyPressed(GameKey::Sneak)) {
		m_client.sendBlockActivated(m_selectedBlock);
		return;
	}
	bool itemActivationSent = false;
	bool sneakedItemActivation = false;
	if (item.id() && item.canBeActivated()) {
		if (!GamePad::isKeyPressed(GameKey::Sneak)) {
			m_client.sendItemActivated(m_selectedBlock);
			itemActivationSent = true;
		}
		else
			sneakedItemActivation = true;
	}

	bool blockActivationSent = false;
	if (!itemActivationSent && block.id() && block.canBeActivated()
	&& (!GamePad::isKeyPressed(GameKey::Sneak) || sneakedItemActivation)) {
		m_client.sendBlockActivated(m_selectedBlock);
		blockActivationSent = true;
	}

	if (block.id() && !itemActivationSent && !blockActivationSent && hotbar.currentItem().id() && item.isBlock()) {
		s8 face = (s8)m_selectedBlock.w;

		s32 x = m_selectedBlock.x;
		s32 y = m_selectedBlock.y;
		s32 z = m_selectedBlock.z;

		const BlockState *blockState = m_world.getBlockState(x, y, z);

		if (blockState->drawType() != BlockDrawType::XShape) {
			// FIXME: Document where these face numbers come from
			if (face == 0) ++x;
			if (face == 3) --x;
			if (face == 1) ++y;
			if (face == 4) --y;
			if (face == 2) ++z;
			if (face == 5) --z;

			blockState = m_world.getBlockState(x, y, z);
		}

		const Block &newBlock = Registry::getInstance().getBlockFromStringID(hotbar.currentItem().stringID());
		Vector3i destination{x, y, z};
		bool merged = resolveSlabPlacement(m_world, destination, newBlock);
		x = destination.x; y = destination.y; z = destination.z;
		blockState = m_world.getBlockState(x, y, z);
		if (blockState && (merged || !blockState->block().id() || blockState->drawType() == BlockDrawType::Liquid || blockState->drawType() == BlockDrawType::XShape)) {
			int mount = -1;
			if (newBlock.hasGroup("group:om_torch") || newBlock.hasGroup("group:om_wall_switch")) {
				mount = torchRotationForFace(face);
				if (mount < 0) return;
			}
			u16 decorationState = 0;
			if (newBlock.hasGroup("group:om_sign")) {
				if (face < 0 || face > 4) return;
				decorationState = face == 2 ? 0 : 1;
				static constexpr int rotations[] = {3, 0, 0, 1, 2, -1};
				if (face != 2) mount = rotations[face];
			}
			if (newBlock.hasGroup("group:om_painting")) {
				if (face < 0 || face > 5) return;
				static constexpr int rotations[] = {16, 8, -1, 20, 12, -1};
				mount = rotations[face];
				if (mount < 0) return;
				decorationState = u16((x * 31u + y * 17u + z * 13u) % newBlock.states().size());
			}
			const BlockState &newBlockState = newBlock.getState(merged ? 1 : decorationState);
			u16 data = merged ? newBlock.param().setParam(BlockParam::State, 0, 1) : 0;
			if (newBlock.isRotatable()) {
				u16 rotation = mount >= 0 ? u16(mount) : m_player.getOppositeDirection() & 0x3;
				if (newBlock.hasGroup("group:om_stairs")) rotation = m_player.getDirection();
				if (newBlock.hasGroup("group:om_piston")) rotation = retro::pistonPlacementRotation(m_player,{x,y,z});
				data = newBlock.param().setParam(BlockParam::Rotation, 0, rotation);
			}
			if (newBlock.hasGroup("group:om_sign") || newBlock.hasGroup("group:om_painting"))
				data = newBlock.param().setParam(BlockParam::State, data, decorationState);
			if (!worldBounds::contains(z) || !blockHasSupport(m_world, {x, y, z}, newBlockState, data)) return;
			DoubleBox playerBox = m_player.hitbox();
			playerBox.move(m_player.x(), m_player.y(), m_player.z());
			if (!newBlockState.isCollidable() || !collision::blockBox(newBlockState, data, x, y, z).intersects(playerBox)) {
				u32 block = u32(newBlock.id()) | (u32(data) << 16);
				if (merged) m_world.setBlockState(x, y, z, 1);
				else {
					m_world.setData(x, y, z, data);
					m_world.setBlock(x, y, z, newBlock.id());
				}
				if (auto *sound = SoundManager::instance())
					sound->playBlock(newBlock.getState(0), BlockSound::Place, {x + 0.5, y + 0.5, z + 0.5});

				m_client.sendPlayerPlaceBlock(x, y, z, block);

			}
		}
	}
}

using namespace BlockGeometry;

void BlockCursor::updateVBOCoords(Vertex vertices[nFaces][nVertsPerFace], const BlockState &blockState,
	float face, u8f orientation)
{
	float epsilon = 0.001f; // Prevent Z-fighting
	FloatBox box = blockState.boundingBox();
	if (blockState.drawType() == BlockDrawType::Torch) { box = torchSelectionBox(orientation); orientation = 0; }
	glm::vec3 bottomLeft{box.x - epsilon, box.y - epsilon, box.z - epsilon};
	glm::vec3 topRight{box.sizeX + epsilon * 2.f, box.sizeY + epsilon * 2.f, box.sizeZ + epsilon * 2.f};
	topRight += bottomLeft;

	const glm::mat3 &orientMatrix = orientMatrices[orientation];

	glm::vec3 vertexPos[nVertsPerCube]{
		// Order is important. It matches the bit order defined in BlockGeometry::cubeVerts.
		{bottomLeft.x, bottomLeft.y, bottomLeft.z},
		{topRight.x,   bottomLeft.y, bottomLeft.z},
		{bottomLeft.x, topRight.y,   bottomLeft.z},
		{topRight.x,   topRight.y,   bottomLeft.z},
		{bottomLeft.x, bottomLeft.y, topRight.z},
		{topRight.x,   bottomLeft.y, topRight.z},
		{bottomLeft.x, topRight.y,   topRight.z},
		{topRight.x,   topRight.y,   topRight.z},
	};

	if (orientation != 0) {
		static const glm::vec3 half{0.5, 0.5, 0.5};
		// Rotate each vertex coordinate around the centre of the cube
		for (int i = 0; i < nVertsPerCube; ++i) {
			vertexPos[i] = orientMatrix * (vertexPos[i] - half) + half;
		}
	}

	for (u8f f = 0 ; f < nFaces; ++f) {
		for (u8f v = 0; v < nVertsPerFace; ++v) {
			vertices[f][v].coord3d[0] = vertexPos[cubeVerts[f][v]].x;
			vertices[f][v].coord3d[1] = vertexPos[cubeVerts[f][v]].y;
			vertices[f][v].coord3d[2] = vertexPos[cubeVerts[f][v]].z;
			vertices[f][v].coord3d[3] = face;
		}
	}
}

void BlockCursor::updateVertexBuffer(const BlockState &blockState, u8f orientation) {
	Vertex vertices[nFaces][nVertsPerFace];
	updateVBOCoords(vertices, blockState, -1, orientation);

	m_vbo.init(vertices, sizeof(vertices), true);
}

void BlockCursor::updateAnimationVertexBuffer(const BlockState &blockState, u8f orientation, int animationPos) {
	Vertex vertices[nFaces][nVertsPerFace];
	updateVBOCoords(vertices, blockState, -2, orientation);

	float color[4] = {1, 1, 1, 0.5};
	for (u8f f = 0; f < nFaces; ++f)
		for (u8f v = 0; v < nVertsPerFace; ++v)
			memcpy(&vertices[f][v].color, &color[0], 4 * sizeof(float));

	if (animationPos != -1) {
		glm::vec4 blockTexCoords{0.1f * (float)animationPos, 0.0f, 0.1f + 0.1f * (float)animationPos, 1.0f};
		float faceTexCoords[nVertsPerFace][nCoordsPerUV] = {
			{blockTexCoords.x, blockTexCoords.w},
			{blockTexCoords.z, blockTexCoords.w},
			{blockTexCoords.z, blockTexCoords.y},
			{blockTexCoords.x, blockTexCoords.y},
		};

		for (u8f f = 0 ; f < nFaces ; ++f) {
			for(u8f v = 0 ; v < nVertsPerFace ; v++) {
				vertices[f][v].texCoord[0] = faceTexCoords[v][0];
				vertices[f][v].texCoord[1] = faceTexCoords[v][1];
			}
		}
	}

	m_animationVBO.init(vertices, sizeof(vertices), true);
}

void BlockCursor::draw(RenderTarget &target, RenderStates states) const {
	if (m_selectedBlock.w == -1) return;

	// Subtract the camera position - see comment in ClientWorld::draw()
	const Vector3d &cameraPosition = m_player.camera().getDPosition();
	states.transform.translate(
		float(m_selectedBlock.x - cameraPosition.x),
		float(m_selectedBlock.y - cameraPosition.y),
		float(m_selectedBlock.z - cameraPosition.z)
	);

	states.primitiveType = BGFX_STATE_PT_LINES;

	target.drawElements(m_vbo, m_ibo, 0, 0, states);

	if (m_animationStart > 0) {
		// refi cracks use transparent pixels, so the overlay must preserve their alpha.
		states.blendFuncColorSrc = BGFX_STATE_BLEND_SRC_ALPHA;
		states.blendFuncColorDst = BGFX_STATE_BLEND_INV_SRC_ALPHA;

		states.texture = m_blockDestroyTexture;

		states.primitiveType = 0; // Defaults to triangles

		target.drawElements(m_animationVBO, target.cubeIndexBuffer(), 0, 0, states);
	}
}

glm::ivec4 BlockCursor::findSelectedBlock() const {
	bool includeSources = m_player.heldItemSlot() >= 0 && m_player.heldItemSlot() < m_player.inventory().width()
		&& m_player.heldItemStack().item().getGroupValue("group:om_bucket") == 1;
	glm::dvec3 position{m_player.camera().getDPosition().x,
	                    m_player.camera().getDPosition().y,
	                    m_player.camera().getDPosition().z};

	s32f bestX = s32f(floor(position.x));
	s32f bestY = s32f(floor(position.y));
	s32f bestZ = s32f(floor(position.z));

	// Deal with a degenerate case: camera in the middle of a block
	const BlockState *blockState = m_world.getBlockState((int)bestX, (int)bestY, (int)bestZ);
	if (blockState && blockState->block().id() && (blockState->drawType() != BlockDrawType::Liquid || (includeSources && (blockState->block().stringID() == "default:water" || blockState->block().stringID() == "default:lava")))) {
		// We're inside a node, therefore there's no face, but we still need
		// to return a valid block. We use face 6 for that. For rightclicks,
		// it should attempt to place the block on the same node clicked, and
		// fail because it's already occupied. For digs, it should just dig
		// the node.
		return glm::ivec4{bestX, bestY, bestZ, 6};
	}

	// Still the degenerate case, but this time for the sub-case where
	// the coordinates of the player are exact integers, thus lying exactly
	// between two faces (or four, if it's in a corner). Deal with that case
	// here. NOTE: We don't deal with height because it's unlikely that the
	// camera ends up being at an integer position.
	if (double(bestX) == position.x && double(bestY) == position.y) {
		for (int y = -1; y <= 0; y++) {
			for (int x = -1; x <= 0; x++) {
				const BlockState *blockState = m_world.getBlockState((int)bestX + x, (int)bestY + y, (int)bestZ);
				if (blockState && blockState->block().id() && (blockState->drawType() != BlockDrawType::Liquid || (includeSources && (blockState->block().stringID() == "default:water" || blockState->block().stringID() == "default:lava")))) {
					return glm::ivec4{bestX + x, bestY + y, bestZ, 6};
				}
			}
		}
	}

	glm::dvec3 lookAt{m_player.dirTargetedX(), m_player.dirTargetedY(), m_player.dirTargetedZ()};

	// Ray casting algorithm to find out which block we are looking at
	const double maxReach = 4.0;
	double bestDepth;
	s8f bestFace = -1;

	glm::dvec3 lookAtN = glm::normalize(lookAt);

	BlockCursorRaycast::rayCastToAxis(AXIS_X, position, lookAtN, maxReach, bestX, bestY, bestZ, bestFace, bestDepth, m_world, includeSources);
	BlockCursorRaycast::rayCastToAxis(AXIS_Y, position, lookAtN, maxReach, bestX, bestY, bestZ, bestFace, bestDepth, m_world, includeSources);
	BlockCursorRaycast::rayCastToAxis(AXIS_Z, position, lookAtN, maxReach, bestX, bestY, bestZ, bestFace, bestDepth, m_world, includeSources);

	return glm::ivec4{bestX, bestY, bestZ, bestFace};
}
