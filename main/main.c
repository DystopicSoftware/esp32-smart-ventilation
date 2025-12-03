#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system_common.h"
#include "esp_log.h"

// Prototipos
void sensor_task(void *pvParameters);
void control_task(void *pvParameters);
esp_err_t config_manager_init(void);
esp_err_t config_manager_load(system_config_t *target_config);
void wifi_init_sta(void);
void start_web_server(app_context_t *ctx); // <--- NUEVO

static app_context_t app_ctx;
static system_config_t global_config;
static system_state_t global_state; // <--- NUEVA variable estática

void app_main(void) {
    // 1. Inicializar Storage
    config_manager_init();
    config_manager_load(&global_config);

    // 2. Inicializar WiFi
    wifi_init_sta();

    // 3. Inicializar Contexto
    app_ctx.sensor_queue = xQueueCreate(5, sizeof(sensor_data_t));
    app_ctx.actuator_queue = xQueueCreate(5, sizeof(fan_command_t));
    app_ctx.config_mutex = xSemaphoreCreateMutex();
    app_ctx.shared_config = &global_config;
    app_ctx.shared_state = &global_state; // <--- Asignar puntero

    // 4. Iniciar Tareas Core
    xTaskCreate(sensor_task, "SensorTask", 4096, &app_ctx, 5, NULL);
    xTaskCreate(control_task, "ControlTask", 4096, &app_ctx, 5, NULL);

    // 5. Iniciar Servidor Web
    start_web_server(&app_ctx); // <--- LANZAMIENTO

    ESP_LOGI("MAIN", "System 3.0 Running: Web Server Active");
}