#pragma once

#include <raylib.h>

namespace cosmodrom {

class CameraController {
public:
    CameraController();

    void update(float deltaTime);
    void handleInput();

    void zoom(float delta);
    void rotate(float yawDelta, float pitchDelta);
    void setTarget(::Vector3 target);

    void setFreeMode();
    void followTarget(::Vector3 target);

    void smoothMoveTo(::Vector3 target, float duration = 1.0f);

    Camera3D& getCamera() { return m_camera; }
    const Camera3D& getCamera() const { return m_camera; }

    void setZoomSpeed(float speed) { m_zoomSpeed = speed; }
    void setRotationSpeed(float speed) { m_rotationSpeed = speed; }
    void setLerpFactor(float factor) { m_lerpFactor = factor; }

    float getDistance() const { return m_orbitDistance; }

private:
    Camera3D m_camera;

    float m_orbitDistance = 300.0f;
    float m_orbitYaw = 45.0f;
    float m_orbitPitch = 30.0f;
    ::Vector3 m_orbitCenter = {0, 0, 0};

    ::Vector3 m_currentPosition;
    ::Vector3 m_targetPosition;
    ::Vector3 m_currentLookAt;
    ::Vector3 m_targetLookAt;

    float m_zoomSpeed = 20.0f;
    float m_rotationSpeed = 0.3f;
    float m_lerpFactor = 5.0f;

    float m_minDistance = 20.0f;
    float m_maxDistance = 1000.0f;

    bool m_followMode = false;
    ::Vector3 m_followTarget = {0, 0, 0};

    void updateOrbitCamera();
    ::Vector3 lerp(::Vector3 a, ::Vector3 b, float t);
};

} 
