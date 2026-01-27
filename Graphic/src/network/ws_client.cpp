#include "network/ws_client.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <random>

namespace cosmodrom {

WebSocketClient::WebSocketClient(const std::string& url)
    : m_url(url) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 99999);
    m_observerId = "visualizer-" + std::to_string(dis(gen));
}

WebSocketClient::~WebSocketClient() {
    disconnect();
}

void WebSocketClient::connect() {
    m_webSocket.setUrl(m_url);

    m_webSocket.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        onMessage(msg);
    });

    m_webSocket.start();
}

void WebSocketClient::disconnect() {
    m_webSocket.stop();
    m_connected = false;
    m_subscribed = false;
}

bool WebSocketClient::isConnected() const {
    return m_connected;
}

void WebSocketClient::onMessage(const ix::WebSocketMessagePtr& msg) {
    switch (msg->type) {
        case ix::WebSocketMessageType::Open:
            m_connected = true;
            sendSubscribe();
            if (m_connectionCallback) {
                m_connectionCallback(true);
            }
            break;

        case ix::WebSocketMessageType::Close:
            m_connected = false;
            m_subscribed = false;
            if (m_connectionCallback) {
                m_connectionCallback(false);
            }
            break;

        case ix::WebSocketMessageType::Message:
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_messageQueue.push(msg->str);
            }
            break;

        case ix::WebSocketMessageType::Error:
            m_connected = false;
            if (m_connectionCallback) {
                m_connectionCallback(false);
            }
            break;

        default:
            break;
    }
}

void WebSocketClient::sendSubscribe() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&time_t_now));

    nlohmann::json msg;
    msg["type"] = "subscribe";
    msg["timestamp"] = std::string(timestamp);
    msg["data"] = {{"observer_id", m_observerId}};

    m_webSocket.send(msg.dump());
    m_subscribed = true;
}

void WebSocketClient::processMessages() {
    std::queue<std::string> messages;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::swap(messages, m_messageQueue);
    }

    while (!messages.empty()) {
        handleMessage(messages.front());
        messages.pop();
    }
}

void WebSocketClient::handleMessage(const std::string& json) {
    try {
        auto j = nlohmann::json::parse(json);
        Message msg;
        from_json(j, msg);

        MessageType type = parseMessageType(msg.type);

        switch (type) {
            case MessageType::Broadcast:
                if (m_broadcastCallback) {
                    BroadcastMessage bm = msg.data.get<BroadcastMessage>();
                    m_broadcastCallback(bm);
                }
                break;

            case MessageType::RocketJoined:
                if (m_rocketJoinedCallback) {
                    RocketJoinedMessage rjm = msg.data.get<RocketJoinedMessage>();
                    m_rocketJoinedCallback(rjm);
                }
                break;

            case MessageType::RocketLeft:
                if (m_rocketLeftCallback) {
                    RocketLeftMessage rlm = msg.data.get<RocketLeftMessage>();
                    m_rocketLeftCallback(rlm);
                }
                break;

            case MessageType::Warning:
                if (m_warningCallback) {
                    WarningMessage wm = msg.data.get<WarningMessage>();
                    m_warningCallback(wm);
                }
                break;

            default:
                break;
        }
    } catch (const std::exception& e) {
        
    }
}

} 
