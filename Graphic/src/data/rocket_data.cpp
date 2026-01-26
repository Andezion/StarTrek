#include "data/rocket_data.hpp"
#include <cmath>

namespace cosmodrom {

RocketData::RocketData(const std::string& id, const std::string& name, Color color)
    : m_id(id), m_name(name), m_color(color) {}

void RocketData::updateState(const RocketState& state) {
    m_currentState = state;
    m_lastUpdateTime = static_cast<float>(GetTime());

    TrajectoryPoint point;
    point.position = state.position;
    point.timestamp = static_cast<float>(state.time);

    if (!m_history.empty()) {
        const auto& last = m_history.back();
        double dx = state.position.x - last.position.x;
        double dy = state.position.y - last.position.y;
        double dz = state.position.z - last.position.z;
        double dist = std::sqrt(dx*dx + dy*dy + dz*dz);

        if (dist < MIN_POINT_DISTANCE) {
            return; 
        }
    }

    m_history.push_back(point);

    while (m_history.size() > MAX_HISTORY_POINTS) {
        m_history.pop_front();
    }
}

void RocketData::setConfig(const RocketConfig& config) {
    m_config = config;
    m_name = config.name;
}

bool RocketData::isActive() const {
    return !m_currentState.crashed && !m_currentState.landed && !m_currentState.in_orbit;
}

float RocketData::getTimeSinceLastUpdate() const {
    return static_cast<float>(GetTime()) - m_lastUpdateTime;
}

Color ColorGenerator::getNextColor() {
    m_currentHue = std::fmod(m_currentHue + 0.618033988749895f, 1.0f);
    return ColorFromHSV(m_currentHue * 360.0f, 0.8f, 0.9f);
}

} 
