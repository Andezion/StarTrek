#pragma once

#include <raylib.h>
#include <string>
#include <vector>
#include <functional>
#include "data/simulation_state.hpp"

namespace cosmodrom {

struct LogEntry {
    std::string message;
    Color color;
    float timestamp;
};

class UIManager {
public:
    static constexpr int PANEL_WIDTH = 280;

    UIManager(SimulationState& state, int screenWidth, int screenHeight);

    void update();
    void render();

    using LaunchCallback = std::function<void(const std::string& name, double lat, double lon)>;
    using TrackCallback = std::function<void(const std::string& rocketId)>;
    using StopTrackCallback = std::function<void()>;

    void setLaunchCallback(LaunchCallback cb) { m_launchCallback = cb; }
    void setTrackCallback(TrackCallback cb) { m_trackCallback = cb; }
    void setStopTrackCallback(StopTrackCallback cb) { m_stopTrackCallback = cb; }

    void addLog(const std::string& message, Color color = WHITE);
    void addWarning(const std::string& message);
    void addError(const std::string& message);

    bool isMouseOverUI() const { return m_mouseOverUI; }
    void setConnected(bool connected) { m_connected = connected; }

    void resize(int screenWidth, int screenHeight);

private:
    SimulationState& m_state;
    int m_screenWidth;
    int m_screenHeight;

    Rectangle m_panel;
    bool m_mouseOverUI = false;
    bool m_connected = false;

    char m_rocketName[64] = "NewRocket";
    bool m_nameEditMode = false;
    float m_launchLat = 45.965f;
    float m_launchLon = 63.305f;

    std::vector<LogEntry> m_logs;
    static constexpr size_t MAX_LOGS = 50;
    int m_logScroll = 0;

    int m_selectedRocketIndex = -1;

    LaunchCallback m_launchCallback;
    TrackCallback m_trackCallback;
    StopTrackCallback m_stopTrackCallback;

    void renderCreatePanel(float& y);
    void renderRocketList(float& y);
    void renderControlPanel(float& y);
    void renderLogPanel(float& y);
    void renderStatusBar();
};

} 
