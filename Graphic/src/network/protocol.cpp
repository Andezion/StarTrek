#include "network/protocol.hpp"

namespace cosmodrom {

MessageType parseMessageType(const std::string& type) {
    if (type == "register") return MessageType::Register;
    if (type == "telemetry") return MessageType::Telemetry;
    if (type == "disconnect") return MessageType::Disconnect;
    if (type == "accepted") return MessageType::Accepted;
    if (type == "rejected") return MessageType::Rejected;
    if (type == "command") return MessageType::Command;
    if (type == "warning") return MessageType::Warning;
    if (type == "shutdown") return MessageType::Shutdown;
    if (type == "trajectory") return MessageType::Trajectory;
    if (type == "rocket_list") return MessageType::RocketList;
    if (type == "subscribe") return MessageType::Subscribe;
    if (type == "unsubscribe") return MessageType::Unsubscribe;
    if (type == "broadcast") return MessageType::Broadcast;
    if (type == "rocket_joined") return MessageType::RocketJoined;
    if (type == "rocket_left") return MessageType::RocketLeft;
    return MessageType::Unknown;
}

std::string messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::Register: return "register";
        case MessageType::Telemetry: return "telemetry";
        case MessageType::Disconnect: return "disconnect";
        case MessageType::Accepted: return "accepted";
        case MessageType::Rejected: return "rejected";
        case MessageType::Command: return "command";
        case MessageType::Warning: return "warning";
        case MessageType::Shutdown: return "shutdown";
        case MessageType::Trajectory: return "trajectory";
        case MessageType::RocketList: return "rocket_list";
        case MessageType::Subscribe: return "subscribe";
        case MessageType::Unsubscribe: return "unsubscribe";
        case MessageType::Broadcast: return "broadcast";
        case MessageType::RocketJoined: return "rocket_joined";
        case MessageType::RocketLeft: return "rocket_left";
        default: return "unknown";
    }
}

void from_json(const nlohmann::json& j, Message& m) {
    j.at("type").get_to(m.type);
    if (j.contains("timestamp")) {
        m.timestamp = j.at("timestamp").get<std::string>();
    }
    if (j.contains("data")) {
        m.data = j.at("data");
    }
}

} 
