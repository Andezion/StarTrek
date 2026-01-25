#include "rocket_physics.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Тест физического движка ракеты\n\n");

    RocketConfig config = {
        .name = "Test Rocket 1",
        .mass_empty = 5000.0,           
        .mass_fuel = 15000.0,           
        .mass_fuel_max = 15000.0,
        .fuel_type = FUEL_TYPE_KEROSENE,
        .drag_coefficient = 0.5,
        .cross_section = 10.0,          
        .engine_count = 4
    };

    config.engines = malloc(sizeof(Engine) * config.engine_count);
    for (uint32_t i = 0; i < config.engine_count; i++) {
        config.engines[i].thrust = 500000.0;           
        config.engines[i].fuel_consumption = 250.0;    
        config.engines[i].is_active = true;
    }

    Vector3 initial_pos = spherical_to_cartesian(45.0, 63.0, 100.0); // 100м над уровнем моря

    RocketState* state = rocket_init(&config, initial_pos);
    if (!state) {
        fprintf(stderr, "Ошибка инициализации ракеты\n");
        free(config.engines);
        return 1;
    }

    printf("Ракета: %s\n", config.name);
    printf("Масса (пустая): %.0f кг\n", config.mass_empty);
    printf("Топливо: %.0f кг\n", config.mass_fuel);
    printf("Двигатели: %u x %.0f кН\n", config.engine_count, config.engines[0].thrust / 1000.0);
    printf("Начальная высота: %.2f м\n\n", state->altitude);

    ControlCommand command;
    command.engine_count = config.engine_count;
    command.engine_throttle = malloc(sizeof(double) * command.engine_count);
    for (uint32_t i = 0; i < command.engine_count; i++) {
        command.engine_throttle[i] = 1.0; 
    }
    command.pitch = 0.0;
    command.yaw = 0.0;
    command.roll = 0.0;

    double dt = 0.1; 
    double total_time = 0.0;
    double print_interval = 10.0; 
    double next_print = print_interval;

    printf("Симуляция запуска\n");
    printf("Время(с) | Высота(км) | Скорость(м/с) | Топливо(кг) | Статус\n");
    printf("---------|------------|---------------|-------------|--------\n");

    while (total_time < 600.0 && !state->landed && !state->crashed) {
        rocket_update(state, &config, &command, dt);
        total_time += dt;

        if (state->fuel_remaining <= 0) {
            for (uint32_t i = 0; i < command.engine_count; i++) {
                command.engine_throttle[i] = 0.0;
            }
        }

        if (total_time >= next_print) {
            const char* status = "Полет";
            if (state->in_orbit) status = "На орбите";
            if (state->landed) status = "Приземление";
            if (state->crashed) status = "Авария";

            printf("%8.1f | %10.2f | %13.1f | %11.0f | %s\n",
                   total_time,
                   state->altitude / 1000.0,
                   state->speed,
                   state->fuel_remaining,
                   status);

            next_print += print_interval;
        }

        if (state->in_orbit) {
            printf("\nОрбита достигнута!\n");
            printf("Высота: %.2f км\n", state->altitude / 1000.0);
            printf("Скорость: %.1f м/с\n", state->speed);
            printf("Оставшееся топливо: %.0f кг\n", state->fuel_remaining);
            break;
        }

        if (state->landed || state->crashed) {
            printf("\n%s\n", state->crashed ? "АВАРИЯ" : "Посадка");
            break;
        }
    }

    free(command.engine_throttle);
    rocket_free(state);
    free(config.engines);

    printf("\nТест завершен\n");
    return 0;
}
