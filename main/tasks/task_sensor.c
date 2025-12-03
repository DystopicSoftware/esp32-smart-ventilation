#include "system_common.h"
#include "hal_interfaces.h"
#include <esp_log.h>

// Dependencias inyectadas
extern const temp_sensor_interface_t temp_mock_impl;
extern const pir_sensor_interface_t pir_mock_impl;

void sensor_task(void *pvParameters) {
    app_context_t *ctx = (app_context_t *)pvParameters;
    const char *TAG = "TASK_SENSOR";
    
    // Inicializar Mocks
    temp_mock_impl.init();
    pir_mock_impl.init();

    sensor_data_t data;

    while (1) {
        // 1. Leer Hardware (Abstracto)
        data.temperature = temp_mock_impl.read_celsius();
        data.presence_detected = pir_mock_impl.is_motion_detected();
        data.timestamp = 0; // TODO: Usar gettimeofday() cuando NTP esté listo

        // 2. Enviar a Cola
        if (xQueueSend(ctx->sensor_queue, &data, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Queue full!");
        }

        // Frecuencia de muestreo: 1 segundo
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}