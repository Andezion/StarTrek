#ifndef ROCKET_PHYSICS_H
#define ROCKET_PHYSICS_H

#include <stdint.h>
#include <stdbool.h>

#define EARTH_RADIUS 6371000.0      // Радиус Земли в метрах
#define EARTH_MASS 5.972e24         // Масса Земли в кг
#define G_CONSTANT 6.674e-11        // Гравитационная постоянная м3/(кг·с2)
#define ORBITAL_VELOCITY 7900.0     // Первая космическая скорость м/с
#define ATMOSPHERE_HEIGHT 100000.0  // Граница атмосферы м (линия Кармана)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum {
    FUEL_TYPE_KEROSENE,     // Керосин
    FUEL_TYPE_LIQUID_H2,    // Жидкий водород
    FUEL_TYPE_SOLID         // Твердое топливо
} FuelType;

typedef struct {
    double x;
    double y;
    double z;
} Vector3;

typedef struct {
    double thrust;          // Тяга в Ньютонах
    double fuel_consumption; // Расход топлива кг/с
    bool is_active;         // Активен ли двигатель
} Engine;

typedef struct {
    char name[64];          // Название ракеты
    double mass_empty;      // Масса пустой ракеты в кг
    double mass_fuel;       // Текущая масса топлива в кг
    double mass_fuel_max;   // Максимальная масса топлива в кг
    FuelType fuel_type;     // Тип топлива

    Engine* engines;        // Массив двигателей
    uint32_t engine_count;  // Количество двигателей

    double drag_coefficient; // Коэффициент сопротивления
    double cross_section;   // Площадь поперечного сечения м2
} RocketConfig;

typedef struct {
    Vector3 position;       // Позиция в метрах (декартовы координаты)
    Vector3 velocity;       // Скорость в м/с
    Vector3 acceleration;   // Ускорение в м/с2

    double altitude;        // Высота над поверхностью Земли в м
    double speed;           // Скорость (модуль вектора) в м/с

    double mass_current;    // Текущая масса (ракета + топливо) в кг
    double fuel_remaining;  // Оставшееся топливо в кг

    bool in_orbit;          // Находится ли на орбите
    bool landed;            // Приземлилась ли
    bool crashed;           // Разбилась ли

    double time;            // Время симуляции в секундах
} RocketState;

typedef struct {
    double* engine_throttle; // Массив дросселей для каждого двигателя (0.0 - 1.0)
    uint32_t engine_count;

    double pitch;           // Угол тангажа (наклон вперед/назад)
    double yaw;             // Угол рыскания (поворот влево/вправо)
    double roll;            // Угол крена (вращение вокруг оси)
} ControlCommand;


RocketState* rocket_init(const RocketConfig* config, Vector3 initial_position);

void rocket_free(RocketState* state);
void rocket_update(RocketState* state, const RocketConfig* config,
                   const ControlCommand* command, double delta_time);

Vector3 calculate_gravity(const Vector3* position);
Vector3 calculate_drag(const RocketState* state, const RocketConfig* config);
Vector3 calculate_thrust(const RocketConfig* config, const ControlCommand* command);

double calculate_fuel_consumption(const RocketConfig* config,
                                  const ControlCommand* command, double delta_time);

bool check_ground_collision(const RocketState* state);
bool check_orbital_stability(const RocketState* state);

void cartesian_to_spherical(const Vector3* position, double* latitude,
                            double* longitude, double* altitude);

Vector3 spherical_to_cartesian(double latitude, double longitude, double altitude);

Vector3 vector_add(const Vector3* a, const Vector3* b);
Vector3 vector_sub(const Vector3* a, const Vector3* b);
Vector3 vector_scale(const Vector3* v, double scalar);
double vector_magnitude(const Vector3* v);
Vector3 vector_normalize(const Vector3* v);
double vector_dot(const Vector3* a, const Vector3* b);
Vector3 vector_cross(const Vector3* a, const Vector3* b);

#endif // ROCKET_PHYSICS_H
