package physics

/*
#cgo CFLAGS: -I../../Physics
#cgo LDFLAGS: -L../../Physics -lrocket_physics -lm
#include "rocket_physics.h"
#include <stdlib.h>
*/
import "C"
import (
	"cosmodrom/client/protocol"
	"math"
	"unsafe"
)

type PlanetConfig struct {
	Radius           float64 // Радиус планеты (м)
	Mass             float64 // Масса планеты (кг)
	AtmosphereHeight float64 // Высота атмосферы (м)
	SurfacePressure  float64 // Давление на поверхности (1.0 для Земли)
	ScaleHeight      float64 // Масштабная высота атмосферы (м)
}

type GravityTurnConfig struct {
	TargetAltitude float64 // Целевая высота орбиты (м)
	TurnStartAlt   float64 // Высота начала поворота (м)
	TurnEndAlt     float64 // Высота окончания поворота (м)
	AutoPitch      bool    // Включен ли автоматический pitch
}

type OrbitPrediction struct {
	Apoapsis         float64 // Апоцентр (м)
	Periapsis        float64 // Перицентр (м)
	Eccentricity     float64 // Эксцентриситет
	OrbitalVelocity  float64 // Текущая скорость
	RequiredVelocity float64 // Нужная скорость для круговой орбиты
	IsStable         bool    // Стабильна ли орбита
}

type RocketPhysics struct {
	state    *C.RocketState
	config   C.RocketConfig
	planet   PlanetConfig
	gtConfig GravityTurnConfig
}

func EarthDefault() PlanetConfig {
	return PlanetConfig{
		Radius:           6371000.0,
		Mass:             5.972e24,
		AtmosphereHeight: 100000.0,
		SurfacePressure:  1.0,
		ScaleHeight:      8500.0,
	}
}

func GravityTurnForOrbit(planet PlanetConfig, targetOrbitAltitude float64) GravityTurnConfig {
	config := GravityTurnConfig{
		TargetAltitude: targetOrbitAltitude,
		AutoPitch:      true,
	}

	config.TurnStartAlt = targetOrbitAltitude * 0.01
	if config.TurnStartAlt < 1000.0 {
		config.TurnStartAlt = 1000.0
	}

	config.TurnEndAlt = targetOrbitAltitude * 0.7

	if config.TurnEndAlt < planet.AtmosphereHeight*0.5 {
		config.TurnEndAlt = planet.AtmosphereHeight * 0.5
	}

	return config
}

func NewRocketPhysics(config *protocol.RocketConfig, initialPos protocol.Vector3) (*RocketPhysics, error) {
	cConfig := C.RocketConfig{
		mass_empty:       C.double(config.MassEmpty),
		mass_fuel:        C.double(config.MassFuel),
		mass_fuel_max:    C.double(config.MassFuelMax),
		drag_coefficient: C.double(config.DragCoefficient),
		cross_section:    C.double(config.CrossSection),
		engine_count:     C.uint32_t(len(config.Engines)),
	}

	nameBytes := []byte(config.Name)
	for i := 0; i < len(nameBytes) && i < 63; i++ {
		cConfig.name[i] = C.char(nameBytes[i])
	}
	cConfig.name[len(nameBytes)] = 0

	switch config.FuelType {
	case protocol.FuelTypeKerosene:
		cConfig.fuel_type = C.FUEL_TYPE_KEROSENE
	case protocol.FuelTypeLiquidH2:
		cConfig.fuel_type = C.FUEL_TYPE_LIQUID_H2
	case protocol.FuelTypeSolid:
		cConfig.fuel_type = C.FUEL_TYPE_SOLID
	}

	if len(config.Engines) > 0 {
		cConfig.engines = (*C.Engine)(C.malloc(C.size_t(len(config.Engines)) * C.size_t(unsafe.Sizeof(C.Engine{}))))
		engines := (*[1 << 30]C.Engine)(unsafe.Pointer(cConfig.engines))[:len(config.Engines):len(config.Engines)]

		for i, engine := range config.Engines {
			engines[i] = C.Engine{
				thrust:           C.double(engine.Thrust),
				fuel_consumption: C.double(engine.FuelConsumption),
				is_active:        C.bool(engine.IsActive),
			}
		}
	}

	cPos := C.Vector3{
		x: C.double(initialPos.X),
		y: C.double(initialPos.Y),
		z: C.double(initialPos.Z),
	}

	state := C.rocket_init(&cConfig, cPos)
	if state == nil {
		if cConfig.engines != nil {
			C.free(unsafe.Pointer(cConfig.engines))
		}
		return nil, &PhysicsError{Message: "не удалось инициализировать физический движок"}
	}

	return &RocketPhysics{
		state:  state,
		config: cConfig,
	}, nil
}

func (p *RocketPhysics) Update(command *protocol.ControlCommand, deltaTime float64) {
	cCommand := C.ControlCommand{
		engine_count: C.uint32_t(len(command.EngineThrottle)),
		pitch:        C.double(command.Pitch),
		yaw:          C.double(command.Yaw),
		roll:         C.double(command.Roll),
	}

	if len(command.EngineThrottle) > 0 {
		cCommand.engine_throttle = (*C.double)(C.malloc(C.size_t(len(command.EngineThrottle)) * C.size_t(unsafe.Sizeof(C.double(0)))))
		throttles := (*[1 << 30]C.double)(unsafe.Pointer(cCommand.engine_throttle))[:len(command.EngineThrottle):len(command.EngineThrottle)]

		for i, throttle := range command.EngineThrottle {
			throttles[i] = C.double(throttle)
		}
	}

	C.rocket_update(p.state, &p.config, &cCommand, C.double(deltaTime))

	if cCommand.engine_throttle != nil {
		C.free(unsafe.Pointer(cCommand.engine_throttle))
	}
}

func (p *RocketPhysics) GetState() protocol.RocketState {
	state := protocol.RocketState{
		Position: protocol.Vector3{
			X: float64(p.state.position.x),
			Y: float64(p.state.position.y),
			Z: float64(p.state.position.z),
		},
		Velocity: protocol.Vector3{
			X: float64(p.state.velocity.x),
			Y: float64(p.state.velocity.y),
			Z: float64(p.state.velocity.z),
		},
		Acceleration: protocol.Vector3{
			X: float64(p.state.acceleration.x),
			Y: float64(p.state.acceleration.y),
			Z: float64(p.state.acceleration.z),
		},
		Altitude:      float64(p.state.altitude),
		Speed:         float64(p.state.speed),
		MassCurrent:   float64(p.state.mass_current),
		FuelRemaining: float64(p.state.fuel_remaining),
		InOrbit:       bool(p.state.in_orbit),
		Landed:        bool(p.state.landed),
		Crashed:       bool(p.state.crashed),
		Time:          float64(p.state.time),
	}

	return state
}

func (p *RocketPhysics) Free() {
	if p.state != nil {
		C.rocket_free(p.state)
		p.state = nil
	}
	if p.config.engines != nil {
		C.free(unsafe.Pointer(p.config.engines))
		p.config.engines = nil
	}
}

func (p *RocketPhysics) SetPlanet(planet PlanetConfig) {
	p.planet = planet
}

func (p *RocketPhysics) SetGravityTurn(gt GravityTurnConfig) {
	p.gtConfig = gt
}

func (p *RocketPhysics) CalculateOptimalPitch() float64 {
	if !p.gtConfig.AutoPitch {
		return 0.0
	}

	alt := float64(p.state.altitude)
	start := p.gtConfig.TurnStartAlt
	end := p.gtConfig.TurnEndAlt

	if alt < start {
		return 0.0
	}

	if alt >= end {
		return 90.0
	}

	progress := (alt - start) / (end - start)
	smoothProgress := math.Sin(progress * math.Pi / 2.0)

	return smoothProgress * 90.0
}

func (p *RocketPhysics) PredictOrbit() OrbitPrediction {
	state := p.GetState()

	r := math.Sqrt(state.Position.X*state.Position.X +
		state.Position.Y*state.Position.Y +
		state.Position.Z*state.Position.Z)
	v := state.Speed

	mu := 6.674e-11 * p.planet.Mass
	specificEnergy := (v*v)/2.0 - mu/r

	hx := state.Position.Y*state.Velocity.Z - state.Position.Z*state.Velocity.Y
	hy := state.Position.Z*state.Velocity.X - state.Position.X*state.Velocity.Z
	hz := state.Position.X*state.Velocity.Y - state.Position.Y*state.Velocity.X
	h := math.Sqrt(hx*hx + hy*hy + hz*hz)

	pred := OrbitPrediction{}

	var a float64
	if math.Abs(specificEnergy) < 1e-10 {
		a = math.Inf(1)
		pred.Eccentricity = 1.0
	} else {
		a = -mu / (2.0 * specificEnergy)
	}

	if !math.IsInf(a, 1) {
		eSq := 1.0 - (h*h)/(mu*a)
		if eSq < 0 {
			eSq = 0
		}
		pred.Eccentricity = math.Sqrt(eSq)
	}

	if pred.Eccentricity < 1.0 && a > 0 {
		pred.Apoapsis = a*(1.0+pred.Eccentricity) - p.planet.Radius
		pred.Periapsis = a*(1.0-pred.Eccentricity) - p.planet.Radius
	} else {
		pred.Apoapsis = -1
		pred.Periapsis = state.Altitude
	}

	pred.OrbitalVelocity = v
	pred.RequiredVelocity = math.Sqrt(mu / (p.planet.Radius + state.Altitude))
	pred.IsStable = pred.Periapsis > p.planet.AtmosphereHeight && pred.Eccentricity < 1.0

	return pred
}

func SphericalToCartesian(latitude, longitude, altitude float64) protocol.Vector3 {
	result := C.spherical_to_cartesian(C.double(latitude), C.double(longitude), C.double(altitude))
	return protocol.Vector3{
		X: float64(result.x),
		Y: float64(result.y),
		Z: float64(result.z),
	}
}

func CartesianToSpherical(pos protocol.Vector3) (latitude, longitude, altitude float64) {
	cPos := C.Vector3{
		x: C.double(pos.X),
		y: C.double(pos.Y),
		z: C.double(pos.Z),
	}

	var lat, lon, alt C.double
	C.cartesian_to_spherical(&cPos, &lat, &lon, &alt)

	return float64(lat), float64(lon), float64(alt)
}

type PhysicsError struct {
	Message string
}

func (e *PhysicsError) Error() string {
	return "Physics error: " + e.Message
}
