#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace cosmodrom {

constexpr double EARTH_RADIUS = 6371000.0;      // м
constexpr double EARTH_MASS = 5.972e24;         // кг
constexpr double G_CONSTANT = 6.674e-11;        // м3/(кг*с2)
constexpr double ORBITAL_VELOCITY = 7900.0;     // м/с
constexpr double ATMOSPHERE_HEIGHT = 100000.0;  // м

constexpr float EARTH_VISUAL_RADIUS = 100.0f;
constexpr float HEIGHT_EXAGGERATION = 10.0f;

struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Vector3, x, y, z)
};

struct Engine {
    double thrust = 0.0;
    double fuel_consumption = 0.0;
    bool is_active = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Engine, thrust, fuel_consumption, is_active)
};

struct RocketConfig {
    std::string name;
    double mass_empty = 0.0;
    double mass_fuel = 0.0;
    double mass_fuel_max = 0.0;
    std::string fuel_type;
    std::vector<Engine> engines;
    double drag_coefficient = 0.0;
    double cross_section = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RocketConfig, name, mass_empty, mass_fuel,
        mass_fuel_max, fuel_type, engines, drag_coefficient, cross_section)
};

struct RocketState {
    Vector3 position;
    Vector3 velocity;
    Vector3 acceleration;
    double altitude = 0.0;
    double speed = 0.0;
    double mass_current = 0.0;
    double fuel_remaining = 0.0;
    bool in_orbit = false;
    bool landed = false;
    bool crashed = false;
    double time = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RocketState, position, velocity, acceleration,
        altitude, speed, mass_current, fuel_remaining, in_orbit, landed, crashed, time)
};

struct RocketInfo {
    std::string rocket_id;
    std::string name;
    RocketState state;
    RocketConfig config;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RocketInfo, rocket_id, name, state, config)
};

enum class MessageType {
    Register,
    Telemetry,
    Disconnect,
    Accepted,
    Rejected,
    Command,
    Warning,
    Shutdown,
    Trajectory,
    RocketList,
    Subscribe,
    Unsubscribe,
    Broadcast,
    RocketJoined,
    RocketLeft,
    Unknown
};

MessageType parseMessageType(const std::string& type);
std::string messageTypeToString(MessageType type);

struct SubscribeMessage {
    std::string observer_id;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(SubscribeMessage, observer_id)
};

struct BroadcastMessage {
    std::string rocket_id;
    std::string name;
    RocketState state;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(BroadcastMessage, rocket_id, name, state)
};

struct RocketJoinedMessage {
    std::string rocket_id;
    std::string name;
    RocketConfig config;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RocketJoinedMessage, rocket_id, name, config)
};

struct RocketLeftMessage {
    std::string rocket_id;
    std::string reason;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RocketLeftMessage, rocket_id, reason)
};

struct WarningMessage {
    std::string rocket_id;
    std::string warning;
    std::string severity;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(WarningMessage, rocket_id, warning, severity)
};

struct Message {
    std::string type;
    std::string timestamp;
    nlohmann::json data;
};

void from_json(const nlohmann::json& j, Message& m);

} 
