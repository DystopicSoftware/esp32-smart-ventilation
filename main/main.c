#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_common.h"
#include "esp_log.h"

// Prototipos
void sensor_task(void *pvParameters);
void control_task(void *pvParameters);

// Nuevos prototipos
esp_err_t config_manager_init(void);
esp_err_t config_manager_load(system_config_t *target_config);
void wifi_init_sta(void);

static app_context_t app_ctx;
static system_config_t global_config;

void app_main(void) {
    // 1. Inicializar NVS y Cargar Config
    config_manager_init();
    config_manager_load(&global_config); // Carga desde flash o pone defaults

    // 2. Inicializar WiFi y NTP (Bloqueante hasta tener IP por simplicidad ahora)
    wifi_init_sta();

    // 3. Inicializar recursos del SO
    app_ctx.sensor_queue = xQueueCreate(5, sizeof(sensor_data_t));
    app_ctx.actuator_queue = xQueueCreate(5, sizeof(fan_command_t));
    app_ctx.config_mutex = xSemaphoreCreateMutex();
    app_ctx.shared_config = &global_config;

    // 4. Crear Tareas
    xTaskCreate(sensor_task, "SensorTask", 4096, &app_ctx, 5, NULL);
    xTaskCreate(control_task, "ControlTask", 4096, &app_ctx, 5, NULL);

    ESP_LOGI("MAIN", "System 2.0 Running: NVS + WiFi + NTP Active");
}