#include "system_common.h"
#include "hal_interfaces.h"
#include <esp_log.h>
#include <math.h>

extern const fan_interface_t fan_mock_impl;

// Helper: Cálculo de PWM Lineal
uint32_t calculate_pwm_linear(float current_temp, float t_min, float t_max) {
    if (current_temp <= t_min) return 0;
    if (current_temp >= t_max) return 100;
    
    float ratio = (current_temp - t_min) / (t_max - t_min);
    return (uint32_t)(ratio * 100.0f);
}

void control_task(void *pvParameters) {
    app_context_t *ctx = (app_context_t *)pvParameters;
    const char *TAG = "TASK_CONTROL";
    fan_mock_impl.init();

    sensor_data_t incoming_data;
    uint32_t target_pwm = 0;

    // Configuración inicial por defecto (simulada)
    // En fase 2, esto vendrá de NVS
    xSemaphoreTake(ctx->config_mutex, portMAX_DELAY);
    ctx->shared_config->operation_mode = 1; // 1 = AUTO (por ejemplo)
    xSemaphoreGive(ctx->config_mutex);

    while (1) {
        // Esperar datos del sensor (Consumidor)
        if (xQueueReceive(ctx->sensor_queue, &incoming_data, portMAX_DELAY) == pdTRUE) {
            
            xSemaphoreTake(ctx->config_mutex, portMAX_DELAY);
            system_config_t *cfg = ctx->shared_config;

            // LÓGICA DE NEGOCIO PRINCIPAL
            switch (cfg->operation_mode) {
                case 0: // MANUAL
                    target_pwm = cfg->manual_duty;
                    break;

                case 1: // AUTOMÁTICO (Temp + PIR)
                    if (incoming_data.presence_detected) {
                        // Ejemplo hardcodeado, luego usar configuración
                        target_pwm = calculate_pwm_linear(incoming_data.temperature, 24.0, 28.0);
                    } else {
                        target_pwm = 0;
                    }
                    break;

                case 2: // PROGRAMADO (Registros)
                    // TODO: Implementar lógica de horarios con NTP
                    target_pwm = 0; 
                    break;
            }
            xSemaphoreGive(ctx->config_mutex);

            ESP_LOGI(TAG, "Mode: %d | Temp: %.1f | PIR: %d -> PWM: %lu%%", 
                     ctx->shared_config->operation_mode, 
                     incoming_data.temperature, 
                     incoming_data.presence_detected, 
                     target_pwm);

            // Ejecutar Actuador (Driver Abstracto)
            fan_mock_impl.set_duty(target_pwm);
        }
    }
}