#pragma once

#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include "data/rocket_data.hpp"

namespace cosmodrom {

struct Cosmodrome {
    std::string name;
    double latitude;
    double longitude;
    Vector3 position;  // Декартовы координаты
};

class SimulationState {
public:
    SimulationState();

    void addRocket(const std::string& id, const std::string& name, const RocketConfig& config);
    void updateRocket(const std::string& id, const RocketState& state);
    void removeRocket(const std::string& id);

    RocketData* getRocket(const std::string& id);
    std::vector<RocketData*> getAllRockets();
    size_t getRocketCount();

    void setTrackedRocket(const std::string& id);
    RocketData* getTrackedRocket();
    const std::string& getTrackedRocketId() const { return m_trackedRocketId; }

    const std::vector<Cosmodrome>& getCosmodromes() const { return m_cosmodromes; }

private:
    std::unordered_map<std::string, std::unique_ptr<RocketData>> m_rockets;
    std::string m_trackedRocketId;
    std::vector<Cosmodrome> m_cosmodromes;
    ColorGenerator m_colorGenerator;
    mutable std::mutex m_mutex;

    void initCosmodromes();
    Vector3 sphericalToCartesian(double lat, double lon, double altitude);
};

} 
