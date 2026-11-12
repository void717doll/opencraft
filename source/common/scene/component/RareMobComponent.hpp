#ifndef RAREMOBCOMPONENT_HPP_
#define RAREMOBCOMPONENT_HPP_

#include <string>
#include "common/network/Network.hpp"

struct RareMobComponent {
	std::string name;
	bool announced = false;
};

inline sf::Packet &operator<<(sf::Packet &packet, const RareMobComponent &rare) { return packet << rare.name; }
inline sf::Packet &operator>>(sf::Packet &packet, RareMobComponent &rare) {
	rare.announced = false;
	return packet >> rare.name;
}
#endif
