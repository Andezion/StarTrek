package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"math"
	"net/http"
	"sync"
	"time"

	"cosmodrom/server/protocol"

	"github.com/gorilla/websocket"
)

type LogEntry struct {
	Timestamp time.Time `json:"timestamp"`
	Message   string    `json:"message"`
	Level     string    `json:"level"`
}

type LogBuffer struct {
	entries []LogEntry
	maxSize int
	mu      sync.RWMutex
}

func NewLogBuffer(maxSize int) *LogBuffer {
	return &LogBuffer{
		entries: make([]LogEntry, 0, maxSize),
		maxSize: maxSize,
	}
}

func (lb *LogBuffer) Add(level, message string) {
	lb.mu.Lock()
	defer lb.mu.Unlock()
	entry := LogEntry{
		Timestamp: time.Now(),
		Message:   message,
		Level:     level,
	}
	if len(lb.entries) >= lb.maxSize {
		lb.entries = lb.entries[1:]
	}
	lb.entries = append(lb.entries, entry)
}

func (lb *LogBuffer) GetAll() []LogEntry {
	lb.mu.RLock()
	defer lb.mu.RUnlock()
	result := make([]LogEntry, len(lb.entries))
	copy(result, lb.entries)
	return result
}

func (lb *LogBuffer) GetSince(since time.Time) []LogEntry {
	lb.mu.RLock()
	defer lb.mu.RUnlock()
	var result []LogEntry
	for _, entry := range lb.entries {
		if entry.Timestamp.After(since) {
			result = append(result, entry)
		}
	}
	return result
}

var serverLogs = NewLogBuffer(500)

func serverLog(level, format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	log.Print(msg)
	serverLogs.Add(level, msg)
}

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

type RocketConnection struct {
	ID         string
	Conn       *websocket.Conn
	Config     protocol.RocketConfig
	State      protocol.RocketState
	LastUpdate time.Time
	mu         sync.RWMutex
}

type ObserverConnection struct {
	ID         string
	Conn       *websocket.Conn
	LastUpdate time.Time
	mu         sync.RWMutex
}

type Server struct {
	rockets                map[string]*RocketConnection
	observers              map[string]*ObserverConnection
	mu                     sync.RWMutex
	collisionCheckInterval time.Duration
	minSafeDistance        float64
}

func NewServer() *Server {
	return &Server{
		rockets:                make(map[string]*RocketConnection),
		observers:              make(map[string]*ObserverConnection),
		collisionCheckInterval: 1 * time.Second,
		minSafeDistance:        1000.0,
	}
}

func (s *Server) Start(port string) error {

	go s.collisionCheckLoop()

	http.HandleFunc("/ws", s.handleWebSocket)
	http.HandleFunc("/rockets", s.handleRocketList)
	http.HandleFunc("/", s.handleIndex)

	http.HandleFunc("/api/logs", s.handleLogs)

	addr := ":" + port
	serverLog("info", "Сервер запущен на %s", addr)
	return http.ListenAndServe(addr, nil)
}

func (s *Server) handleWebSocket(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		serverLog("error", "Ошибка при обновлении до WebSocket: %v", err)
		return
	}

	serverLog("info", "Новое подключение от %s", conn.RemoteAddr())

	go s.handleClient(conn)
}

func (s *Server) handleClient(conn *websocket.Conn) {
	defer conn.Close()

	var rocketConn *RocketConnection
	var observerConn *ObserverConnection

	for {
		_, msgBytes, err := conn.ReadMessage()
		if err != nil {
			if rocketConn != nil {
				serverLog("warning", "Ракета %s отключилась: %v", rocketConn.ID, err)
				s.removeRocket(rocketConn.ID)
			}
			if observerConn != nil {
				serverLog("info", "Наблюдатель %s отключился: %v", observerConn.ID, err)
				s.removeObserver(observerConn.ID)
			}
			break
		}

		var msg protocol.Message
		if err := json.Unmarshal(msgBytes, &msg); err != nil {
			serverLog("error", "Ошибка декодирования сообщения: %v", err)
			continue
		}

		switch msg.Type {
		case protocol.MsgTypeRegister:
			rocketConn = s.handleRegister(conn, msg)

		case protocol.MsgTypeTelemetry:
			if rocketConn != nil {
				s.handleTelemetry(rocketConn, msg)
			}

		case protocol.MsgTypeDisconnect:
			if rocketConn != nil {
				serverLog("info", "Ракета %s запросила отключение", rocketConn.ID)
				s.removeRocket(rocketConn.ID)
				return
			}

		case protocol.MsgTypeSubscribe:
			observerConn = s.handleSubscribe(conn, msg)

		case protocol.MsgTypeUnsubscribe:
			if observerConn != nil {
				log.Printf("Наблюдатель %s отписался", observerConn.ID)
				s.removeObserver(observerConn.ID)
				return
			}
		}
	}
}

func (s *Server) handleRegister(conn *websocket.Conn, msg protocol.Message) *RocketConnection {
	data, _ := json.Marshal(msg.Data)
	var registerMsg protocol.RegisterMessage
	if err := json.Unmarshal(data, &registerMsg); err != nil {
		serverLog("error", "Ошибка декодирования регистрации: %v", err)
		return nil
	}

	if err := protocol.ValidateRocketConfig(&registerMsg.Config); err != nil {
		s.sendMessage(conn, protocol.MsgTypeRejected, protocol.RejectedMessage{
			RocketID: registerMsg.RocketID,
			Reason:   err.Error(),
		})
		return nil
	}

	s.mu.RLock()
	_, exists := s.rockets[registerMsg.RocketID]
	s.mu.RUnlock()

	if exists {
		s.sendMessage(conn, protocol.MsgTypeRejected, protocol.RejectedMessage{
			RocketID: registerMsg.RocketID,
			Reason:   "ракета с таким ID уже зарегистрирована",
		})
		return nil
	}

	rocketConn := &RocketConnection{
		ID:         registerMsg.RocketID,
		Conn:       conn,
		Config:     registerMsg.Config,
		LastUpdate: time.Now(),
	}

	s.mu.Lock()
	s.rockets[registerMsg.RocketID] = rocketConn
	s.mu.Unlock()

	s.sendMessage(conn, protocol.MsgTypeAccepted, protocol.AcceptedMessage{
		RocketID: registerMsg.RocketID,
		Message:  "Регистрация успешна. Вы можете начинать запуск.",
	})

	s.broadcastToObservers(protocol.MsgTypeRocketJoined, protocol.RocketJoinedMessage{
		RocketID: registerMsg.RocketID,
		Name:     registerMsg.Config.Name,
		Config:   registerMsg.Config,
	})

	serverLog("info", "Ракета %s (%s) зарегистрирована", registerMsg.RocketID, registerMsg.Config.Name)

	return rocketConn
}

func (s *Server) handleTelemetry(rocketConn *RocketConnection, msg protocol.Message) {
	data, _ := json.Marshal(msg.Data)
	var telemetryMsg protocol.TelemetryMessage
	if err := json.Unmarshal(data, &telemetryMsg); err != nil {
		serverLog("error", "Ошибка декодирования телеметрии: %v", err)
		return
	}

	rocketConn.mu.Lock()
	rocketConn.State = telemetryMsg.State
	rocketConn.LastUpdate = time.Now()
	rocketName := rocketConn.Config.Name
	rocketConn.mu.Unlock()

	s.broadcastToObservers(protocol.MsgTypeBroadcast, protocol.BroadcastMessage{
		RocketID: rocketConn.ID,
		Name:     rocketName,
		State:    telemetryMsg.State,
	})

	if int(telemetryMsg.State.Time)%10 == 0 {
		serverLog("info", "Ракета %s: высота=%.2f км, скорость=%.1f м/с, топливо=%.0f кг",
			rocketConn.ID,
			telemetryMsg.State.Altitude/1000.0,
			telemetryMsg.State.Speed,
			telemetryMsg.State.FuelRemaining)
	}
}

func (s *Server) removeRocket(rocketID string) {
	s.mu.Lock()
	rocket, exists := s.rockets[rocketID]
	delete(s.rockets, rocketID)
	s.mu.Unlock()

	if exists {
		s.broadcastToObservers(protocol.MsgTypeRocketLeft, protocol.RocketLeftMessage{
			RocketID: rocketID,
			Reason:   "disconnected",
		})
		serverLog("info", "Ракета %s (%s) удалена из списка", rocketID, rocket.Config.Name)
	}
}

func (s *Server) handleSubscribe(conn *websocket.Conn, msg protocol.Message) *ObserverConnection {
	data, _ := json.Marshal(msg.Data)
	var subscribeMsg protocol.SubscribeMessage
	if err := json.Unmarshal(data, &subscribeMsg); err != nil {
		serverLog("error", "Ошибка декодирования подписки: %v", err)
		return nil
	}

	observerConn := &ObserverConnection{
		ID:         subscribeMsg.ObserverID,
		Conn:       conn,
		LastUpdate: time.Now(),
	}

	s.mu.Lock()
	s.observers[subscribeMsg.ObserverID] = observerConn
	s.mu.Unlock()

	s.sendCurrentRocketsToObserver(observerConn)

	serverLog("info", "Наблюдатель %s подписался на события", subscribeMsg.ObserverID)
	return observerConn
}

func (s *Server) removeObserver(observerID string) {
	s.mu.Lock()
	delete(s.observers, observerID)
	s.mu.Unlock()
	serverLog("info", "Наблюдатель %s удален из списка", observerID)
}

func (s *Server) sendCurrentRocketsToObserver(observer *ObserverConnection) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	for _, rocket := range s.rockets {
		rocket.mu.RLock()
		s.sendMessage(observer.Conn, protocol.MsgTypeRocketJoined, protocol.RocketJoinedMessage{
			RocketID: rocket.ID,
			Name:     rocket.Config.Name,
			Config:   rocket.Config,
		})
		s.sendMessage(observer.Conn, protocol.MsgTypeBroadcast, protocol.BroadcastMessage{
			RocketID: rocket.ID,
			Name:     rocket.Config.Name,
			State:    rocket.State,
		})
		rocket.mu.RUnlock()
	}
}

func (s *Server) broadcastToObservers(msgType protocol.MessageType, data interface{}) {
	s.mu.RLock()
	observers := make([]*ObserverConnection, 0, len(s.observers))
	for _, obs := range s.observers {
		observers = append(observers, obs)
	}
	s.mu.RUnlock()

	for _, obs := range observers {
		obs.mu.Lock()
		s.sendMessage(obs.Conn, msgType, data)
		obs.mu.Unlock()
	}
}

func (s *Server) collisionCheckLoop() {
	ticker := time.NewTicker(s.collisionCheckInterval)
	defer ticker.Stop()

	for range ticker.C {
		s.checkCollisions()
	}
}

func (s *Server) checkCollisions() {
	s.mu.RLock()
	rockets := make([]*RocketConnection, 0, len(s.rockets))
	for _, rocket := range s.rockets {
		rockets = append(rockets, rocket)
	}
	s.mu.RUnlock()

	for i := 0; i < len(rockets); i++ {
		for j := i + 1; j < len(rockets); j++ {
			rocket1 := rockets[i]
			rocket2 := rockets[j]

			rocket1.mu.RLock()
			rocket2.mu.RLock()

			distance := calculateDistance(rocket1.State.Position, rocket2.State.Position)

			if distance < s.minSafeDistance {
				severity := "medium"
				if distance < s.minSafeDistance/2 {
					severity = "high"
				}
				if distance < s.minSafeDistance/4 {
					severity = "critical"
				}

				warning := fmt.Sprintf("Опасное сближение с ракетой %s! Расстояние: %.1f м", rocket2.ID, distance)
				s.sendMessage(rocket1.Conn, protocol.MsgTypeWarning, protocol.WarningMessage{
					RocketID: rocket1.ID,
					Warning:  warning,
					Severity: severity,
				})

				warning = fmt.Sprintf("Опасное сближение с ракетой %s! Расстояние: %.1f м", rocket1.ID, distance)
				s.sendMessage(rocket2.Conn, protocol.MsgTypeWarning, protocol.WarningMessage{
					RocketID: rocket2.ID,
					Warning:  warning,
					Severity: severity,
				})

				serverLog("warning", "Предупреждение: ракеты %s и %s на расстоянии %.1f м", rocket1.ID, rocket2.ID, distance)
			}

			rocket1.mu.RUnlock()
			rocket2.mu.RUnlock()
		}
	}
}

func calculateDistance(p1, p2 protocol.Vector3) float64 {
	dx := p1.X - p2.X
	dy := p1.Y - p2.Y
	dz := p1.Z - p2.Z
	return math.Sqrt(dx*dx + dy*dy + dz*dz)
}

func (s *Server) sendMessage(conn *websocket.Conn, msgType protocol.MessageType, data interface{}) {
	msg := protocol.Message{
		Type:      msgType,
		Timestamp: time.Now(),
		Data:      data,
	}

	if err := conn.WriteJSON(msg); err != nil {
		serverLog("error", "Ошибка отправки сообщения: %v", err)
	}
}

func (s *Server) handleRocketList(w http.ResponseWriter, r *http.Request) {
	s.mu.RLock()
	rockets := make([]protocol.RocketInfo, 0, len(s.rockets))
	for _, rocket := range s.rockets {
		rocket.mu.RLock()
		rockets = append(rockets, protocol.RocketInfo{
			RocketID: rocket.ID,
			Name:     rocket.Config.Name,
			State:    rocket.State,
			Config:   rocket.Config,
		})
		rocket.mu.RUnlock()
	}
	s.mu.RUnlock()

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(rockets)
}

func (s *Server) handleLogs(w http.ResponseWriter, r *http.Request) {
	sinceStr := r.URL.Query().Get("since")
	var logs []LogEntry
	if sinceStr != "" {
		since, err := time.Parse(time.RFC3339Nano, sinceStr)
		if err == nil {
			logs = serverLogs.GetSince(since)
		} else {
			logs = serverLogs.GetAll()
		}
	} else {
		logs = serverLogs.GetAll()
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(logs)
}

func (s *Server) handleIndex(w http.ResponseWriter, r *http.Request) {
	html := `<!DOCTYPE html>
<html lang="ru">
<head>
    <title>Cosmodrom - Центр управления</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Courier New', monospace;
            background: #0a0e17;
            color: #c8d6e5;
            height: 100vh;
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #0d1b2a, #1b2838);
            border-bottom: 1px solid #1e3a5f;
            padding: 12px 24px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }
        .header h1 {
            font-size: 18px;
            color: #4fc3f7;
            letter-spacing: 2px;
            text-transform: uppercase;
        }
        .header .status {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 12px;
        }
        .header .status .dot {
            width: 8px; height: 8px;
            border-radius: 50%;
            background: #4caf50;
            animation: pulse 2s infinite;
        }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.4; }
        }
        .container {
            display: flex;
            height: calc(100vh - 50px);
        }
        .sidebar {
            width: 280px;
            min-width: 280px;
            background: #0d1117;
            border-right: 1px solid #1e3a5f;
            display: flex;
            flex-direction: column;
        }
        .sidebar-header {
            padding: 12px 16px;
            border-bottom: 1px solid #1e3a5f;
            font-size: 11px;
            color: #4fc3f7;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .rocket-list {
            flex: 1;
            overflow-y: auto;
            padding: 8px;
        }
        .rocket-item {
            padding: 10px 12px;
            border-radius: 6px;
            margin-bottom: 4px;
            cursor: pointer;
            transition: background 0.2s;
            border: 1px solid transparent;
        }
        .rocket-item:hover {
            background: #161b22;
            border-color: #1e3a5f;
        }
        .rocket-item.selected {
            background: #1a2332;
            border-color: #4fc3f7;
        }
        .rocket-item .name {
            font-size: 13px;
            font-weight: bold;
            color: #e6edf3;
            margin-bottom: 4px;
        }
        .rocket-item .id {
            font-size: 10px;
            color: #6e7681;
        }
        .rocket-item .mini-stats {
            display: flex;
            gap: 12px;
            margin-top: 6px;
            font-size: 10px;
        }
        .rocket-item .mini-stats span {
            color: #8b949e;
        }
        .rocket-item .mini-stats .val {
            color: #58a6ff;
        }
        .status-badge {
            display: inline-block;
            padding: 1px 6px;
            border-radius: 3px;
            font-size: 9px;
            font-weight: bold;
            text-transform: uppercase;
            margin-left: 6px;
        }
        .status-flight { background: #1a4a2e; color: #4caf50; }
        .status-orbit { background: #1a3a4a; color: #4fc3f7; }
        .status-landed { background: #3a3a1a; color: #ffb74d; }
        .status-crashed { background: #4a1a1a; color: #ef5350; }
        .main-content {
            flex: 1;
            display: flex;
            flex-direction: column;
            overflow: hidden;
        }
        .tabs {
            display: flex;
            background: #0d1117;
            border-bottom: 1px solid #1e3a5f;
        }
        .tab {
            padding: 10px 20px;
            font-size: 12px;
            color: #8b949e;
            cursor: pointer;
            border-bottom: 2px solid transparent;
            transition: all 0.2s;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .tab:hover { color: #c8d6e5; }
        .tab.active {
            color: #4fc3f7;
            border-bottom-color: #4fc3f7;
        }
        .tab-content {
            flex: 1;
            overflow: hidden;
            display: none;
        }
        .tab-content.active { display: flex; flex-direction: column; }

        /* Telemetry panel */
        .telemetry-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 12px;
            padding: 16px;
            overflow-y: auto;
        }
        .telemetry-card {
            background: #161b22;
            border: 1px solid #1e3a5f;
            border-radius: 8px;
            padding: 14px;
        }
        .telemetry-card .label {
            font-size: 10px;
            color: #6e7681;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-bottom: 6px;
        }
        .telemetry-card .value {
            font-size: 24px;
            font-weight: bold;
            color: #4fc3f7;
        }
        .telemetry-card .unit {
            font-size: 12px;
            color: #6e7681;
            margin-left: 4px;
        }
        .telemetry-card.wide {
            grid-column: span 3;
        }
        .fuel-bar-container {
            width: 100%;
            height: 8px;
            background: #21262d;
            border-radius: 4px;
            margin-top: 8px;
            overflow: hidden;
        }
        .fuel-bar {
            height: 100%;
            border-radius: 4px;
            transition: width 0.3s;
            background: linear-gradient(90deg, #ef5350, #ffb74d, #4caf50);
        }
        .no-rocket-selected {
            display: flex;
            align-items: center;
            justify-content: center;
            height: 100%;
            color: #6e7681;
            font-size: 14px;
        }

        /* Logs panel */
        .log-container {
            flex: 1;
            overflow-y: auto;
            padding: 12px 16px;
            font-size: 12px;
            line-height: 1.8;
        }
        .log-entry {
            padding: 2px 0;
            border-bottom: 1px solid #161b22;
            display: flex;
            gap: 12px;
        }
        .log-entry .log-time {
            color: #6e7681;
            white-space: nowrap;
            min-width: 80px;
        }
        .log-entry .log-level {
            font-weight: bold;
            min-width: 60px;
            text-transform: uppercase;
            font-size: 10px;
            padding-top: 2px;
        }
        .log-level.info { color: #4fc3f7; }
        .log-level.warning { color: #ffb74d; }
        .log-level.error { color: #ef5350; }
        .log-entry .log-msg { color: #c8d6e5; }

        .server-tab-label { position: relative; }

        ::-webkit-scrollbar { width: 6px; }
        ::-webkit-scrollbar-track { background: #0d1117; }
        ::-webkit-scrollbar-thumb { background: #1e3a5f; border-radius: 3px; }
        ::-webkit-scrollbar-thumb:hover { background: #2a4a6f; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Cosmodrom - Центр управления</h1>
        <div class="status">
            <div class="dot" id="ws-dot"></div>
            <span id="ws-status">Подключение...</span>
            <span style="margin-left: 16px; color: #6e7681;">Ракет: <span id="rocket-count" style="color: #4fc3f7;">0</span></span>
        </div>
    </div>
    <div class="container">
        <div class="sidebar">
            <div class="sidebar-header">Активные ракеты</div>
            <div class="rocket-list" id="rocket-list">
                <div style="padding: 20px; color: #6e7681; text-align: center; font-size: 12px;">
                    Нет активных ракет
                </div>
            </div>
        </div>
        <div class="main-content">
            <div class="tabs">
                <div class="tab active" data-tab="telemetry">Телеметрия</div>
                <div class="tab server-tab-label" data-tab="logs">Логи сервера</div>
            </div>
            <div class="tab-content active" id="tab-telemetry">
                <div class="no-rocket-selected" id="no-rocket-msg">
                    Выберите ракету из списка слева
                </div>
                <div class="telemetry-grid" id="telemetry-grid" style="display: none;">
                    <div class="telemetry-card">
                        <div class="label">Высота</div>
                        <div><span class="value" id="t-altitude">0.00</span><span class="unit">км</span></div>
                    </div>
                    <div class="telemetry-card">
                        <div class="label">Скорость</div>
                        <div><span class="value" id="t-speed">0.0</span><span class="unit">м/с</span></div>
                    </div>
                    <div class="telemetry-card">
                        <div class="label">Ускорение</div>
                        <div><span class="value" id="t-accel">0.0</span><span class="unit">G</span></div>
                    </div>
                    <div class="telemetry-card">
                        <div class="label">Масса</div>
                        <div><span class="value" id="t-mass">0</span><span class="unit">кг</span></div>
                    </div>
                    <div class="telemetry-card">
                        <div class="label">Время полёта</div>
                        <div><span class="value" id="t-time">0</span><span class="unit">с</span></div>
                    </div>
                    <div class="telemetry-card">
                        <div class="label">Статус</div>
                        <div><span class="value" id="t-status" style="font-size: 16px;">-</span></div>
                    </div>
                    <div class="telemetry-card wide">
                        <div class="label">Топливо (<span id="t-fuel-pct">0</span>%)</div>
                        <div><span class="value" id="t-fuel" style="font-size: 18px;">0</span><span class="unit">кг</span></div>
                        <div class="fuel-bar-container">
                            <div class="fuel-bar" id="t-fuel-bar" style="width: 0%"></div>
                        </div>
                    </div>
                    <div class="telemetry-card">
                        <div class="label">Позиция X</div>
                        <div><span class="value" id="t-px" style="font-size: 14px;">0</span><span class="unit">м</span></div>
                    </div>
                    <div class="telemetry-card">
                        <div class="label">Позиция Y</div>
                        <div><span class="value" id="t-py" style="font-size: 14px;">0</span><span class="unit">м</span></div>
                    </div>
                    <div class="telemetry-card">
                        <div class="label">Позиция Z</div>
                        <div><span class="value" id="t-pz" style="font-size: 14px;">0</span><span class="unit">м</span></div>
                    </div>
                </div>
            </div>
            <div class="tab-content" id="tab-logs">
                <div class="log-container" id="log-container"></div>
            </div>
        </div>
    </div>

    <script>
        const rockets = {};
        let selectedRocketId = null;
        let ws = null;
        let logPollTimer = null;
        let lastLogTime = null;

        function connectWS() {
            const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
            ws = new WebSocket(protocol + '//' + location.host + '/ws');

            ws.onopen = () => {
                document.getElementById('ws-dot').style.background = '#4caf50';
                document.getElementById('ws-status').textContent = 'Подключено';
                ws.send(JSON.stringify({
                    type: 'subscribe',
                    timestamp: new Date().toISOString(),
                    data: { observer_id: 'web-dashboard-' + Math.random().toString(36).substr(2, 6) }
                }));
            };

            ws.onclose = () => {
                document.getElementById('ws-dot').style.background = '#ef5350';
                document.getElementById('ws-status').textContent = 'Отключено';
                setTimeout(connectWS, 3000);
            };

            ws.onerror = () => {
                document.getElementById('ws-dot').style.background = '#ef5350';
                document.getElementById('ws-status').textContent = 'Ошибка';
            };

            ws.onmessage = (event) => {
                const msg = JSON.parse(event.data);
                handleMessage(msg);
            };
        }

        function handleMessage(msg) {
            switch (msg.type) {
                case 'rocket_joined':
                    rockets[msg.data.rocket_id] = {
                        id: msg.data.rocket_id,
                        name: msg.data.name,
                        config: msg.data.config,
                        state: null
                    };
                    renderRocketList();
                    break;

                case 'broadcast':
                    if (rockets[msg.data.rocket_id]) {
                        rockets[msg.data.rocket_id].state = msg.data.state;
                        rockets[msg.data.rocket_id].name = msg.data.name;
                    } else {
                        rockets[msg.data.rocket_id] = {
                            id: msg.data.rocket_id,
                            name: msg.data.name,
                            config: null,
                            state: msg.data.state
                        };
                    }
                    renderRocketList();
                    if (msg.data.rocket_id === selectedRocketId) {
                        renderTelemetry(rockets[msg.data.rocket_id]);
                    }
                    break;

                case 'rocket_left':
                    delete rockets[msg.data.rocket_id];
                    if (msg.data.rocket_id === selectedRocketId) {
                        selectedRocketId = null;
                        document.getElementById('no-rocket-msg').style.display = 'flex';
                        document.getElementById('telemetry-grid').style.display = 'none';
                    }
                    renderRocketList();
                    break;

                case 'warning':
                    break;
            }
            document.getElementById('rocket-count').textContent = Object.keys(rockets).length;
        }

        function getStatusInfo(state) {
            if (!state) return { text: 'ОЖИДАНИЕ', cls: 'flight' };
            if (state.crashed) return { text: 'КРУШЕНИЕ', cls: 'crashed' };
            if (state.landed) return { text: 'ПОСАДКА', cls: 'landed' };
            if (state.in_orbit) return { text: 'ОРБИТА', cls: 'orbit' };
            return { text: 'ПОЛЁТ', cls: 'flight' };
        }

        function renderRocketList() {
            const list = document.getElementById('rocket-list');
            const ids = Object.keys(rockets);
            if (ids.length === 0) {
                list.innerHTML = '<div style="padding: 20px; color: #6e7681; text-align: center; font-size: 12px;">Нет активных ракет</div>';
                return;
            }

            list.innerHTML = ids.map(id => {
                const r = rockets[id];
                const st = getStatusInfo(r.state);
                const alt = r.state ? (r.state.altitude / 1000).toFixed(1) : '0.0';
                const spd = r.state ? r.state.speed.toFixed(0) : '0';
                const sel = id === selectedRocketId ? 'selected' : '';
                return '<div class="rocket-item ' + sel + '" onclick="selectRocket(\'' + id + '\')">' +
                    '<div class="name">' + escapeHtml(r.name) +
                    '<span class="status-badge status-' + st.cls + '">' + st.text + '</span></div>' +
                    '<div class="id">' + escapeHtml(id) + '</div>' +
                    '<div class="mini-stats"><span>ALT: <span class="val">' + alt + ' км</span></span>' +
                    '<span>SPD: <span class="val">' + spd + ' м/с</span></span></div></div>';
            }).join('');
        }

        function selectRocket(id) {
            selectedRocketId = id;
            document.getElementById('no-rocket-msg').style.display = 'none';
            document.getElementById('telemetry-grid').style.display = 'grid';
            renderRocketList();
            if (rockets[id]) renderTelemetry(rockets[id]);
        }

        function renderTelemetry(rocket) {
            const s = rocket.state;
            if (!s) return;

            document.getElementById('t-altitude').textContent = (s.altitude / 1000).toFixed(2);
            document.getElementById('t-speed').textContent = s.speed.toFixed(1);

            const accelMag = Math.sqrt(
                s.acceleration.x * s.acceleration.x +
                s.acceleration.y * s.acceleration.y +
                s.acceleration.z * s.acceleration.z
            );
            document.getElementById('t-accel').textContent = (accelMag / 9.81).toFixed(2);
            document.getElementById('t-mass').textContent = s.mass_current.toFixed(0);
            document.getElementById('t-time').textContent = s.time.toFixed(1);

            const st = getStatusInfo(s);
            const statusEl = document.getElementById('t-status');
            statusEl.textContent = st.text;
            statusEl.className = 'value status-badge status-' + st.cls;
            statusEl.style.fontSize = '16px';

            document.getElementById('t-fuel').textContent = s.fuel_remaining.toFixed(0);
            const maxFuel = rocket.config ? rocket.config.mass_fuel_max : s.fuel_remaining;
            const pct = maxFuel > 0 ? (s.fuel_remaining / maxFuel * 100) : 0;
            document.getElementById('t-fuel-pct').textContent = pct.toFixed(1);
            document.getElementById('t-fuel-bar').style.width = pct + '%';

            document.getElementById('t-px').textContent = s.position.x.toFixed(0);
            document.getElementById('t-py').textContent = s.position.y.toFixed(0);
            document.getElementById('t-pz').textContent = s.position.z.toFixed(0);
        }

        function pollLogs() {
            let url = '/api/logs';
            if (lastLogTime) {
                url += '?since=' + encodeURIComponent(lastLogTime);
            }
            fetch(url)
                .then(r => r.json())
                .then(logs => {
                    if (!logs || logs.length === 0) return;
                    const container = document.getElementById('log-container');
                    logs.forEach(entry => {
                        const div = document.createElement('div');
                        div.className = 'log-entry';
                        const t = new Date(entry.timestamp);
                        const timeStr = t.toLocaleTimeString('ru-RU');
                        div.innerHTML =
                            '<span class="log-time">' + timeStr + '</span>' +
                            '<span class="log-level ' + entry.level + '">' + entry.level + '</span>' +
                            '<span class="log-msg">' + escapeHtml(entry.message) + '</span>';
                        container.appendChild(div);
                        lastLogTime = entry.timestamp;
                    });
                    container.scrollTop = container.scrollHeight;
                })
                .catch(() => {});
        }

        function escapeHtml(str) {
            const div = document.createElement('div');
            div.textContent = str;
            return div.innerHTML;
        }

        // Tabs
        document.querySelectorAll('.tab').forEach(tab => {
            tab.addEventListener('click', () => {
                document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
                document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
                tab.classList.add('active');
                document.getElementById('tab-' + tab.dataset.tab).classList.add('active');
            });
        });

        connectWS();
        pollLogs();
        logPollTimer = setInterval(pollLogs, 2000);
    </script>
</body>
</html>`
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Write([]byte(html))
}

func main() {
	port := flag.String("port", "8080", "Порт для сервера")
	flag.Parse()

	server := NewServer()
	log.Fatal(server.Start(*port))
}
