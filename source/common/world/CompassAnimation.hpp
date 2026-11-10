#ifndef COMPASSANIMATION_HPP_
#define COMPASSANIMATION_HPP_
#include <algorithm>
#include <cmath>
#include "common/core/IntTypes.hpp"
class CompassAnimation {
	public:
		unsigned update(u64 tick, u16 dimension, double x, double y, double yaw) {
			constexpr double tau = 6.283185307179586;
			if (tick != m_tick) {
				// engine yaw increases toward north; compass frames increase clockwise.
				double target = yaw * tau / 360.0 - std::atan2(0.5 - y, 0.5 - x);
				if (dimension) { m_random = m_random * 1664525u + 1013904223u; target = double(m_random) / 4294967296.0 * tau; }
				if (m_tick == ~u64(0) && !dimension) m_angle = target;
				double difference = std::remainder(target - m_angle, tau);
				m_velocity = (m_velocity + std::clamp(difference, -1.0, 1.0) * 0.1) * 0.8;
				m_angle = std::remainder(m_angle + m_velocity, tau);
				m_tick = tick;
			}
			return unsigned(int(std::floor(m_angle / tau * 32 + 16.5)) & 31);
		}
	private:
		u64 m_tick = ~u64(0);
		u32 m_random = 0x9e3779b9;
		double m_angle = 0, m_velocity = 0;
};
#endif
