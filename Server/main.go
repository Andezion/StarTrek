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

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		return true // В будущем нужна более строгая проверка
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

	addr := ":" + port
	log.Printf("Сервер запущен на %s", addr)
	return http.ListenAndServe(addr, nil)
}

func (s *Server) handleWebSocket(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("Ошибка при обновлении до WebSocket: %v", err)
		return
	}

	log.Printf("Новое подключение от %s", conn.RemoteAddr())

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
				log.Printf("Ракета %s отключилась: %v", rocketConn.ID, err)
				s.removeRocket(rocketConn.ID)
			}
			if observerConn != nil {
				log.Printf("Наблюдатель %s отключился: %v", observerConn.ID, err)
				s.removeObserver(observerConn.ID)
			}
			break
		}

		var msg protocol.Message
		if err := json.Unmarshal(msgBytes, &msg); err != nil {
			log.Printf("Ошибка декодирования сообщения: %v", err)
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
				log.Printf("Ракета %s запросила отключение", rocketConn.ID)
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
		log.Printf("Ошибка декодирования регистрации: %v", err)
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

	log.Printf("Ракета %s (%s) зарегистрирована", registerMsg.RocketID, registerMsg.Config.Name)

	return rocketConn
}

func (s *Server) handleTelemetry(rocketConn *RocketConnection, msg protocol.Message) {
	data, _ := json.Marshal(msg.Data)
	var telemetryMsg protocol.TelemetryMessage
	if err := json.Unmarshal(data, &telemetryMsg); err != nil {
		log.Printf("Ошибка декодирования телеметрии: %v", err)
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
		log.Printf("Ракета %s: высота=%.2f км, скорость=%.1f м/с, топливо=%.0f кг",
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
		log.Printf("Ракета %s (%s) удалена из списка", rocketID, rocket.Config.Name)
	}
}

func (s *Server) handleSubscribe(conn *websocket.Conn, msg protocol.Message) *ObserverConnection {
	data, _ := json.Marshal(msg.Data)
	var subscribeMsg protocol.SubscribeMessage
	if err := json.Unmarshal(data, &subscribeMsg); err != nil {
		log.Printf("Ошибка декодирования подписки: %v", err)
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

	log.Printf("Наблюдатель %s подписался на события", subscribeMsg.ObserverID)
	return observerConn
}

func (s *Server) removeObserver(observerID string) {
	s.mu.Lock()
	delete(s.observers, observerID)
	s.mu.Unlock()
	log.Printf("Наблюдатель %s удален из списка", observerID)
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

				log.Printf("Предупреждение: ракеты %s и %s на расстоянии %.1f м", rocket1.ID, rocket2.ID, distance)
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
		log.Printf("Ошибка отправки сообщения: %v", err)
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

func (s *Server) handleIndex(w http.ResponseWriter, r *http.Request) {
	html := `
<!DOCTYPE html>
<html>
<head>
    <title>Cosmodrom - Сервер координации ракет</title>
    <meta charset="UTF-8">
</head>
<body>
    <h1>Cosmodrom - Сервер координации ракет</h1>
    <p>Сервер работает и готов к приему подключений.</p>
    <ul>
        <li><a href="/rockets">Список активных ракет (JSON)</a></li>
        <li>WebSocket endpoint: ws://localhost:8080/ws</li>
    </ul>
</body>
</html>
`
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Write([]byte(html))
}

func main() {
	port := flag.String("port", "8080", "Порт для сервера")
	flag.Parse()

	server := NewServer()
	log.Fatal(server.Start(*port))
}
