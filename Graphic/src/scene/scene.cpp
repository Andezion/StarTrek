#include "scene/scene.hpp"
#include <cmath>

namespace cosmodrom {

Scene::Scene(SimulationState& state)
    : m_state(state) {
}

Scene::~Scene() {
}

void Scene::update(float deltaTime) {
    m_camera.handleInput();

    if (!m_followingRocketId.empty()) {
        RocketData* rocket = m_state.getRocket(m_followingRocketId);
        if (rocket) {
            ::Vector3 visualPos = worldToVisual(rocket->getState().position);
            m_camera.followTarget(visualPos);
        } else {
            m_followingRocketId.clear();
            m_camera.setFreeMode();
        }
    }

    m_camera.update(deltaTime);
}

void Scene::render() {
    BeginMode3D(m_camera.getCamera());

    if (m_showGrid) {
        DrawGrid(20, 20.0f);
    }

    renderPlanet();
    renderCosmodromes();
    renderRockets();

    EndMode3D();

    auto rockets = m_state.getAllRockets();
    for (auto* rocket : rockets) {
        renderRocketLabel(rocket);
    }
}

void Scene::renderPlanet() {
    DrawSphere({0, 0, 0}, EARTH_VISUAL_RADIUS, GREEN);
    DrawSphereWires({0, 0, 0}, EARTH_VISUAL_RADIUS, 16, 16, DARKGREEN);
}

void Scene::renderCosmodromes() {
    const auto& cosmodromes = m_state.getCosmodromes();

    for (const auto& cosmodrome : cosmodromes) {
        ::Vector3 visualPos = worldToVisual(cosmodrome.position);
        
        DrawCube(visualPos, 3.0f, 3.0f, 3.0f, BLACK);
        DrawCubeWires(visualPos, 3.0f, 3.0f, 3.0f, GRAY);
    }
}

void Scene::renderRockets() {
    auto rockets = m_state.getAllRockets();

    for (auto* rocket : rockets) {
        renderRocket(rocket);
        if (m_showTrajectory) {
            renderTrajectory(rocket);
        }
    }
}

void Scene::renderRocket(RocketData* rocket) {
    const RocketState& state = rocket->getState();
    ::Vector3 visualPos = worldToVisual(state.position);

    Color rocketColor = rocket->getColor();

    float rocketSize = 0.8f;
    if (rocket->isActive()) {
        rocketSize = 1.2f;
    }

    ::Vector3 direction = {0, 0, 1};
    if (state.speed > 10.0) {
        float vlen = std::sqrt(state.velocity.x * state.velocity.x +
                               state.velocity.y * state.velocity.y +
                               state.velocity.z * state.velocity.z);
        if (vlen > 0) {
            direction = {
                static_cast<float>(state.velocity.x / vlen),
                static_cast<float>(state.velocity.y / vlen),
                static_cast<float>(state.velocity.z / vlen)
            };
        }
    }

    DrawSphere(visualPos, rocketSize, rocketColor);

    if (rocket->isActive() && state.fuel_remaining > 0) {
        ::Vector3 flamePos = {
            visualPos.x - direction.x * rocketSize * 2,
            visualPos.y - direction.y * rocketSize * 2,
            visualPos.z - direction.z * rocketSize * 2
        };
        DrawSphere(flamePos, rocketSize * 0.7f, ORANGE);
    }

    if (state.crashed) {
        DrawSphere(visualPos, rocketSize * 1.5f, RED);
    }

    if (state.in_orbit) {
        DrawCircle3D(visualPos, rocketSize * 2.5f, {1, 0, 0}, 90.0f, SKYBLUE);
    }
}

void Scene::renderTrajectory(RocketData* rocket) {
    const auto& history = rocket->getHistory();

    if (history.size() < 2) {
        return;
    }

    Color trajectoryColor = rocket->getColor();
    trajectoryColor.a = 150; 

    ::Vector3 prevPos = worldToVisual(history[0].position);

    for (size_t i = 1; i < history.size(); i++) {
        ::Vector3 currPos = worldToVisual(history[i].position);

        float alpha = static_cast<float>(i) / static_cast<float>(history.size());
        Color lineColor = trajectoryColor;
        lineColor.a = static_cast<unsigned char>(50 + alpha * 150);

        DrawLine3D(prevPos, currPos, lineColor);

        if (i % 50 == 0) {
            DrawSphere(currPos, 0.5f, BLUE);
        }

        prevPos = currPos;
    }
}

void Scene::renderRocketLabel(RocketData* rocket) {
    const RocketState& state = rocket->getState();
    ::Vector3 visualPos = worldToVisual(state.position);

    ::Vector2 screenPos = GetWorldToScreen(visualPos, m_camera.getCamera());

    ::Vector3 camPos = m_camera.getCamera().position;
    ::Vector3 toRocket = {
        visualPos.x - camPos.x,
        visualPos.y - camPos.y,
        visualPos.z - camPos.z
    };

    ::Vector3 camDir = {
        m_camera.getCamera().target.x - camPos.x,
        m_camera.getCamera().target.y - camPos.y,
        m_camera.getCamera().target.z - camPos.z
    };

    float dot = toRocket.x * camDir.x + toRocket.y * camDir.y + toRocket.z * camDir.z;
    if (dot < 0) {
        return; 
    }

    const char* name = rocket->getName().c_str();
    DrawText(name, static_cast<int>(screenPos.x) - 30, static_cast<int>(screenPos.y) - 30, 14, rocket->getColor());

    const char* status = "";
    Color statusColor = WHITE;
    if (state.crashed) {
        status = "CRASHED";
        statusColor = RED;
    } else if (state.landed) {
        status = "LANDED";
        statusColor = GREEN;
    } else if (state.in_orbit) {
        status = "IN ORBIT";
        statusColor = SKYBLUE;
    } else {
        char altText[32];
        snprintf(altText, sizeof(altText), "%.1f km", state.altitude / 1000.0);
        DrawText(altText, static_cast<int>(screenPos.x) - 25, static_cast<int>(screenPos.y) - 15, 12, WHITE);
        return;
    }
    DrawText(status, static_cast<int>(screenPos.x) - 30, static_cast<int>(screenPos.y) - 15, 12, statusColor);
}

void Scene::followRocket(const std::string& rocketId) {
    m_followingRocketId = rocketId;
    m_state.setTrackedRocket(rocketId);
}

void Scene::stopFollowing() {
    m_followingRocketId.clear();
    m_state.setTrackedRocket("");
    m_camera.setFreeMode();
}

::Vector3 Scene::worldToVisual(const Vector3& realPos) const {
    return worldToVisual(realPos.x, realPos.y, realPos.z);
}

::Vector3 Scene::worldToVisual(double x, double y, double z) const {
    const float BASE_SCALE = EARTH_VISUAL_RADIUS / static_cast<float>(EARTH_RADIUS);

    double realDist = std::sqrt(x*x + y*y + z*z);
    if (realDist < 1.0) {
        return {0, 0, 0};
    }

    float nx = static_cast<float>(x / realDist);
    float ny = static_cast<float>(y / realDist);
    float nz = static_cast<float>(z / realDist);

    double altitude = realDist - EARTH_RADIUS;

    float visualDist = EARTH_VISUAL_RADIUS + static_cast<float>(altitude) * BASE_SCALE * m_heightExaggeration;

    return {
        nx * visualDist,
        ny * visualDist,
        nz * visualDist
    };
}

} 
