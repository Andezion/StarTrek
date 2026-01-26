#pragma once

#include <raylib.h>
#include "scene/camera.hpp"
#include "data/simulation_state.hpp"
#include "network/protocol.hpp"

namespace cosmodrom {

class Scene {
public:
    Scene(SimulationState& state);
    ~Scene();

    void update(float deltaTime);
    void render();

    CameraController& getCamera() { return m_camera; }

    void setShowTrajectory(bool show) { m_showTrajectory = show; }
    void setShowGrid(bool show) { m_showGrid = show; }
    void setHeightExaggeration(float factor) { m_heightExaggeration = factor; }

    void followRocket(const std::string& rocketId);
    void stopFollowing();

private:
    SimulationState& m_state;
    CameraController m_camera;

    bool m_showTrajectory = true;
    bool m_showGrid = true;
    float m_heightExaggeration = HEIGHT_EXAGGERATION;

    std::string m_followingRocketId;

    void renderPlanet();
    void renderCosmodromes();
    void renderRockets();
    void renderRocket(RocketData* rocket);
    void renderTrajectory(RocketData* rocket);
    void renderRocketLabel(RocketData* rocket);

    ::Vector3 worldToVisual(const Vector3& realPos) const;
    ::Vector3 worldToVisual(double x, double y, double z) const;
};

} 
