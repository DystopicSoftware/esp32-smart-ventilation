#include "system_common.h"
#include "hal_interfaces.h"
#include <esp_log.h>

static const char *TAG = "TASK_SENSOR"; // ¡Aquí está la corrección del error de imagen!

// Importar interfaces disponibles
extern const temp_sensor_interface_t temp_mock_impl; // Mock Temp (Usar este hoy)
extern const temp_sensor_interface_t ntc_sensor_impl; // Real Temp (Falta resistencia)

extern const pir_sensor_interface_t pir_mock_impl;   // Mock PIR
extern const pir_sensor_interface_t pir_driver_impl; // Real PIR (Usar este hoy)

void sensor_task(void *pvParameters) {
    app_context_t *ctx = (app_context_t *)pvParameters;
    
    // --- CAMBIO AQUÍ: Usamos el NTC Real ---
    const temp_sensor_interface_t *temp_sensor = &ntc_sensor_impl; // <--- YA NO ES EL MOCK
    
    const pir_sensor_interface_t *pir_sensor = &pir_driver_impl;   // PIR Real

    temp_sensor->init();
    pir_sensor->init();

    sensor_data_t data;

    while (1) {
        data.temperature = temp_sensor->read_celsius();
        data.presence_detected = pir_sensor->is_motion_detected();
        data.timestamp = 0; 

        if (xQueueSend(ctx->sensor_queue, &data, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Queue full!");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}