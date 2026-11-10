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
#include "common/core/BlockGeometry.hpp"
#include "common/world/LiquidState.hpp"
#include "common/scene/MinecloniaLeverMesh.hpp"
#include "common/scene/MinecloniaTorchMesh.hpp"
#include "common/world/FenceGeometry.hpp"
#include "common/world/StairGeometry.hpp"

#include "client/core/Config.hpp"
#include "client/world/mesh/BlockMesher.hpp"
#include "client/world/mesh/LightUtils.hpp"

using namespace BlockGeometry;
using namespace LightUtils;

namespace {
void addTorch(s8f x, s8f y, s8f z, ChunkMeshBuildingJob &job, const BlockState &state, u16 data) {
	const auto &uv = job.textureAtlas->getTexCoords(state.tiles().getTextureForFace(0));
	u16 rotation = state.block().param().getParam(BlockParam::Rotation, data);
	const auto &mesh = rotation ? mineclonia::torch_wall : mineclonia::torch_floor;
	for (const auto &point : mesh) {
		float px = point.x - 0.5f, py = point.y - 0.5f;
		if (rotation == 1) { float previous = px; px = py; py = -previous; }
		if (rotation == 3) { float previous = px; px = -py; py = previous; }
		if (rotation == 4) { px = -px; py = -py; }
		Vertex vertex{};
		vertex.coord3d[0] = x + px + 0.5f;
		vertex.coord3d[1] = y + py + 0.5f;
		vertex.coord3d[2] = z + point.z;
		vertex.coord3d[3] = 6;
		vertex.normal[2] = 1;
		for (float &color : vertex.color) color = 1;
		vertex.texCoord[0] = uv.x + point.u * uv.sizeX;
		vertex.texCoord[1] = uv.y + point.v * uv.sizeY;
		vertex.lightValue[0] = job.chunkData.getSunlight(x, y, z);
		vertex.lightValue[1] = job.chunkData.getTorchlight(x, y, z);
		vertex.ambientOcclusion = 4;
		job.vertices[ChunkMeshLayer::Flora].push_back(vertex);
	}
	job.totalVertexCount += mesh.size();
}

void addLever(s8f x,s8f y,s8f z,ChunkMeshBuildingJob &job,const BlockState &state) {
	const auto &mesh=state.id()?mineclonia::lever_on:mineclonia::lever_off;
	for (const auto &point:mesh) {
		const auto &uv=job.textureAtlas->getTexCoords(state.tiles().getTextureForFace(point.handle?5:0));
		Vertex vertex{};
		vertex.coord3d[0]=x+point.x; vertex.coord3d[1]=y+point.y; vertex.coord3d[2]=z+point.z;
		vertex.coord3d[3]=6; vertex.normal[2]=1;
		for(float &color:vertex.color) color=1;
		vertex.texCoord[0]=uv.x+point.u*uv.sizeX; vertex.texCoord[1]=uv.y+point.v*uv.sizeY;
		vertex.lightValue[0]=job.chunkData.getSunlight(x,y,z);
		vertex.lightValue[1]=job.chunkData.getTorchlight(x,y,z);
		vertex.ambientOcclusion=4;
		job.vertices[ChunkMeshLayer::Flora].push_back(vertex);
	}
	job.totalVertexCount+=mesh.size();
}
void addRail(s8f x, s8f y, s8f z, ChunkMeshBuildingJob &job, const BlockState &state) {
	const FloatRect &uv = job.textureAtlas->getTexCoords(state.tiles().getTextureForFace(0));
	unsigned shape = state.block().getGroupValue("group:om_rail_type") ? state.id() & 7 : state.id();
	unsigned turns = shape == 1 || shape == 2 || shape == 3 ? 1 : shape >= 6 ? shape - 6 : 0;
	Vertex vertices[4]{};
	constexpr float corners[4][2] = {{0,0},{1,0},{1,1},{0,1}};
	for (unsigned i=0; i<4; ++i) {
		float px=corners[i][0], py=corners[i][1], height=1.f/16;
		if (shape == 2) height += px;
		if (shape == 3) height += 1-px;
		if (shape == 4) height += 1-py;
		if (shape == 5) height += py;
		auto &vertex=vertices[i];
		vertex.coord3d[0]=x+px; vertex.coord3d[1]=y+py; vertex.coord3d[2]=z+height;
		vertex.coord3d[3]=4;
		vertex.normal[2]=1;
		for (float &color:vertex.color) color=1;
		for (unsigned turn=0; turn<turns; ++turn) { float previous=px; px=py; py=1-previous; }
		vertex.texCoord[0]=uv.x+px*uv.sizeX;
		vertex.texCoord[1]=uv.y+py*uv.sizeY;
		vertex.lightValue[0]=job.chunkData.getSunlight(x,y,z);
		vertex.lightValue[1]=job.chunkData.getTorchlight(x,y,z);
		vertex.ambientOcclusion=4;
	}
	for (unsigned i:{0,1,2,0,2,3}) job.vertices[ChunkMeshLayer::Flora].push_back(vertices[i]);
	job.totalVertexCount += 6;
}
}


void BlockMesher::addBlock(s8f x, s8f y, s8f z, ChunkMeshBuildingJob &job,
                           const BlockState &blockState, u16 blockParam)
{
	if (blockState.block().hasGroup("group:om_sign") && blockState.id() == 0) {
		u16 rotation = blockState.block().param().getParam(BlockParam::Rotation, blockParam) & 3;
		BlockState post = blockState;
		FloatBox box{0.4375f, 0.4375f, 0, 0.125f, 0.125f, 0.5f};
		post.boundingBox(box);
		addBlock(x, y, z, job, post, box, rotation, orientMatrices[rotation]);
	}
	if (blockState.block().hasGroup("group:om_piston_head")) {
		u16 rotation = blockState.block().param().getParam(BlockParam::Rotation, blockParam) & 31;
		BlockState shaft = blockState;
		FloatBox box{0.375f,0.375f,0,0.25f,0.25f,0.75f};
		shaft.boundingBox(box);
		addBlock(x,y,z,job,shaft,box,rotation,orientMatrices[rotation]);
	}
	if (blockState.block().hasGroup("group:om_stairs")) {
		u16 rotation = blockState.block().param().getParam(BlockParam::Rotation, blockParam);
		BlockState part = blockState;
		for (const auto &box : retro::stairBoxes(rotation)) {
			part.boundingBox(box);
			addBlock(x, y, z, job, part, box, 0, orientMatrices[0]);
		}
		return;
	}
	if (blockState.block().hasGroup("group:fence")) {
		u16 id = blockState.block().id();
		auto boxes = retro::fenceBoxes(job.chunkData.getBlockID(x - 1, y, z) == id,
			job.chunkData.getBlockID(x + 1, y, z) == id, job.chunkData.getBlockID(x, y - 1, z) == id,
			job.chunkData.getBlockID(x, y + 1, z) == id);
		BlockState part = blockState;
		for (const auto &box : boxes) {
			part.boundingBox(box);
			addBlock(x, y, z, job, part, box, 0, orientMatrices[0]);
		}
		return;
	}
	if (blockState.drawType() == BlockDrawType::Torch) { addTorch(x, y, z, job, blockState, blockParam); return; }
	if (blockState.drawType() == BlockDrawType::Lever) { addLever(x,y,z,job,blockState); return; }
	if (blockState.drawType() == BlockDrawType::Rail) { addRail(x,y,z,job,blockState); return; }
	const FloatBox &boundingBox = blockState.boundingBox();

	u8f orientation = blockState.block().isRotatable()
		? (u8f)blockState.block().param().getParam(BlockParam::Rotation, blockParam) : 0;
	const glm::mat3 &orientMatrix = orientMatrices[orientation];

	addBlock(x, y, z, job, blockState, boundingBox, orientation, orientMatrix);
}

void BlockMesher::addBlock(s8f x, s8f y, s8f z, ChunkMeshBuildingJob &job,
                           const BlockState &blockState, const FloatBox &boundingBox,
                           u8f orientation, const glm::mat3 &orientMatrix)
{
	glm::vec3 vertexPos[nVertsPerCube]{
		// Order is important. It matches the bit order defined in BlockGeometry::cubeVerts.
		{boundingBox.x,                     boundingBox.y,                     boundingBox.z},
		{boundingBox.x + boundingBox.sizeX, boundingBox.y,                     boundingBox.z},
		{boundingBox.x,                     boundingBox.y + boundingBox.sizeY, boundingBox.z},
		{boundingBox.x + boundingBox.sizeX, boundingBox.y + boundingBox.sizeY, boundingBox.z},
		{boundingBox.x,                     boundingBox.y,                     boundingBox.z + boundingBox.sizeZ},
		{boundingBox.x + boundingBox.sizeX, boundingBox.y,                     boundingBox.z + boundingBox.sizeZ},
		{boundingBox.x,                     boundingBox.y + boundingBox.sizeY, boundingBox.z + boundingBox.sizeZ},
		{boundingBox.x + boundingBox.sizeX, boundingBox.y + boundingBox.sizeY, boundingBox.z + boundingBox.sizeZ},
	};

	if (blockState.drawType() == BlockDrawType::Cactus) {
		// Ignore bounding box, initialize it to full node coordinates
		for (u8f i = 0; i < nVertsPerCube; ++i) {
			vertexPos[i].x = float((i >> 0) & 1);
			vertexPos[i].y = float((i >> 1) & 1);
			vertexPos[i].z = float((i >> 2) & 1);
		}
	}

	// vNeighbour is used to find neighbouring cubes per vertex.
	// Same binary layout.
	glm::vec3 vNeighbour[nVertsPerCube] = {
		{-1,-1,-1}, { 1,-1,-1}, {-1, 1,-1}, { 1, 1,-1}, {-1,-1, 1}, { 1,-1, 1}, {-1, 1, 1}, {1, 1, 1},
	};

	if (orientation) { // don't work extra if it's not oriented differently
		static const glm::vec3 half{0.5, 0.5, 0.5};
		// Rotate each vertex coordinate around the centre of the
		// cube, and each vertex neighbour around the origin
		for (int i = 0; i < nVertsPerCube; ++i) {
			vertexPos[i] = orientMatrix * (vertexPos[i] - half) + half;
			vNeighbour[i] = orientMatrix * vNeighbour[i];
		}
	}

	for (s8f f = 0; f < nFaces ; ++f) {
		// Construct the normal vector to a face
		const glm::vec3 glmNormal = orientMatrix * faceNormals[f];
		const Vector3<s8f> normal{s8f(glmNormal.x), s8f(glmNormal.y), s8f(glmNormal.z)};

		// Construct an array with the 4 vertex positions of this face
		glm::vec3 *faceVerts[nVertsPerFace]{
			&vertexPos[cubeVerts[f][0]], &vertexPos[cubeVerts[f][1]],
			&vertexPos[cubeVerts[f][2]], &vertexPos[cubeVerts[f][3]]
		};

		// Construct an array with the 4 vertex neighbours of this face
		// (as integer vectors)
		const Vector3<s8f> corner0{s8f(vNeighbour[cubeVerts[f][0]].x), s8f(vNeighbour[cubeVerts[f][0]].y), s8f(vNeighbour[cubeVerts[f][0]].z)};
		const Vector3<s8f> corner1{s8f(vNeighbour[cubeVerts[f][1]].x), s8f(vNeighbour[cubeVerts[f][1]].y), s8f(vNeighbour[cubeVerts[f][1]].z)};
		const Vector3<s8f> corner2{s8f(vNeighbour[cubeVerts[f][2]].x), s8f(vNeighbour[cubeVerts[f][2]].y), s8f(vNeighbour[cubeVerts[f][2]].z)};
		const Vector3<s8f> corner3{s8f(vNeighbour[cubeVerts[f][3]].x), s8f(vNeighbour[cubeVerts[f][3]].y), s8f(vNeighbour[cubeVerts[f][3]].z)};

		const Vector3<s8f> *vFaceNeighbours[nVertsPerFace]{&corner0, &corner1, &corner2, &corner3};

		addBlockFace(x, y, z, f, job, blockState, normal, faceVerts, vFaceNeighbours);
	}
}

void BlockMesher::addBlockFace(s8f x, s8f y, s8f z, s8f f, ChunkMeshBuildingJob &job,
                              const BlockState &blockState,
                              const Vector3<s8f> &normal,
                              const glm::vec3 *const vertexPos[nVertsPerFace],
                              const Vector3<s8f> *const neighbourOfs[nVertsPerFace])
{
	// Get surrounding block for the face
	s8f sx = x + normal.x;
	s8f sy = y + normal.y;
	s8f sz = z + normal.z;

	const BlockState *surroundingBlockState = job.chunkData.getBlockState(sx, sy, sz);

	bool sameLiquid = surroundingBlockState && liquid::kind(blockState.block())
		&& liquid::kind(blockState.block()) == liquid::kind(surroundingBlockState->block());
	if (sameLiquid) {
		float ownHeight = liquid::height(blockState.block(), job.chunkData.getBlockParam(x, y, z));
		float otherHeight = liquid::height(surroundingBlockState->block(), job.chunkData.getBlockParam(sx, sy, sz));
		if (normal.z != 0 || otherHeight >= ownHeight) return;
	}
	// Skip hidden faces
	if (surroundingBlockState && surroundingBlockState->block().id()
	&& ((blockState.drawType() == BlockDrawType::Solid && surroundingBlockState->drawType() == BlockDrawType::Solid && surroundingBlockState->isOpaque())
	 || (blockState.block().id() == surroundingBlockState->block().id() && blockState.drawType() == BlockDrawType::Glass)
	 || (blockState.drawType() == BlockDrawType::Liquid && surroundingBlockState->drawType() == BlockDrawType::Solid)
	 || (blockState.drawType() == BlockDrawType::Cactus && surroundingBlockState->block().id() == blockState.block().id() && f > 3)))
		return;

	const FloatBox &boundingBox = blockState.boundingBox();

	const std::string *texture = &blockState.tiles().getTextureForFace(f);
	int chestPartner = 0;
	const auto &tiles = blockState.tiles().textureFilenames();
	if (blockState.block().hasGroup("group:om_chest") && tiles.size() >= 10) {
		u16 rotation = blockState.block().param().getParam(BlockParam::Rotation, job.chunkData.getBlockParam(x, y, z));
		const glm::vec3 axis = orientMatrices[rotation & 31] * glm::vec3{0, 1, 0};
		for (int side : {-1, 1}) {
			s8f nx = x + s8f(axis.x) * side, ny = y + s8f(axis.y) * side;
			const auto *neighbor = job.chunkData.getBlockState(nx, ny, z);
			if (neighbor && neighbor->block().id() == blockState.block().id()
			 && neighbor->block().param().getParam(BlockParam::Rotation, job.chunkData.getBlockParam(nx, ny, z)) == rotation) {
				if (chestPartner) { chestPartner = 0; break; }
				chestPartner = side;
			}
		}
		if (chestPartner) {
			if (f == BlockFace::East) texture = &tiles[chestPartner > 0 ? 6 : 7];
			else if (f == BlockFace::West) texture = &tiles[8];
			else if (f == BlockFace::Top || f == BlockFace::Bottom) texture = &tiles[9];
		}
	}
	const FloatRect &blockTexCoords = job.textureAtlas->getTexCoords(*texture);

	// Calculate UV's
	// These are tough to obtain. Note that texture Y grows in the up-down direction, and so does V.
	// Vertex index in the bitmap array and U/V correspondence is:
	//    U0V0 -> 3 2 <- U1V0
	//    U0V1 -> 0 1 <- U1V1
	float U0, V0, U1, V1;
	if (blockState.drawType() == BlockDrawType::Campfire) {
		// the source log sheet has a 16x4 bark strip and a 4x4 cut end.
		U0 = 0.f;
		U1 = f < 2 ? 0.25f : 1.f;
		V0 = f < 2 ? 0.25f : 0.f;
		V1 = f < 2 ? 0.5f : 0.25f;
	}
	else if (blockState.drawType() == BlockDrawType::Cactus) {
		U0 = 0.f;
		V0 = 0.f;
		U1 = 1.f;
		V1 = 1.f;
	}
	else {
		U0 = (f == 0) ? 1.f - (boundingBox.y + boundingBox.sizeY) : (f == 1) ? boundingBox.y :
		     (f == 3) ? 1.f - (boundingBox.x + boundingBox.sizeX) : boundingBox.x;
		V0 = (f <= 3) ? 1.f - (boundingBox.z + boundingBox.sizeZ) : (f == 4) ? boundingBox.y : 1.f - (boundingBox.y + boundingBox.sizeY);
		U1 = (f == 0) ? 1.f - boundingBox.y : (f == 1) ? boundingBox.y + boundingBox.sizeY :
		     (f == 3) ? 1.f - boundingBox.x : boundingBox.x + boundingBox.sizeX;
		V1 = (f <= 3) ? 1.f - boundingBox.z : (f == 4) ? boundingBox.y + boundingBox.sizeY : 1.f - boundingBox.y;
	}

	// Prepare vertex information for VBO
	Vertex vertices[nVertsPerFace];
	for (s8f v = 0; v < nVertsPerFace; ++v) {
		if (blockState.drawType() == BlockDrawType::Cactus) {
			vertices[v].coord3d[0] = x + vertexPos[v]->x - boundingBox.x * (float)normal.x;
			vertices[v].coord3d[1] = y + vertexPos[v]->y - boundingBox.y * (float)normal.y;
			vertices[v].coord3d[2] = z + vertexPos[v]->z - boundingBox.z * (float)normal.z;
		}
		else {
			float blockHeight = vertexPos[v]->z;
			if (blockState.drawType() == BlockDrawType::Liquid) {
				const BlockState *topBlockState = job.chunkData.getBlockState(x, y, z + 1);
				bool covered = topBlockState && liquid::kind(blockState.block())
					&& liquid::kind(topBlockState->block()) == liquid::kind(blockState.block());
				if (f != BlockFace::Bottom)
					blockHeight *= liquid::height(blockState.block(), job.chunkData.getBlockParam(x, y, z), covered);
			}

			vertices[v].coord3d[0] = x + vertexPos[v]->x;
			vertices[v].coord3d[1] = y + vertexPos[v]->y;
			vertices[v].coord3d[2] = z + blockHeight;
		}

		vertices[v].coord3d[0] += blockState.drawOffset().x;
		vertices[v].coord3d[1] += blockState.drawOffset().y;
		vertices[v].coord3d[2] += blockState.drawOffset().z;

		vertices[v].coord3d[3] = f;

		vertices[v].normal[0] = (float)normal.x;
		vertices[v].normal[1] = (float)normal.y;
		vertices[v].normal[2] = (float)normal.z;

		const Color colorMultiplier = blockState.colorMultiplier();
		vertices[v].color[0] = colorMultiplier.r;
		vertices[v].color[1] = colorMultiplier.g;
		vertices[v].color[2] = colorMultiplier.b;
		vertices[v].color[3] = colorMultiplier.a;

		float U = (v == 0 || v == 3) ? U0 : U1;
		float V = (v >= 2) ? V0 : V1;
		if (chestPartner) {
			if (f == BlockFace::Top || f == BlockFace::Bottom) {
				float oldU = U;
				U = f == BlockFace::Top ? 1.f - V : V;
				V = f == BlockFace::Top ? oldU : 1.f - oldU;
				if (chestPartner < 0) U = 1.f - U;
			}
			else if (f == BlockFace::West && chestPartner < 0) U = 1.f - U;
		}
		vertices[v].texCoord[0] = math::qlerpf(blockTexCoords.x, blockTexCoords.x + blockTexCoords.sizeX, U);
		vertices[v].texCoord[1] = math::qlerpf(blockTexCoords.y, blockTexCoords.y + blockTexCoords.sizeY, V);

		if (Config::isSmoothLightingEnabled)
			vertices[v].lightValue[0] = getLightForVertex(LightType::Sun, x, y, z, *neighbourOfs[v], normal, job.chunkData);
		else if (blockState.isOpaque())
			vertices[v].lightValue[0] = job.chunkData.getSunlight(sx, sy, sz);
		else
			vertices[v].lightValue[0] = job.chunkData.getSunlight(x, y, z);

		if (Config::isSmoothLightingEnabled && !blockState.isLightSource())
			vertices[v].lightValue[1] = getLightForVertex(LightType::Torch, x, y, z, *neighbourOfs[v], normal, job.chunkData);
		else if (blockState.isOpaque())
			vertices[v].lightValue[1] = job.chunkData.getTorchlight(sx, sy, sz);
		else
			vertices[v].lightValue[1] = job.chunkData.getTorchlight(x, y, z);

		vertices[v].ambientOcclusion = getAmbientOcclusion(x, y, z, *neighbourOfs[v], normal, job.chunkData);
	}

	auto addVertex = [&](u8 v) {
		if (Config::ambientOcclusion != 1 || blockState.isLightSource())
			vertices[v].ambientOcclusion = 4;

		if (blockState.drawType() == BlockDrawType::Liquid)
			job.vertices[ChunkMeshLayer::Liquid].emplace_back(vertices[v]);
		else if (blockState.drawType() == BlockDrawType::Glass)
			job.vertices[ChunkMeshLayer::Glass].emplace_back(vertices[v]);
		else if (blockState.colorMultiplier() != Color::White)
			job.vertices[ChunkMeshLayer::NoMipMap].emplace_back(vertices[v]);
		else
			job.vertices[ChunkMeshLayer::Solid].emplace_back(vertices[v]);
	};

	// Flipping quad to fix anisotropy issue
	if (vertices[0].ambientOcclusion + vertices[2].ambientOcclusion >
			vertices[1].ambientOcclusion + vertices[3].ambientOcclusion) {
		addVertex(0);
		addVertex(1);
		addVertex(2);
		addVertex(2);
		addVertex(3);
		addVertex(0);
	} else {
		addVertex(0);
		addVertex(1);
		addVertex(3);
		addVertex(3);
		addVertex(1);
		addVertex(2);
	}

	job.totalVertexCount += 6;
}
