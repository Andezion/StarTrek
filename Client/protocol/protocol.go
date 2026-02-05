package protocol

import "time"

type MessageType string

const (
	MsgTypeRegister   MessageType = "register"   // Регистрация ракеты
	MsgTypeTelemetry  MessageType = "telemetry"  // Телеметрия состояния ракеты
	MsgTypeDisconnect MessageType = "disconnect" // Отключение ракеты

	MsgTypeAccepted   MessageType = "accepted"    // Регистрация принята
	MsgTypeRejected   MessageType = "rejected"    // Регистрация отклонена
	MsgTypeCommand    MessageType = "command"     // Команда управления
	MsgTypeWarning    MessageType = "warning"     // Предупреждение
	MsgTypeShutdown   MessageType = "shutdown"    // Команда на выключение
	MsgTypeTrajectory MessageType = "trajectory"  // Рекомендуемая траектория
	MsgTypeRocketList MessageType = "rocket_list" // Список активных ракет

	MsgTypeSubscribe    MessageType = "subscribe"     // Подписка на события (от визуализатора)
	MsgTypeUnsubscribe  MessageType = "unsubscribe"   // Отписка от событий
	MsgTypeBroadcast    MessageType = "broadcast"     // Рассылка телеметрии наблюдателям
	MsgTypeRocketJoined MessageType = "rocket_joined" // Новая ракета подключилась
	MsgTypeRocketLeft   MessageType = "rocket_left"   // Ракета отключилась
)

type FuelType string

const (
	FuelTypeKerosene FuelType = "kerosene"
	FuelTypeLiquidH2 FuelType = "liquid_h2"
	FuelTypeSolid    FuelType = "solid"
)

type Vector3 struct {
	X float64 `json:"x"`
	Y float64 `json:"y"`
	Z float64 `json:"z"`
}

type Engine struct {
	Thrust          float64 `json:"thrust"`           // Тяга в Ньютонах
	FuelConsumption float64 `json:"fuel_consumption"` // Расход топлива кг/с
	IsActive        bool    `json:"is_active"`        // Активен ли двигатель
}

type RocketConfig struct {
	Name            string   `json:"name"`             // Название ракеты
	MassEmpty       float64  `json:"mass_empty"`       // Масса пустой ракеты в кг
	MassFuel        float64  `json:"mass_fuel"`        // Текущая масса топлива в кг
	MassFuelMax     float64  `json:"mass_fuel_max"`    // Максимальная масса топлива в кг
	FuelType        FuelType `json:"fuel_type"`        // Тип топлива
	Engines         []Engine `json:"engines"`          // Массив двигателей
	DragCoefficient float64  `json:"drag_coefficient"` // Коэффициент сопротивления
	CrossSection    float64  `json:"cross_section"`    // Площадь поперечного сечения м2
}

type RocketState struct {
	Position      Vector3 `json:"position"`       // Позиция в метрах
	Velocity      Vector3 `json:"velocity"`       // Скорость в м/с
	Acceleration  Vector3 `json:"acceleration"`   // Ускорение в м/с2
	Altitude      float64 `json:"altitude"`       // Высота над поверхностью Земли в м
	Speed         float64 `json:"speed"`          // Скорость (модуль вектора) в м/с
	MassCurrent   float64 `json:"mass_current"`   // Текущая масса в кг
	FuelRemaining float64 `json:"fuel_remaining"` // Оставшееся топливо в кг
	InOrbit       bool    `json:"in_orbit"`       // Находится ли на орбите
	Landed        bool    `json:"landed"`         // Приземлилась ли
	Crashed       bool    `json:"crashed"`        // Разбилась ли
	Time          float64 `json:"time"`           // Время симуляции в секундах

	OrbitApoapsis         float64 `json:"orbit_apoapsis"`          // Апоцентр (м), -1 если не определен
	OrbitPeriapsis        float64 `json:"orbit_periapsis"`         // Перицентр (м)
	OrbitEccentricity     float64 `json:"orbit_eccentricity"`      // Эксцентриситет
	OrbitRequiredVelocity float64 `json:"orbit_required_velocity"` // Необходимая скорость для круговой орбиты
	OrbitIsStable         bool    `json:"orbit_is_stable"`         // Стабильна ли орбита
}

type ControlCommand struct {
	EngineThrottle []float64 `json:"engine_throttle"` // Дроссели двигателей (0.0 - 1.0)
	Pitch          float64   `json:"pitch"`           // Угол тангажа
	Yaw            float64   `json:"yaw"`             // Угол рыскания
	Roll           float64   `json:"roll"`            // Угол крена
}

type Message struct {
	Type      MessageType `json:"type"`
	Timestamp time.Time   `json:"timestamp"`
	Data      interface{} `json:"data"`
}

type RegisterMessage struct {
	RocketID string       `json:"rocket_id"`
	Config   RocketConfig `json:"config"`
}

type TelemetryMessage struct {
	RocketID string      `json:"rocket_id"`
	State    RocketState `json:"state"`
}

type CommandMessage struct {
	RocketID string         `json:"rocket_id"`
	Command  ControlCommand `json:"command"`
}

type AcceptedMessage struct {
	RocketID string `json:"rocket_id"`
	Message  string `json:"message"`
}

type RejectedMessage struct {
	RocketID string `json:"rocket_id"`
	Reason   string `json:"reason"`
}

type WarningMessage struct {
	RocketID string `json:"rocket_id"`
	Warning  string `json:"warning"`
	Severity string `json:"severity"` // low, medium, high, critical
}

type TrajectoryMessage struct {
	RocketID  string    `json:"rocket_id"`
	Waypoints []Vector3 `json:"waypoints"`
}

type RocketInfo struct {
	RocketID string       `json:"rocket_id"`
	Name     string       `json:"name"`
	State    RocketState  `json:"state"`
	Config   RocketConfig `json:"config"`
}

type RocketListMessage struct {
	Rockets []RocketInfo `json:"rockets"`
}

type DisconnectMessage struct {
	RocketID string `json:"rocket_id"`
	Reason   string `json:"reason"`
}

type SubscribeMessage struct {
	ObserverID string `json:"observer_id"`
}

type UnsubscribeMessage struct {
	ObserverID string `json:"observer_id"`
}

type BroadcastMessage struct {
	RocketID string      `json:"rocket_id"`
	Name     string      `json:"name"`
	State    RocketState `json:"state"`
}

type RocketJoinedMessage struct {
	RocketID string       `json:"rocket_id"`
	Name     string       `json:"name"`
	Config   RocketConfig `json:"config"`
}

type RocketLeftMessage struct {
	RocketID string `json:"rocket_id"`
	Reason   string `json:"reason"`
}

const (
	EarthRadius      = 6371000.0 // м
	EarthMass        = 5.972e24  // кг
	GConstant        = 6.674e-11 // м2/(кг*с2)
	OrbitalVelocity  = 7900.0    // м/с
	AtmosphereHeight = 100000.0  // м
)

func ValidateRocketConfig(config *RocketConfig) error {
	if config.Name == "" {
		return &ValidationError{Field: "name", Message: "название ракеты не может быть пустым"}
	}

	if config.MassEmpty <= 0 {
		return &ValidationError{Field: "mass_empty", Message: "масса пустой ракеты должна быть положительной"}
	}

	if config.MassFuel < 0 {
		return &ValidationError{Field: "mass_fuel", Message: "масса топлива не может быть отрицательной"}
	}

	if config.MassFuelMax < config.MassFuel {
		return &ValidationError{Field: "mass_fuel_max", Message: "максимальная масса топлива должна быть >= текущей массе"}
	}

	if len(config.Engines) == 0 {
		return &ValidationError{Field: "engines", Message: "ракета должна иметь хотя бы один двигатель"}
	}

	for i, engine := range config.Engines {
		if engine.Thrust <= 0 {
			return &ValidationError{Field: "engines", Message: "тяга двигателя должна быть положительной", Index: i}
		}
		if engine.FuelConsumption < 0 {
			return &ValidationError{Field: "engines", Message: "расход топлива не может быть отрицательным", Index: i}
		}
	}

	if config.DragCoefficient < 0 {
		return &ValidationError{Field: "drag_coefficient", Message: "коэффициент сопротивления не может быть отрицательным"}
	}

	if config.CrossSection <= 0 {
		return &ValidationError{Field: "cross_section", Message: "площадь сечения должна быть положительной"}
	}

	return nil
}

type ValidationError struct {
	Field   string
	Message string
	Index   int
}

func (e *ValidationError) Error() string {
	if e.Index >= 0 {
		return e.Field + "[" + string(rune(e.Index)) + "]: " + e.Message
	}
	return e.Field + ": " + e.Message
}
