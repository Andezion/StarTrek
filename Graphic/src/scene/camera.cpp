#include "scene/camera.hpp"
#include <cmath>

namespace cosmodrom {

CameraController::CameraController() {
    m_camera.position = {200.0f, 200.0f, 200.0f};
    m_camera.target = {0.0f, 0.0f, 0.0f};
    m_camera.up = {0.0f, 0.0f, 1.0f};
    m_camera.fovy = 45.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;

    m_currentPosition = m_camera.position;
    m_targetPosition = m_camera.position;
    m_currentLookAt = m_camera.target;
    m_targetLookAt = m_camera.target;

    updateOrbitCamera();
}

void CameraController::update(float deltaTime) {
    float t = 1.0f - std::exp(-m_lerpFactor * deltaTime);

    m_currentPosition = lerp(m_currentPosition, m_targetPosition, t);
    m_currentLookAt = lerp(m_currentLookAt, m_targetLookAt, t);

    m_camera.position = m_currentPosition;
    m_camera.target = m_currentLookAt;

    if (m_followMode) {
        m_orbitCenter = m_followTarget;
        updateOrbitCamera();
    }
}

void CameraController::handleInput() {
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        zoom(-wheel * m_zoomSpeed);
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        ::Vector2 delta = GetMouseDelta();
        rotate(delta.x * m_rotationSpeed, -delta.y * m_rotationSpeed);
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        ::Vector2 delta = GetMouseDelta();
        m_orbitCenter.x -= delta.x * 0.5f;
        m_orbitCenter.y -= delta.y * 0.5f;
        updateOrbitCamera();
    }
}

void CameraController::zoom(float delta) {
    m_orbitDistance += delta;
    m_orbitDistance = std::max(m_minDistance, std::min(m_maxDistance, m_orbitDistance));
    updateOrbitCamera();
}

void CameraController::rotate(float yawDelta, float pitchDelta) {
    m_orbitYaw += yawDelta;
    m_orbitPitch += pitchDelta;

    m_orbitPitch = std::max(-89.0f, std::min(89.0f, m_orbitPitch));

    updateOrbitCamera();
}

void CameraController::setTarget(::Vector3 target) {
    m_targetLookAt = target;
}

void CameraController::setFreeMode() {
    m_followMode = false;
}

void CameraController::followTarget(::Vector3 target) {
    m_followMode = true;
    m_followTarget = target;
    m_orbitCenter = target;
    updateOrbitCamera();
}

void CameraController::smoothMoveTo(::Vector3 target, float duration) {
    m_targetLookAt = target;
    m_orbitCenter = target;
    updateOrbitCamera();
}

void CameraController::updateOrbitCamera() {
    const float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

    float yawRad = m_orbitYaw * DEG_TO_RAD;
    float pitchRad = m_orbitPitch * DEG_TO_RAD;

    m_targetPosition.x = m_orbitCenter.x + m_orbitDistance * std::cos(pitchRad) * std::cos(yawRad);
    m_targetPosition.y = m_orbitCenter.y + m_orbitDistance * std::cos(pitchRad) * std::sin(yawRad);
    m_targetPosition.z = m_orbitCenter.z + m_orbitDistance * std::sin(pitchRad);

    m_targetLookAt = m_orbitCenter;
}

::Vector3 CameraController::lerp(::Vector3 a, ::Vector3 b, float t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    };
}

} 
