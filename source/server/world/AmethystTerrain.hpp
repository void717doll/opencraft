#ifndef AMETHYSTTERRAIN_HPP_
#define AMETHYSTTERRAIN_HPP_

#include "common/core/IntTypes.hpp"

class Heightmap;
class ServerChunk;

void decorateAmethyst(ServerChunk &chunk, Heightmap &heightmap, s32 seed);

#endif
