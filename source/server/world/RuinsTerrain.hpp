#ifndef RUINSTERRAIN_HPP_
#define RUINSTERRAIN_HPP_
#include <optional>
#include "common/core/Vector3.hpp"
class ServerChunk;
class ServerWorld;
class Heightmap;
void decorateRuinsTerrain(ServerChunk &chunk,Heightmap &heightmap,s32 seed);
std::optional<Vector3i> nearestUndergroundRuin(ServerWorld &world,const Vector3i &point);
void decorateCacheTerrain(ServerChunk &chunk, Heightmap &heightmap, s32 seed);
std::optional<Vector3i> nearestTreasureCache(ServerWorld &world, const Vector3i &point);
#endif
