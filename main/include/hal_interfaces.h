#pragma once
#include <stdbool.h>
#include <esp_err.h>

// Interfaz Sensor Temperatura
typedef struct {
    esp_err_t (*init)(void);
    float (*read_celsius)(void);
} temp_sensor_interface_t;

// Interfaz Sensor PIR
typedef struct {
    esp_err_t (*init)(void);
    bool (*is_motion_detected)(void);
} pir_sensor_interface_t;

// Interfaz Ventilador
typedef struct {
    esp_err_t (*init)(void);
    esp_err_t (*set_duty)(uint32_t percent);
} fan_interface_t;