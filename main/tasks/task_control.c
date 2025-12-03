#include "system_common.h"
#include "hal_interfaces.h"
#include <esp_log.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "TASK_CONTROL";
extern const fan_interface_t fan_mock_impl;

// --- FUNCIONES AUXILIARES ---

// Convierte hora/min a minutos absolutos (ej: 01:30 -> 90 min)
static int get_minutes_from_midnight(int hour, int min) {
    return (hour * 60) + min;
}

// Verifica si la hora actual está dentro del rango del registro
static bool is_time_in_range(struct tm *now, schedule_reg_t *reg) {
    int now_mins = get_minutes_from_midnight(now->tm_hour, now->tm_min);
    int start_mins = get_minutes_from_midnight(reg->start_hour, reg->start_min);
    int end_mins = get_minutes_from_midnight(reg->end_hour, reg->end_min);

    if (start_mins < end_mins) {
        // Caso normal (ej: 14:00 a 18:00)
        return (now_mins >= start_mins && now_mins < end_mins);
    } else {
        // Caso cruce de medianoche (ej: 22:00 a 06:00)
        // Es válido si es mayor que el inicio (noche) O menor que el fin (madrugada)
        return (now_mins >= start_mins || now_mins < end_mins);
    }
}

// Cálculo de PWM Lineal (Reutilizable)
static uint32_t calculate_pwm_linear(float current_temp, float t_min, float t_max) {
    if (current_temp <= t_min) return 0;
    if (current_temp >= t_max) return 100;
    
    // Protección contra división por cero
    if ((t_max - t_min) < 0.1) return 100;

    float ratio = (current_temp - t_min) / (t_max - t_min);
    return (uint32_t)(ratio * 100.0f);
}

// --- TAREA PRINCIPAL ---

void control_task(void *pvParameters) {
    app_context_t *ctx = (app_context_t *)pvParameters;
    
    fan_mock_impl.init();

    sensor_data_t incoming_data;
    uint32_t target_pwm = 0;

    // Variables de tiempo
    time_t now;
    struct tm timeinfo;
    bool time_synced = false;

    while (1) {
        // Esperar datos del sensor
        if (xQueueReceive(ctx->sensor_queue, &incoming_data, portMAX_DELAY) == pdTRUE) {
            
            // Actualizar tiempo
            time(&now);
            localtime_r(&now, &timeinfo);
            // Si el año es < 2020, asumimos que NTP no ha sincronizado aún
            time_synced = (timeinfo.tm_year > (2020 - 1900));

            xSemaphoreTake(ctx->config_mutex, portMAX_DELAY);
            system_config_t *cfg = ctx->shared_config;

            switch (cfg->operation_mode) {
                // ---------------- MANUAL ----------------
                case 0: 
                    target_pwm = cfg->manual_duty;
                    break;

                // ---------------- AUTO (General) ----------------
                case 1: 
                    if (incoming_data.presence_detected) {
                        // Usamos un rango genérico hardcodeado por ahora (o podrías agregarlo a config global)
                        target_pwm = calculate_pwm_linear(incoming_data.temperature, 24.0, 28.0);
                    } else {
                        target_pwm = 0;
                    }
                    break;

                // ---------------- PROGRAMADO (Horarios) ----------------
                case 2:
                    target_pwm = 0; // Por defecto apagado si no coincide horario
                    
                    if (!time_synced) {
                        ESP_LOGW(TAG, "Modo Programado requiere NTP. Hora no valida.");
                        break; 
                    }

                    if (!incoming_data.presence_detected) {
                         // Regla de Oro: Sin presencia = Apagado (incluso en horario)
                         target_pwm = 0;
                    } else {
                        // Buscar en los 3 registros
                        bool match_found = false;
                        for (int i = 0; i < 3; i++) {
                            schedule_reg_t *reg = &cfg->schedules[i];
                            
                            if (reg->active && is_time_in_range(&timeinfo, reg)) {
                                match_found = true;
                                target_pwm = calculate_pwm_linear(
                                    incoming_data.temperature, 
                                    reg->temp_min_0_percent, 
                                    reg->temp_max_100_percent
                                );
                                
                                ESP_LOGI(TAG, "Registro #%d Activo (%02d:%02d-%02d:%02d). Rango Temp: %.1f-%.1f", 
                                         i, reg->start_hour, reg->start_min, reg->end_hour, reg->end_min,
                                         reg->temp_min_0_percent, reg->temp_max_100_percent);
                                break; // Prioridad al primero que encuentre
                            }
                        }
                        if (!match_found) {
                            ESP_LOGD(TAG, "Fuera de horario programado");
                        }
                    }
                    break;
            }
            xSemaphoreGive(ctx->config_mutex);

            // Logging de estado
            char time_buf[10];
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &timeinfo);
            
            ESP_LOGI(TAG, "[%s] Mode: %d | Temp: %.1f | PIR: %d -> PWM: %lu%%", 
                     time_buf,
                     ctx->shared_config->operation_mode, 
                     incoming_data.temperature, 
                     incoming_data.presence_detected, 
                     target_pwm);

            fan_mock_impl.set_duty(target_pwm);
        }
    }
}