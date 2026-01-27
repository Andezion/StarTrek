#include "ui/ui_manager.hpp"
#include <cstdlib>

#ifndef TextToFloat
static inline float TextToFloat(const char* text) {
    return static_cast<float>(atof(text));
}
#endif

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

namespace cosmodrom {

UIManager::UIManager(SimulationState& state, int screenWidth, int screenHeight)
    : m_state(state), m_screenWidth(screenWidth), m_screenHeight(screenHeight) {
    m_panel = {0, 0, static_cast<float>(PANEL_WIDTH), static_cast<float>(screenHeight)};

    GuiSetStyle(DEFAULT, TEXT_SIZE, 14);
}

void UIManager::resize(int screenWidth, int screenHeight) {
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_panel.height = static_cast<float>(screenHeight);
}

void UIManager::update() {
    ::Vector2 mousePos = GetMousePosition();
    m_mouseOverUI = CheckCollisionPointRec(mousePos, m_panel);
}

void UIManager::render() {
    DrawRectangleRec(m_panel, Fade(DARKGRAY, 0.9f));
    DrawRectangleLinesEx(m_panel, 2, GRAY);

    float y = 10.0f;
    float padding = 10.0f;
    float width = PANEL_WIDTH - 2 * padding;

    DrawText("COSMODROM", static_cast<int>(padding), static_cast<int>(y), 20, WHITE);
    y += 30;

    const char* statusText = m_connected ? "Connected" : "Disconnected";
    Color statusColor = m_connected ? GREEN : RED;
    DrawText(statusText, static_cast<int>(padding), static_cast<int>(y), 14, statusColor);
    y += 25;

    DrawLine(static_cast<int>(padding), static_cast<int>(y),
             static_cast<int>(PANEL_WIDTH - padding), static_cast<int>(y), GRAY);
    y += 10;

    renderCreatePanel(y);

    DrawLine(static_cast<int>(padding), static_cast<int>(y),
             static_cast<int>(PANEL_WIDTH - padding), static_cast<int>(y), GRAY);
    y += 10;

    renderRocketList(y);

    DrawLine(static_cast<int>(padding), static_cast<int>(y),
             static_cast<int>(PANEL_WIDTH - padding), static_cast<int>(y), GRAY);
    y += 10;

    renderControlPanel(y);

    DrawLine(static_cast<int>(padding), static_cast<int>(y),
             static_cast<int>(PANEL_WIDTH - padding), static_cast<int>(y), GRAY);
    y += 10;

    renderLogPanel(y);

    renderStatusBar();
}

void UIManager::renderCreatePanel(float& y) {
    float padding = 10.0f;
    float width = PANEL_WIDTH - 2 * padding;

    DrawText("CREATE ROCKET", static_cast<int>(padding), static_cast<int>(y), 16, LIGHTGRAY);
    y += 25;

    DrawText("Name:", static_cast<int>(padding), static_cast<int>(y), 12, WHITE);
    y += 15;

    Rectangle nameRect = {padding, y, width, 25};
    if (GuiTextBox(nameRect, m_rocketName, 64, m_nameEditMode)) {
        m_nameEditMode = !m_nameEditMode;
    }
    y += 35;

    DrawText("Cosmodrome:", static_cast<int>(padding), static_cast<int>(y), 12, WHITE);
    y += 15;

    const auto& cosmodromes = m_state.getCosmodromes();
    float itemHeight = 22.0f;

    for (int i = 0; i < static_cast<int>(cosmodromes.size()); i++) {
        Rectangle itemRect = {padding, y, width, itemHeight};
        bool isSelected = (i == m_selectedCosmodromeIndex);

        Color bgColor = isSelected ? DARKGREEN : Fade(BLACK, 0.3f);
        DrawRectangleRec(itemRect, bgColor);
        DrawRectangleLinesEx(itemRect, 1, isSelected ? GREEN : GRAY);

        DrawText(cosmodromes[i].name.c_str(), static_cast<int>(padding + 5), static_cast<int>(y + 4), 12, WHITE);

        if (CheckCollisionPointRec(GetMousePosition(), itemRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            m_selectedCosmodromeIndex = i;
        }

        y += itemHeight + 2;
    }
    y += 10;

    Rectangle launchRect = {padding, y, width, 30};
    if (GuiButton(launchRect, "LAUNCH ROCKET")) {
        if (m_launchCallback && m_selectedCosmodromeIndex >= 0) {
            m_launchCallback(m_rocketName, m_selectedCosmodromeIndex);
            addLog("Launching: " + std::string(m_rocketName), GREEN);
        }
    }
    y += 40;
}

void UIManager::renderRocketList(float& y) {
    float padding = 10.0f;
    float width = PANEL_WIDTH - 2 * padding;

    DrawText("ACTIVE ROCKETS", static_cast<int>(padding), static_cast<int>(y), 16, LIGHTGRAY);
    y += 25;

    auto rockets = m_state.getAllRockets();

    if (rockets.empty()) {
        DrawText("No rockets", static_cast<int>(padding), static_cast<int>(y), 12, GRAY);
        y += 25;
        return;
    }

    int visibleCount = std::min(static_cast<int>(rockets.size()), 5);
    float itemHeight = 25.0f;

    for (int i = 0; i < visibleCount; i++) {
        RocketData* rocket = rockets[i];
        Rectangle itemRect = {padding, y, width, itemHeight};

        bool isSelected = (i == m_selectedRocketIndex);
        bool isTracked = (rocket->getId() == m_state.getTrackedRocketId());

        Color bgColor = isSelected ? DARKBLUE : (isTracked ? DARKGREEN : Fade(BLACK, 0.3f));
        DrawRectangleRec(itemRect, bgColor);

        DrawRectangle(static_cast<int>(padding), static_cast<int>(y),
                     5, static_cast<int>(itemHeight), rocket->getColor());

        const char* name = rocket->getName().c_str();
        DrawText(name, static_cast<int>(padding + 10), static_cast<int>(y + 5), 12, WHITE);

        const RocketState& state = rocket->getState();
        const char* status = "";
        Color statusColor = WHITE;
        if (state.crashed) {
            status = "[X]";
            statusColor = RED;
        } else if (state.landed) {
            status = "[L]";
            statusColor = GREEN;
        } else if (state.in_orbit) {
            status = "[O]";
            statusColor = SKYBLUE;
        } else {
            status = "[F]";
            statusColor = YELLOW;
        }
        DrawText(status, static_cast<int>(padding + width - 30), static_cast<int>(y + 5), 12, statusColor);

        if (CheckCollisionPointRec(GetMousePosition(), itemRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            m_selectedRocketIndex = i;
        }

        y += itemHeight + 2;
    }

    if (rockets.size() > 5) {
        char moreText[32];
        snprintf(moreText, sizeof(moreText), "... and %zu more", rockets.size() - 5);
        DrawText(moreText, static_cast<int>(padding), static_cast<int>(y), 10, GRAY);
        y += 15;
    }

    y += 10;
}

void UIManager::renderControlPanel(float& y) {
    float padding = 10.0f;
    float width = PANEL_WIDTH - 2 * padding;

    DrawText("CONTROLS", static_cast<int>(padding), static_cast<int>(y), 16, LIGHTGRAY);
    y += 25;

    auto rockets = m_state.getAllRockets();

    if (m_selectedRocketIndex >= 0 && m_selectedRocketIndex < static_cast<int>(rockets.size())) {
        RocketData* rocket = rockets[m_selectedRocketIndex];
        const RocketState& state = rocket->getState();

        DrawText(rocket->getName().c_str(), static_cast<int>(padding), static_cast<int>(y), 14, rocket->getColor());
        y += 20;

        char info[128];
        snprintf(info, sizeof(info), "Alt: %.1f km", state.altitude / 1000.0);
        DrawText(info, static_cast<int>(padding), static_cast<int>(y), 12, WHITE);
        y += 15;

        snprintf(info, sizeof(info), "Speed: %.1f m/s", state.speed);
        DrawText(info, static_cast<int>(padding), static_cast<int>(y), 12, WHITE);
        y += 15;

        snprintf(info, sizeof(info), "Fuel: %.0f kg", state.fuel_remaining);
        DrawText(info, static_cast<int>(padding), static_cast<int>(y), 12, WHITE);
        y += 20;

        Rectangle trackRect = {padding, y, width / 2 - 5, 25};
        const char* trackText = (rocket->getId() == m_state.getTrackedRocketId()) ? "UNTRACK" : "TRACK";
        if (GuiButton(trackRect, trackText)) {
            if (rocket->getId() == m_state.getTrackedRocketId()) {
                if (m_stopTrackCallback) m_stopTrackCallback();
            } else {
                if (m_trackCallback) m_trackCallback(rocket->getId());
            }
        }

        Rectangle centerRect = {padding + width / 2 + 5, y, width / 2 - 5, 25};
        if (GuiButton(centerRect, "CENTER")) {
            if (m_trackCallback) m_trackCallback(rocket->getId());
        }
        y += 35;
    } else {
        DrawText("Select a rocket", static_cast<int>(padding), static_cast<int>(y), 12, GRAY);
        y += 25;
    }
}

void UIManager::renderLogPanel(float& y) {
    float padding = 10.0f;
    float width = PANEL_WIDTH - 2 * padding;
    float remainingHeight = m_screenHeight - y - 40; 

    DrawText("LOGS", static_cast<int>(padding), static_cast<int>(y), 16, LIGHTGRAY);
    y += 25;

    Rectangle logArea = {padding, y, width, remainingHeight};
    DrawRectangleRec(logArea, Fade(BLACK, 0.5f));

    float logY = y + 5;
    int maxVisible = static_cast<int>((remainingHeight - 10) / 15);

    int start = std::max(0, static_cast<int>(m_logs.size()) - maxVisible);
    for (int i = static_cast<int>(m_logs.size()) - 1; i >= start && logY < y + remainingHeight - 15; i--) {
        const LogEntry& entry = m_logs[i];
        DrawText(entry.message.c_str(), static_cast<int>(padding + 5), static_cast<int>(logY), 10, entry.color);
        logY += 15;
    }

    y += remainingHeight + 10;
}

void UIManager::renderStatusBar() {
    float barHeight = 30.0f;
    Rectangle bar = {0, static_cast<float>(m_screenHeight) - barHeight,
                    static_cast<float>(PANEL_WIDTH), barHeight};
    DrawRectangleRec(bar, Fade(BLACK, 0.8f));

    char statusText[128];
    snprintf(statusText, sizeof(statusText), "Rockets: %zu | FPS: %d",
            m_state.getRocketCount(), GetFPS());
    DrawText(statusText, 10, m_screenHeight - 22, 12, WHITE);
}

void UIManager::addLog(const std::string& message, Color color) {
    LogEntry entry;
    entry.message = message;
    entry.color = color;
    entry.timestamp = static_cast<float>(GetTime());

    m_logs.push_back(entry);

    while (m_logs.size() > MAX_LOGS) {
        m_logs.erase(m_logs.begin());
    }
}

void UIManager::addWarning(const std::string& message) {
    addLog("[WARN] " + message, YELLOW);
}

void UIManager::addError(const std::string& message) {
    addLog("[ERR] " + message, RED);
}

} 
