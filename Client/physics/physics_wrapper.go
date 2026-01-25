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
	"unsafe"
)

type RocketPhysics struct {
	state  *C.RocketState
	config C.RocketConfig
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
