#ifndef ROPEINTERACTION_HPP_
#define ROPEINTERACTION_HPP_
#include "common/core/Vector3.hpp"
class ServerWorld;
class Player;
bool useClimbingRope(ServerWorld &world, Player &player, Vector3i position, s8 face);
#endif
