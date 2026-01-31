#pragma once

#include <memory>
#include <string>
#include "scene/scene.hpp"
#include "network/ws_client.hpp"
#include "data/simulation_state.hpp"
#include "ui/ui_manager.hpp"

namespace cosmodrom {

struct AppConfig {
    int screenWidth = 1920;
    int screenHeight = 1080;
    std::string windowTitle = "Cosmodrom 3D Visualizer";
    std::string serverUrl = "ws://localhost:8080/ws";
    int targetFPS = 60;
};

class Application {
public:
    Application(const AppConfig& config = AppConfig());
    ~Application();

    void run();

private:
    AppConfig m_config;

    std::unique_ptr<SimulationState> m_state;
    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<WebSocketClient> m_wsClient;
    std::unique_ptr<UIManager> m_ui;

    bool m_running = true;

    void init();
    void initWindow();
    void initNetwork();
    void initScene();
    void initUI();

    void update(float deltaTime);
    void render();
    void handleInput();

    void onBroadcast(const BroadcastMessage& msg);
    void onRocketJoined(const RocketJoinedMessage& msg);
    void onRocketLeft(const RocketLeftMessage& msg);
    void onWarning(const WarningMessage& msg);
    void onConnectionChanged(bool connected);

    void onLaunchRocket(const std::string& name, int cosmodromeIndex);
    void onTrackRocket(const std::string& rocketId);
    void onStopTracking();

    void shutdown();
};

} 
