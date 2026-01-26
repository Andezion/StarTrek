#pragma once

#include <string>
#include <deque>
#include <raylib.h>
#include "network/protocol.hpp"

namespace cosmodrom {

struct TrajectoryPoint {
    Vector3 position;
    float timestamp;
};

class RocketData {
public:
    static constexpr size_t MAX_HISTORY_POINTS = 10000;
    static constexpr float MIN_POINT_DISTANCE = 100.0f;

    RocketData(const std::string& id, const std::string& name, Color color);

    void updateState(const RocketState& state);
    void setConfig(const RocketConfig& config);

    const std::string& getId() const { return m_id; }
    const std::string& getName() const { return m_name; }
    Color getColor() const { return m_color; }
    const RocketState& getState() const { return m_currentState; }
    const RocketConfig& getConfig() const { return m_config; }

    const std::deque<TrajectoryPoint>& getHistory() const { return m_history; }

    bool isActive() const;
    float getTimeSinceLastUpdate() const;

private:
    std::string m_id;
    std::string m_name;
    Color m_color;

    RocketState m_currentState;
    RocketConfig m_config;

    std::deque<TrajectoryPoint> m_history;
    float m_lastUpdateTime = 0.0f;
};

class ColorGenerator {
public:
    Color getNextColor();

private:
    float m_currentHue = 0.0f;
};

} 
