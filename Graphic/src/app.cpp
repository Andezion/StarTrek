#include "app.hpp"
#include <cstdlib>
#include <random>

namespace cosmodrom {

Application::Application(const AppConfig& config)
    : m_config(config) {
}

Application::~Application() {
    shutdown();
}

void Application::run() {
    init();

    while (!WindowShouldClose() && m_running) {
        float deltaTime = GetFrameTime();

        handleInput();
        update(deltaTime);
        render();
    }

    shutdown();
}

void Application::init() {
    initWindow();
    m_state = std::make_unique<SimulationState>();
    initScene();
    initUI();
    initNetwork();
}

void Application::initWindow() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(m_config.screenWidth, m_config.screenHeight, m_config.windowTitle.c_str());
    SetTargetFPS(m_config.targetFPS);
}

void Application::initNetwork() {
    m_wsClient = std::make_unique<WebSocketClient>(m_config.serverUrl);

    m_wsClient->setBroadcastCallback([this](const BroadcastMessage& msg) {
        onBroadcast(msg);
    });

    m_wsClient->setRocketJoinedCallback([this](const RocketJoinedMessage& msg) {
        onRocketJoined(msg);
    });

    m_wsClient->setRocketLeftCallback([this](const RocketLeftMessage& msg) {
        onRocketLeft(msg);
    });

    m_wsClient->setWarningCallback([this](const WarningMessage& msg) {
        onWarning(msg);
    });

    m_wsClient->setConnectionCallback([this](bool connected) {
        onConnectionChanged(connected);
    });

    m_wsClient->connect();
    m_ui->addLog("Connecting to " + m_config.serverUrl + "...", YELLOW);
}

void Application::initScene() {
    m_scene = std::make_unique<Scene>(*m_state);
}

void Application::initUI() {
    m_ui = std::make_unique<UIManager>(*m_state, m_config.screenWidth, m_config.screenHeight);

    m_ui->setLaunchCallback([this](const std::string& name, double lat, double lon) {
        onLaunchRocket(name, lat, lon);
    });

    m_ui->setTrackCallback([this](const std::string& rocketId) {
        onTrackRocket(rocketId);
    });

    m_ui->setStopTrackCallback([this]() {
        onStopTracking();
    });
}

void Application::handleInput() {
    if (IsWindowResized()) {
        m_config.screenWidth = GetScreenWidth();
        m_config.screenHeight = GetScreenHeight();
        m_ui->resize(m_config.screenWidth, m_config.screenHeight);
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        m_running = false;
    }

    if (IsKeyPressed(KEY_SPACE)) {
        onStopTracking();
    }
}

void Application::update(float deltaTime) {
    m_wsClient->processMessages();

    m_ui->update();

    if (!m_ui->isMouseOverUI()) {
        m_scene->update(deltaTime);
    } else {
        m_scene->getCamera().update(deltaTime);
    }
}

void Application::render() {
    BeginDrawing();
    ClearBackground(Color{20, 20, 30, 255});

    m_scene->render();

    m_ui->render();

    int helpY = m_config.screenHeight - 60;
    DrawText("RMB: Rotate | Scroll: Zoom | MMB: Pan | Space: Stop tracking",
             UIManager::PANEL_WIDTH + 10, helpY, 12, GRAY);

    EndDrawing();
}

void Application::onBroadcast(const BroadcastMessage& msg) {
    m_state->updateRocket(msg.rocket_id, msg.state);
}

void Application::onRocketJoined(const RocketJoinedMessage& msg) {
    m_state->addRocket(msg.rocket_id, msg.name, msg.config);
    m_ui->addLog("Rocket joined: " + msg.name, GREEN);
}

void Application::onRocketLeft(const RocketLeftMessage& msg) {
    m_state->removeRocket(msg.rocket_id);
    m_ui->addLog("Rocket left: " + msg.rocket_id, ORANGE);
}

void Application::onWarning(const WarningMessage& msg) {
    m_ui->addWarning(msg.warning);
}

void Application::onConnectionChanged(bool connected) {
    m_ui->setConnected(connected);
    if (connected) {
        m_ui->addLog("Connected to server", GREEN);
    } else {
        m_ui->addError("Disconnected from server");
    }
}

void Application::onLaunchRocket(const std::string& name, double lat, double lon) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 99999);
    std::string rocketId = "rocket-" + std::to_string(dis(gen));

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "../Client/cosmodrom-client -id %s -name \"%s\" -lat %.3f -lon %.3f &",
             rocketId.c_str(), name.c_str(), lat, lon);

    int result = system(cmd);
    if (result == 0) {
        m_ui->addLog("Launched rocket subprocess", GREEN);
    } else {
        m_ui->addError("Failed to launch rocket");
    }
}

void Application::onTrackRocket(const std::string& rocketId) {
    m_scene->followRocket(rocketId);
    m_ui->addLog("Tracking: " + rocketId, SKYBLUE);
}

void Application::onStopTracking() {
    m_scene->stopFollowing();
    m_ui->addLog("Stopped tracking", GRAY);
}

void Application::shutdown() {
    if (m_wsClient) {
        m_wsClient->disconnect();
    }
    CloseWindow();
}

} 
