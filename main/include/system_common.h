#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// --- Definiciones de Tipos ---
// (Incluir aquí sensor_data_t, fan_command_t, system_config_t)
// data_types.h

// Datos del Sensor (Producido por Sensor Task)
typedef struct {
    float temperature;
    bool presence_detected;
    int64_t timestamp;
} sensor_data_t;

// Comandos de Actuación (Consumido por Actuator Task)
typedef struct {
    uint32_t duty_cycle_percent; // 0-100
    bool enabled;
} fan_command_t;

// Definición de Registro Horario
typedef struct {
    uint8_t start_hour;
    uint8_t start_min;
    uint8_t end_hour;
    uint8_t end_min;
    float temp_min_0_percent;   // T_0%
    float temp_max_100_percent; // T_100%
    bool active;
} schedule_reg_t;

// Configuración Global
typedef struct {
    enum { MODE_MANUAL, MODE_AUTO, MODE_SCHEDULE } operation_mode;
    uint32_t manual_duty;
    schedule_reg_t schedules[3];
} system_config_t;

// Contexto del Sistema (Inyección de dependencias)
typedef struct {
    QueueHandle_t sensor_queue;
    QueueHandle_t actuator_queue;
    SemaphoreHandle_t config_mutex;
    system_config_t *shared_config; // Puntero a config en memoria compartida
} app_context_t;