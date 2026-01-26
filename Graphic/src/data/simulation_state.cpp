#include "data/simulation_state.hpp"
#include <cmath>

namespace cosmodrom {

SimulationState::SimulationState() {
    initCosmodromes();
}

void SimulationState::initCosmodromes() {
    Cosmodrome baikonur;
    baikonur.name = "Baikonur";
    baikonur.latitude = 45.965;
    baikonur.longitude = 63.305;
    baikonur.position = sphericalToCartesian(baikonur.latitude, baikonur.longitude, 0);
    m_cosmodromes.push_back(baikonur);

    Cosmodrome canaveral;
    canaveral.name = "Cape Canaveral";
    canaveral.latitude = 28.573;
    canaveral.longitude = -80.649;
    canaveral.position = sphericalToCartesian(canaveral.latitude, canaveral.longitude, 0);
    m_cosmodromes.push_back(canaveral);

    Cosmodrome kourou;
    kourou.name = "Kourou";
    kourou.latitude = 5.239;
    kourou.longitude = -52.768;
    kourou.position = sphericalToCartesian(kourou.latitude, kourou.longitude, 0);
    m_cosmodromes.push_back(kourou);

    Cosmodrome vostochny;
    vostochny.name = "Vostochny";
    vostochny.latitude = 51.884;
    vostochny.longitude = 128.333;
    vostochny.position = sphericalToCartesian(vostochny.latitude, vostochny.longitude, 0);
    m_cosmodromes.push_back(vostochny);
}

Vector3 SimulationState::sphericalToCartesian(double lat, double lon, double altitude) {
    const double DEG_TO_RAD = M_PI / 180.0;
    double r = EARTH_RADIUS + altitude;
    double latRad = lat * DEG_TO_RAD;
    double lonRad = lon * DEG_TO_RAD;

    Vector3 pos;
    pos.x = r * std::cos(latRad) * std::cos(lonRad);
    pos.y = r * std::cos(latRad) * std::sin(lonRad);
    pos.z = r * std::sin(latRad);
    return pos;
}

void SimulationState::addRocket(const std::string& id, const std::string& name, const RocketConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_rockets.find(id) != m_rockets.end()) {
        return; 
    }

    Color color = m_colorGenerator.getNextColor();
    auto rocket = std::make_unique<RocketData>(id, name, color);
    rocket->setConfig(config);
    m_rockets[id] = std::move(rocket);
}

void SimulationState::updateRocket(const std::string& id, const RocketState& state) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_rockets.find(id);
    if (it != m_rockets.end()) {
        it->second->updateState(state);
    }
}

void SimulationState::removeRocket(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_rockets.erase(id);
    if (m_trackedRocketId == id) {
        m_trackedRocketId.clear();
    }
}

RocketData* SimulationState::getRocket(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_rockets.find(id);
    if (it != m_rockets.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<RocketData*> SimulationState::getAllRockets() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<RocketData*> rockets;
    rockets.reserve(m_rockets.size());
    for (auto& pair : m_rockets) {
        rockets.push_back(pair.second.get());
    }
    return rockets;
}

size_t SimulationState::getRocketCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rockets.size();
}

void SimulationState::setTrackedRocket(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_trackedRocketId = id;
}

RocketData* SimulationState::getTrackedRocket() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_trackedRocketId.empty()) {
        return nullptr;
    }
    auto it = m_rockets.find(m_trackedRocketId);
    if (it != m_rockets.end()) {
        return it->second.get();
    }
    return nullptr;
}

} 
