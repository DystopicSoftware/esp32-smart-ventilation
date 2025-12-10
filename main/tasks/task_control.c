#include "system_common.h"
#include "hal_interfaces.h"
#include <esp_log.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "TASK_CONTROL";
//extern const fan_interface_t fan_mock_impl;
extern const fan_interface_t fan_driver_impl; // USAR ESTE (Real PWM)


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
    
    // Inicializar el Driver Real (LED integrado GPIO 2)
    fan_driver_impl.init();

    sensor_data_t incoming_data;
    uint32_t target_pwm = 0;

    // Variables de tiempo
    time_t now;
    struct tm timeinfo;
    bool time_synced = false;

    while (1) {
        // Esperar datos del sensor (Bloqueante hasta que llegue algo)
        if (xQueueReceive(ctx->sensor_queue, &incoming_data, portMAX_DELAY) == pdTRUE) {
            
            // 1. Actualizar tiempo
            time(&now);
            localtime_r(&now, &timeinfo);
            time_synced = (timeinfo.tm_year > (2020 - 1900));

            // 2. Tomar Mutex para leer Config y escribir Estado
            xSemaphoreTake(ctx->config_mutex, portMAX_DELAY);
            system_config_t *cfg = ctx->shared_config;

            // --- LÓGICA DE CONTROL ---
            switch (cfg->operation_mode) {
                case 0: // MANUAL
                    target_pwm = cfg->manual_duty;
                    break;

                case 1: // AUTO
                    if (incoming_data.presence_detected) {
                        target_pwm = calculate_pwm_linear(incoming_data.temperature, 15.0, 25.0);
                    } else {
                        target_pwm = 0;
                    }
                    break;

                case 2: // PROGRAMADO
                    target_pwm = 0; 
                    if (!time_synced) {
                        ESP_LOGW(TAG, "Falta NTP para modo programado");
                        break; 
                    }
                    if (incoming_data.presence_detected) {
                        for (int i = 0; i < 3; i++) {
                            schedule_reg_t *reg = &cfg->schedules[i];
                            if (reg->active && is_time_in_range(&timeinfo, reg)) {
                                target_pwm = calculate_pwm_linear(
                                    incoming_data.temperature, 
                                    reg->temp_min_0_percent, 
                                    reg->temp_max_100_percent
                                );
                                // Log breve para depuración
                                ESP_LOGD(TAG, "Regla horaria #%d activa", i);
                                break; 
                            }
                        }
                    }
                    break;
            }

            // 3. Actualizar el Estado Compartido (Para el Servidor Web)
            if (ctx->shared_state != NULL) {
                ctx->shared_state->current_temp = incoming_data.temperature;
                ctx->shared_state->presence = incoming_data.presence_detected;
                ctx->shared_state->current_pwm = target_pwm;
                strftime(ctx->shared_state->current_time_str, 16, "%H:%M:%S", &timeinfo);
            }

            // Guardamos el modo en una variable local para el log, así podemos soltar el mutex rápido
            int current_mode = cfg->operation_mode;

            // 4. Liberar Mutex (¡SOLO UNA VEZ!)
            xSemaphoreGive(ctx->config_mutex); 

            // 5. Actuar sobre el Hardware (Ventilador)
            fan_driver_impl.set_duty(target_pwm);

            // 6. Logging informativo
            ESP_LOGI(TAG, "[%s] Mode: %d | Temp: %.1f | PIR: %d -> PWM: %lu%%", 
                     ctx->shared_state->current_time_str,
                     current_mode, // Usamos la variable local
                     incoming_data.temperature, 
                     incoming_data.presence_detected, 
                     target_pwm);
        }
    }
}