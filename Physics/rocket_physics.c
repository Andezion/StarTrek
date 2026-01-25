#include "rocket_physics.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

Vector3 vector_add(const Vector3* a, const Vector3* b) {
    Vector3 result = {a->x + b->x, a->y + b->y, a->z + b->z};
    return result;
}

Vector3 vector_sub(const Vector3* a, const Vector3* b) {
    Vector3 result = {a->x - b->x, a->y - b->y, a->z - b->z};
    return result;
}

Vector3 vector_scale(const Vector3* v, double scalar) {
    Vector3 result = {v->x * scalar, v->y * scalar, v->z * scalar};
    return result;
}

double vector_magnitude(const Vector3* v) {
    return sqrt(v->x * v->x + v->y * v->y + v->z * v->z);
}

Vector3 vector_normalize(const Vector3* v) {
    double mag = vector_magnitude(v);
    if (mag < 1e-10) {
        Vector3 zero = {0, 0, 0};
        return zero;
    }
    return vector_scale(v, 1.0 / mag);
}

double vector_dot(const Vector3* a, const Vector3* b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

Vector3 vector_cross(const Vector3* a, const Vector3* b) {
    Vector3 result = {
        a->y * b->z - a->z * b->y,
        a->z * b->x - a->x * b->z,
        a->x * b->y - a->y * b->x
    };
    return result;
}

RocketState* rocket_init(const RocketConfig* config, Vector3 initial_position) {
    RocketState* state = (RocketState*)calloc(1, sizeof(RocketState));
    if (!state) return NULL;

    state->position = initial_position;
    state->velocity = (Vector3){0, 0, 0};
    state->acceleration = (Vector3){0, 0, 0};

    state->mass_current = config->mass_empty + config->mass_fuel;
    state->fuel_remaining = config->mass_fuel;

    state->altitude = vector_magnitude(&initial_position) - EARTH_RADIUS;
    state->speed = 0.0;

    state->in_orbit = false;
    state->landed = false;
    state->crashed = false;
    state->time = 0.0;

    return state;
}

void rocket_free(RocketState* state) {
    if (state) {
        free(state);
    }
}

Vector3 calculate_gravity(const Vector3* position) {
    double distance = vector_magnitude(position);
    if (distance < EARTH_RADIUS) {
        Vector3 zero = {0, 0, 0};
        return zero;
    }

    double gravity_magnitude = G_CONSTANT * EARTH_MASS / (distance * distance);

    Vector3 direction = vector_normalize(position);
    Vector3 gravity = vector_scale(&direction, -gravity_magnitude);

    return gravity;
}

Vector3 calculate_drag(const RocketState* state, const RocketConfig* config) {
    // Упрощенная модель, типа плотность экспоненциально уменьшается с высотой

    if (state->altitude > ATMOSPHERE_HEIGHT) {
        Vector3 zero = {0, 0, 0};
        return zero;
    }

    double rho_0 = 1.225;
    double scale_height = 8500.0; 

    double rho = rho_0 * exp(-state->altitude / scale_height);

    double velocity_magnitude = vector_magnitude(&state->velocity);
    if (velocity_magnitude < 1e-6) {
        Vector3 zero = {0, 0, 0};
        return zero;
    }

    double drag_force = 0.5 * rho * velocity_magnitude * velocity_magnitude *
                        config->drag_coefficient * config->cross_section;

    Vector3 velocity_direction = vector_normalize(&state->velocity);
    Vector3 drag = vector_scale(&velocity_direction, -drag_force);

    return drag;
}

Vector3 calculate_thrust(const RocketConfig* config, const ControlCommand* command) {
    Vector3 total_thrust = {0, 0, 0};

    if (!command || !command->engine_throttle) {
        return total_thrust;
    }

    double thrust_magnitude = 0.0;
    for (uint32_t i = 0; i < config->engine_count && i < command->engine_count; i++) {
        if (config->engines[i].is_active) {
            thrust_magnitude += config->engines[i].thrust * command->engine_throttle[i];
        }
    }

    // TODO - Добавить расчет ориентации ракеты на основе углов Эйлера
    Vector3 up_direction = vector_normalize(&(Vector3){0, 0, 1});

    total_thrust = vector_scale(&up_direction, thrust_magnitude);

    return total_thrust;
}

double calculate_fuel_consumption(const RocketConfig* config,
                                  const ControlCommand* command, double delta_time) {
    if (!command || !command->engine_throttle) {
        return 0.0;
    }

    double total_consumption = 0.0;
    for (uint32_t i = 0; i < config->engine_count && i < command->engine_count; i++) {
        if (config->engines[i].is_active) {
            total_consumption += config->engines[i].fuel_consumption *
                                command->engine_throttle[i] * delta_time;
        }
    }

    return total_consumption;
}

bool check_ground_collision(const RocketState* state) {
    double distance = vector_magnitude(&state->position);
    return distance <= EARTH_RADIUS;
}

bool check_orbital_stability(const RocketState* state) {
    if (state->altitude < ATMOSPHERE_HEIGHT) {
        return false;
    }

    double distance = vector_magnitude(&state->position);
    double orbital_speed = sqrt(G_CONSTANT * EARTH_MASS / distance);

    double speed_ratio = state->speed / orbital_speed;
    if (speed_ratio >= 0.9 && speed_ratio <= 1.1) {
        return true;
    }

    return false;
}

void rocket_update(RocketState* state, const RocketConfig* config,
                   const ControlCommand* command, double delta_time) {
    
    if (state->landed || state->crashed) {
        return; 
    }

    Vector3 gravity_force = calculate_gravity(&state->position);
    Vector3 drag_force = calculate_drag(state, config);
    Vector3 thrust_force = calculate_thrust(config, command);

    Vector3 total_force = vector_add(&gravity_force, &drag_force);
    total_force = vector_add(&total_force, &thrust_force);

    if (state->mass_current > 0) {
        state->acceleration = vector_scale(&total_force, 1.0 / state->mass_current);
    } else {
        state->acceleration = (Vector3){0, 0, 0};
    }

    Vector3 delta_velocity = vector_scale(&state->acceleration, delta_time);
    state->velocity = vector_add(&state->velocity, &delta_velocity);
    state->speed = vector_magnitude(&state->velocity);

    Vector3 delta_position = vector_scale(&state->velocity, delta_time);
    state->position = vector_add(&state->position, &delta_position);

    double fuel_consumed = calculate_fuel_consumption(config, command, delta_time);
    state->fuel_remaining -= fuel_consumed;
    if (state->fuel_remaining < 0) {
        state->fuel_remaining = 0;
    }

    state->mass_current = config->mass_empty + state->fuel_remaining;

    double distance = vector_magnitude(&state->position);
    state->altitude = distance - EARTH_RADIUS;

    if (check_ground_collision(state)) {
        if (state->speed < 5.0) { 
            state->landed = true;
        } else {
            state->crashed = true;
        }
        state->velocity = (Vector3){0, 0, 0};
        state->acceleration = (Vector3){0, 0, 0};
        return;
    }

    state->in_orbit = check_orbital_stability(state);

    state->time += delta_time;
}

void cartesian_to_spherical(const Vector3* position, double* latitude,
                            double* longitude, double* altitude) {
    double x = position->x;
    double y = position->y;
    double z = position->z;

    double r = sqrt(x*x + y*y + z*z);
    *altitude = r - EARTH_RADIUS;

    *latitude = asin(z / r) * 180.0 / M_PI;
    *longitude = atan2(y, x) * 180.0 / M_PI;
}

Vector3 spherical_to_cartesian(double latitude, double longitude, double altitude) {
    double lat_rad = latitude * M_PI / 180.0;
    double lon_rad = longitude * M_PI / 180.0;
    double r = EARTH_RADIUS + altitude;

    Vector3 result = {
        r * cos(lat_rad) * cos(lon_rad),
        r * cos(lat_rad) * sin(lon_rad),
        r * sin(lat_rad)
    };

    return result;
}
