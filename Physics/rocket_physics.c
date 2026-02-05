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

    if (state->altitude > EARTH_ATMOSPHERE) {
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

Vector3 calculate_thrust(const RocketConfig* config, const ControlCommand* command,
                         const Vector3* position) {
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

    if (thrust_magnitude < 1e-6) {
        return total_thrust;
    }

    Vector3 radial_up = vector_normalize(position);

    Vector3 z_axis = {0, 0, 1};
    Vector3 east = vector_cross(&radial_up, &z_axis);
    double east_mag = vector_magnitude(&east);
    if (east_mag < 0.01) {
        Vector3 x_axis = {1, 0, 0};
        east = vector_cross(&radial_up, &x_axis);
    }
    east = vector_normalize(&east);

    double pitch_rad = command->pitch * M_PI / 180.0;
    Vector3 thrust_dir = {
        radial_up.x * cos(pitch_rad) + east.x * sin(pitch_rad),
        radial_up.y * cos(pitch_rad) + east.y * sin(pitch_rad),
        radial_up.z * cos(pitch_rad) + east.z * sin(pitch_rad)
    };

    total_thrust = vector_scale(&thrust_dir, thrust_magnitude);

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
    if (state->altitude < EARTH_ATMOSPHERE) {
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
    Vector3 thrust_force = calculate_thrust(config, command, &state->position);

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

PlanetConfig planet_earth_default(void) {
    PlanetConfig earth = {
        .radius = EARTH_RADIUS,
        .mass = EARTH_MASS,
        .atmosphere_height = EARTH_ATMOSPHERE,
        .surface_pressure = 1.0,
        .scale_height = EARTH_SCALE_HEIGHT
    };
    return earth;
}

PlanetConfig planet_create(double radius, double mass, double atmosphere_height,
                           double surface_pressure, double scale_height) {
    PlanetConfig planet = {
        .radius = radius,
        .mass = mass,
        .atmosphere_height = atmosphere_height,
        .surface_pressure = surface_pressure,
        .scale_height = scale_height
    };
    return planet;
}

double orbital_velocity_at_altitude(const PlanetConfig* planet, double altitude) {
    double r = planet->radius + altitude;
    return sqrt(G_CONSTANT * planet->mass / r);
}

GravityTurnConfig gravity_turn_for_orbit(const PlanetConfig* planet, double target_orbit_altitude) {
    GravityTurnConfig config;
    config.target_altitude = target_orbit_altitude;
    config.auto_pitch = true;

    config.turn_start_alt = fmax(target_orbit_altitude * 0.01, 1000.0);

    config.turn_end_alt = target_orbit_altitude * 0.7;

    if (config.turn_end_alt < planet->atmosphere_height * 0.5) {
        config.turn_end_alt = planet->atmosphere_height * 0.5;
    }

    return config;
}

double calculate_optimal_pitch(const RocketState* state, const PlanetConfig* planet,
                               const GravityTurnConfig* gt_config) {
    if (!gt_config->auto_pitch) {
        return 0.0; 
    }

    double alt = state->altitude;
    double start = gt_config->turn_start_alt;
    double end = gt_config->turn_end_alt;

    if (alt < start) {
        return 0.0;
    }

    if (alt >= end) {
        return 90.0;
    }

    double progress = (alt - start) / (end - start);

    double smooth_progress = sin(progress * M_PI / 2.0);

    return smooth_progress * 90.0;
}

OrbitPrediction predict_orbit(const RocketState* state, const PlanetConfig* planet) {
    OrbitPrediction pred = {0};

    double r = vector_magnitude(&state->position);
    double v = state->speed;

    double mu = G_CONSTANT * planet->mass;
    double specific_energy = (v * v / 2.0) - (mu / r);

    Vector3 h_vec = vector_cross(&state->position, &state->velocity);
    double h = vector_magnitude(&h_vec);

    double a;
    if (fabs(specific_energy) < 1e-10) {
        a = INFINITY;
        pred.eccentricity = 1.0;
    } else {
        a = -mu / (2.0 * specific_energy);
    }

    if (a != INFINITY) {
        double e_sq = 1.0 - (h * h) / (mu * a);
        if (e_sq < 0) e_sq = 0;
        pred.eccentricity = sqrt(e_sq);
    }

    if (pred.eccentricity < 1.0 && a > 0) {
        pred.apoapsis = a * (1.0 + pred.eccentricity) - planet->radius;
        pred.periapsis = a * (1.0 - pred.eccentricity) - planet->radius;
    } else {
        pred.apoapsis = -1; 
        pred.periapsis = state->altitude; 
    }

    pred.orbital_velocity = v;
    pred.required_velocity = orbital_velocity_at_altitude(planet, state->altitude);

    pred.is_stable = (pred.periapsis > planet->atmosphere_height) && (pred.eccentricity < 1.0);

    return pred;
}

void rocket_update_with_planet(RocketState* state, const RocketConfig* config,
                               const ControlCommand* command, const PlanetConfig* planet,
                               double delta_time) {
    if (state->landed || state->crashed) {
        return;
    }

    double distance = vector_magnitude(&state->position);
    Vector3 gravity_force = {0, 0, 0};
    if (distance > planet->radius) {
        double gravity_magnitude = G_CONSTANT * planet->mass / (distance * distance);
        Vector3 direction = vector_normalize(&state->position);
        gravity_force = vector_scale(&direction, -gravity_magnitude);
    }

    Vector3 drag_force = {0, 0, 0};
    if (state->altitude < planet->atmosphere_height && state->altitude > 0) {
        double rho = planet->surface_pressure * 1.225 * exp(-state->altitude / planet->scale_height);
        double velocity_magnitude = vector_magnitude(&state->velocity);
        if (velocity_magnitude > 1e-6) {
            double drag = 0.5 * rho * velocity_magnitude * velocity_magnitude *
                         config->drag_coefficient * config->cross_section;
            Vector3 velocity_direction = vector_normalize(&state->velocity);
            drag_force = vector_scale(&velocity_direction, -drag);
        }
    }

    Vector3 thrust_force = calculate_thrust(config, command, &state->position);

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

    distance = vector_magnitude(&state->position);
    state->altitude = distance - planet->radius;

    if (distance <= planet->radius) {
        if (state->speed < 5.0) {
            state->landed = true;
        } else {
            state->crashed = true;
        }
        state->velocity = (Vector3){0, 0, 0};
        state->acceleration = (Vector3){0, 0, 0};
        return;
    }

    OrbitPrediction orbit = predict_orbit(state, planet);
    state->in_orbit = orbit.is_stable;

    state->time += delta_time;
}
