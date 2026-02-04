package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"math/rand"
	"os"
	"os/signal"
	"time"

	"cosmodrom/client/physics"
	"cosmodrom/client/protocol"

	"github.com/gorilla/websocket"
)

type RocketClient struct {
	ID          string
	config      protocol.RocketConfig
	physics     *physics.RocketPhysics
	conn        *websocket.Conn
	serverURL   string
	command     protocol.ControlCommand
	registered  bool
	running     bool
	telemetryHz float64
}

func NewRocketClient(id string, config protocol.RocketConfig, serverURL string) *RocketClient {
	return &RocketClient{
		ID:          id,
		config:      config,
		serverURL:   serverURL,
		telemetryHz: 10.0,
		running:     true,
	}
}

func (r *RocketClient) Connect() error {
	var err error
	r.conn, _, err = websocket.DefaultDialer.Dial(r.serverURL, nil)
	if err != nil {
		return fmt.Errorf("Ошибка подключения к серверу: %w", err)
	}

	log.Printf("Подключено к серверу %s", r.serverURL)
	return nil
}

func (r *RocketClient) Register() error {
	msg := protocol.Message{
		Type:      protocol.MsgTypeRegister,
		Timestamp: time.Now(),
		Data: protocol.RegisterMessage{
			RocketID: r.ID,
			Config:   r.config,
		},
	}

	if err := r.conn.WriteJSON(msg); err != nil {
		return fmt.Errorf("Ошибка отправки регистрации: %w", err)
	}

	var response protocol.Message
	if err := r.conn.ReadJSON(&response); err != nil {
		return fmt.Errorf("Ошибка чтения ответа: %w", err)
	}

	switch response.Type {
	case protocol.MsgTypeAccepted:
		data, _ := json.Marshal(response.Data)
		var acceptedMsg protocol.AcceptedMessage
		json.Unmarshal(data, &acceptedMsg)
		log.Printf("Регистрация принята: %s", acceptedMsg.Message)
		r.registered = true
		return nil

	case protocol.MsgTypeRejected:
		data, _ := json.Marshal(response.Data)
		var rejectedMsg protocol.RejectedMessage
		json.Unmarshal(data, &rejectedMsg)
		return fmt.Errorf("Регистрация отклонена: %s", rejectedMsg.Reason)

	default:
		return fmt.Errorf("Неожиданный ответ от сервера: %s", response.Type)
	}
}

func (r *RocketClient) InitPhysics(latitude, longitude, altitude float64) error {
	initialPos := physics.SphericalToCartesian(latitude, longitude, altitude)

	var err error
	r.physics, err = physics.NewRocketPhysics(&r.config, initialPos)
	if err != nil {
		return fmt.Errorf("Ошибка инициализации физики: %w", err)
	}

	r.command = protocol.ControlCommand{
		EngineThrottle: make([]float64, len(r.config.Engines)),
		Pitch:          0.0,
		Yaw:            0.0,
		Roll:           0.0,
	}

	for i := range r.command.EngineThrottle {
		r.command.EngineThrottle[i] = 1.0
	}

	log.Printf("Физический движок инициализирован")
	return nil
}

func (r *RocketClient) Run() {
	defer r.physics.Free()

	go r.receiveMessages()

	dt := 0.01
	telemetryInterval := 1.0 / r.telemetryHz
	lastTelemetry := time.Now()

	ticker := time.NewTicker(time.Duration(dt * float64(time.Second)))
	defer ticker.Stop()

	log.Printf("Запуск симуляции ракеты %s", r.ID)
	log.Printf("Конфигурация: %s, двигатели: %d x %.0f кН",
		r.config.Name,
		len(r.config.Engines),
		r.config.Engines[0].Thrust/1000.0)

	for r.running {
		<-ticker.C

		state := r.physics.GetState()
		alt := state.Altitude

		if alt < 500.0 {
			r.command.Pitch = 0.0
		} else if alt < 600.0 {
			r.command.Pitch = (alt - 500.0) / (600.0 - 500.0) * 25.0 // 0° → 25°
		} else if alt < 700.0 {
			r.command.Pitch = 25.0 + (alt-600.0)/(700.0-600.0)*35.0 // 25° → 60°
		} else if alt < 800.0 {
			r.command.Pitch = 60.0 + (alt-700.0)/(800.0-700.0)*20.0 // 60° → 80°
		} else if alt < 900.0 {
			r.command.Pitch = 80.0 + (alt-800.0)/(900.0-800.0)*10.0 // 80° → 90°
		} else {
			r.command.Pitch = 90.0 // Полностью горизонтально
		}

		r.physics.Update(&r.command, dt)

		state = r.physics.GetState()

		if state.FuelRemaining <= 0 {
			for i := range r.command.EngineThrottle {
				r.command.EngineThrottle[i] = 0.0
			}
		}

		if time.Since(lastTelemetry).Seconds() >= telemetryInterval {
			if err := r.sendTelemetry(state); err != nil {
				log.Printf("Ошибка отправки телеметрии: %v", err)
			}
			lastTelemetry = time.Now()
		}

		if state.Landed {
			log.Printf("Ракета %s успешно приземлилась", r.ID)
			log.Printf("Конечная высота: %.2f м, скорость: %.1f м/с", state.Altitude, state.Speed)
			r.running = false
		}

		if state.Crashed {
			log.Printf("Ракета %s разбилась", r.ID)
			log.Printf("Конечная высота: %.2f м, скорость: %.1f м/с", state.Altitude, state.Speed)
			r.running = false
		}

		if state.InOrbit {
			log.Printf("Ракета %s вышла на орбиту!", r.ID)
			log.Printf("Высота: %.2f км, скорость: %.1f м/с, топливо: %.0f кг",
				state.Altitude/1000.0, state.Speed, state.FuelRemaining)
		}
	}

	r.disconnect()
}

func (r *RocketClient) sendTelemetry(state protocol.RocketState) error {
	if !r.registered {
		return nil
	}

	msg := protocol.Message{
		Type:      protocol.MsgTypeTelemetry,
		Timestamp: time.Now(),
		Data: protocol.TelemetryMessage{
			RocketID: r.ID,
			State:    state,
		},
	}

	return r.conn.WriteJSON(msg)
}

func (r *RocketClient) receiveMessages() {
	for r.running {
		var msg protocol.Message
		if err := r.conn.ReadJSON(&msg); err != nil {
			if r.running {
				log.Printf("Ошибка чтения сообщения: %v", err)
			}
			return
		}

		switch msg.Type {
		case protocol.MsgTypeCommand:
			r.handleCommand(msg)

		case protocol.MsgTypeWarning:
			r.handleWarning(msg)

		case protocol.MsgTypeShutdown:
			log.Printf("Получена команда на выключение от сервера")
			r.running = false
		}
	}
}

func (r *RocketClient) handleCommand(msg protocol.Message) {
	data, _ := json.Marshal(msg.Data)
	var commandMsg protocol.CommandMessage
	if err := json.Unmarshal(data, &commandMsg); err != nil {
		log.Printf("Ошибка декодирования команды: %v", err)
		return
	}

	r.command = commandMsg.Command
	log.Printf("Получена команда управления от сервера")
}

func (r *RocketClient) handleWarning(msg protocol.Message) {
	data, _ := json.Marshal(msg.Data)
	var warningMsg protocol.WarningMessage
	if err := json.Unmarshal(data, &warningMsg); err != nil {
		log.Printf("Ошибка декодирования предупреждения: %v", err)
		return
	}

	log.Printf("ПРЕДУПРЕЖДЕНИЕ [%s]: %s", warningMsg.Severity, warningMsg.Warning)
}

func (r *RocketClient) disconnect() {
	if r.conn != nil {
		msg := protocol.Message{
			Type:      protocol.MsgTypeDisconnect,
			Timestamp: time.Now(),
			Data: protocol.DisconnectMessage{
				RocketID: r.ID,
				Reason:   "Завершение полёта",
			},
		}
		r.conn.WriteJSON(msg)
		r.conn.Close()
	}
}

func (r *RocketClient) Stop() {
	r.running = false
}

func main() {
	serverURL := flag.String("server", "ws://localhost:8080/ws", "URL сервера")
	rocketID := flag.String("id", fmt.Sprintf("rocket-%d", rand.Intn(10000)), "ID ракеты")
	rocketName := flag.String("name", "Test Rocket", "Название ракеты")
	latitude := flag.Float64("lat", 45.0, "Широта запуска")
	longitude := flag.Float64("lon", 63.0, "Долгота запуска")
	altitude := flag.Float64("alt", 100.0, "Высота над уровнем моря")

	flag.Parse()

	config := protocol.RocketConfig{
		Name:            *rocketName,
		MassEmpty:       20000.0,  // Масса пустой ракеты
		MassFuel:        400000.0, // Топливо (достаточно для орбиты)
		MassFuelMax:     400000.0,
		FuelType:        protocol.FuelTypeKerosene,
		DragCoefficient: 0.3,  // Аэродинамический коэффициент
		CrossSection:    12.0, // Площадь сечения м2
		Engines: []protocol.Engine{
			{Thrust: 7600000.0, FuelConsumption: 2500.0, IsActive: true}, // Merlin engine
		},
	}

	client := NewRocketClient(*rocketID, config, *serverURL)

	if err := client.Connect(); err != nil {
		log.Fatalf("Ошибка подключения: %v", err)
	}

	if err := client.Register(); err != nil {
		log.Fatalf("Ошибка регистрации: %v", err)
	}

	if err := client.InitPhysics(*latitude, *longitude, *altitude); err != nil {
		log.Fatalf("Ошибка инициализации физики: %v", err)
	}

	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, os.Interrupt)
	go func() {
		<-sigChan
		log.Println("Получен сигнал прерывания, завершение...")
		client.Stop()
	}()

	client.Run()

	log.Println("Клиент завершил работу")
}
