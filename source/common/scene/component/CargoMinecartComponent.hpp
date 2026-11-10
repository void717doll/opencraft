#ifndef CARGOMINECARTCOMPONENT_HPP_
#define CARGOMINECARTCOMPONENT_HPP_

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "common/inventory/Inventory.hpp"
#include "common/network/Network.hpp"
#include "common/network/NetworkUtils.hpp"

// a separate saved component keeps existing passenger cart packets readable.
struct CargoMinecartComponent : public ISerializable {
	u8 kind = 1;
	u32 fuel = 0;
	u8 burnPhase = 0;
	Vector3d push{0, 0, 0};
	Inventory inventory{9, 3, "minecart"};
	std::string viewer;
	bool isUpdated = false;
	Network::Command packetType{};
	bool addCoal() {
		if (kind != 2 || fuel > 70800) return false;
		fuel += 1200;
		return true;
	}
	void drive(Vector3d &velocity) {
		double length = std::hypot(push.x, push.y);
		if (kind != 2 || !fuel || length <= 0.01) {
			velocity *= 0.9;
			return;
		}
		push *= 1 / length;
		velocity.x = velocity.x * 0.8 + push.x * 0.04;
		velocity.y = velocity.y * 0.8 + push.y * 0.04;
		// beta burns fuel on one in four powered updates; use a saved phase for stable replays.
		burnPhase = u8((burnPhase + 1) % 4);
		if (!burnPhase) --fuel;
	}
	void followTrack(const Vector3d &velocity) {
		if (kind != 2 || std::hypot(push.x, push.y) <= 0.01 || velocity.x * velocity.x + velocity.y * velocity.y <= 0.001) return;
		if (push.x * velocity.x + push.y * velocity.y < 0) push = {0, 0, 0};
		else push = {velocity.x, velocity.y, 0};
	}
	void serialize(sf::Packet &packet) const override { packet << kind << fuel << burnPhase << push << inventory; }
	void deserialize(sf::Packet &packet) override {
		packet >> kind >> fuel >> burnPhase >> push >> inventory;
		if (!packet || (kind != 1 && kind != 2) || inventory.width() != 9 || inventory.height() != 3)
			throw std::runtime_error("invalid cargo minecart data");
		fuel = std::min<u32>(fuel, 72000);
		burnPhase %= 4;
		if (!std::isfinite(push.x) || !std::isfinite(push.y)) push = {0, 0, 0};
		push.z = 0;
		viewer.clear();
	}
};
#endif
