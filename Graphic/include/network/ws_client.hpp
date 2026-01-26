#pragma once

#include <ixwebsocket/IXWebSocket.h>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include "network/protocol.hpp"

namespace cosmodrom {

class WebSocketClient {
public:
    using BroadcastCallback = std::function<void(const BroadcastMessage&)>;
    using RocketJoinedCallback = std::function<void(const RocketJoinedMessage&)>;
    using RocketLeftCallback = std::function<void(const RocketLeftMessage&)>;
    using WarningCallback = std::function<void(const WarningMessage&)>;
    using ConnectionCallback = std::function<void(bool connected)>;

    WebSocketClient(const std::string& url = "ws://localhost:8080/ws");
    ~WebSocketClient();

    void connect();
    void disconnect();
    bool isConnected() const;

    void setBroadcastCallback(BroadcastCallback cb) { m_broadcastCallback = cb; }
    void setRocketJoinedCallback(RocketJoinedCallback cb) { m_rocketJoinedCallback = cb; }
    void setRocketLeftCallback(RocketLeftCallback cb) { m_rocketLeftCallback = cb; }
    void setWarningCallback(WarningCallback cb) { m_warningCallback = cb; }
    void setConnectionCallback(ConnectionCallback cb) { m_connectionCallback = cb; }

    void processMessages();

    const std::string& getObserverId() const { return m_observerId; }

private:
    ix::WebSocket m_webSocket;
    std::string m_url;
    std::string m_observerId;
    bool m_connected = false;
    bool m_subscribed = false;

    BroadcastCallback m_broadcastCallback;
    RocketJoinedCallback m_rocketJoinedCallback;
    RocketLeftCallback m_rocketLeftCallback;
    WarningCallback m_warningCallback;
    ConnectionCallback m_connectionCallback;

    std::mutex m_queueMutex;
    std::queue<std::string> m_messageQueue;

    void onMessage(const ix::WebSocketMessagePtr& msg);
    void handleMessage(const std::string& json);
    void sendSubscribe();
};

} 
