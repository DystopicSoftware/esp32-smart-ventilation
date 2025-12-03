#include "system_common.h"
#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>
#include <esp_system.h>

static const char *TAG = "WEB_SERVER";
static app_context_t *global_ctx = NULL; // Referencia local al contexto

// Prototipo de guardado (desde config_manager)
extern esp_err_t config_manager_save(const system_config_t *source_config);

/* 
 * ------------------------------------------------------------------
 * HANDLER: GET /api/status
 * Devuelve JSON con el estado actual del sistema
 * ------------------------------------------------------------------
 */
static esp_err_t api_status_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    
    // Crear objeto JSON
    cJSON *root = cJSON_CreateObject();

    // Bloquear mutex para leer datos consistentes
    if (xSemaphoreTake(global_ctx->config_mutex, portMAX_DELAY)) {
        // Datos de Configuración
        cJSON_AddNumberToObject(root, "mode", global_ctx->shared_config->operation_mode);
        
        // Datos de Estado en Tiempo Real
        cJSON_AddNumberToObject(root, "temp", global_ctx->shared_state->current_temp);
        cJSON_AddBoolToObject(root, "pir", global_ctx->shared_state->presence);
        cJSON_AddNumberToObject(root, "pwm", global_ctx->shared_state->current_pwm);
        cJSON_AddStringToObject(root, "time", global_ctx->shared_state->current_time_str);
        
        xSemaphoreGive(global_ctx->config_mutex);
    }

    // Convertir a string y enviar
    const char *json_response = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
    
    // Limpieza
    free((void *)json_response);
    cJSON_Delete(root);
    return ESP_OK;
}

/* 
 * ------------------------------------------------------------------
 * HANDLER: POST /api/settings
 * Recibe JSON para cambiar configuración (Ej: {"mode": 0, "manual_duty": 80})
 * ------------------------------------------------------------------
 */
static esp_err_t api_settings_post_handler(httpd_req_t *req) {
    char buf[100]; // Buffer para recibir el body
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Leer datos del request
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0'; // Null termination

    // Parsear JSON
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid JSON");
        return ESP_FAIL;
    }

    // Aplicar cambios
    if (xSemaphoreTake(global_ctx->config_mutex, portMAX_DELAY)) {
        cJSON *mode = cJSON_GetObjectItem(root, "mode");
        cJSON *duty = cJSON_GetObjectItem(root, "manual_duty");

        if (mode) {
            global_ctx->shared_config->operation_mode = mode->valueint;
            ESP_LOGI(TAG, "Web cambió Modo a: %d", mode->valueint);
        }
        if (duty) {
            global_ctx->shared_config->manual_duty = duty->valueint;
            ESP_LOGI(TAG, "Web cambió Duty Manual a: %d", duty->valueint);
        }

        // Guardar en NVS inmediatamente
        config_manager_save(global_ctx->shared_config);

        xSemaphoreGive(global_ctx->config_mutex);
    }

    cJSON_Delete(root);
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Estructuras de definición de rutas
static const httpd_uri_t uri_status = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = api_status_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_settings = {
    .uri       = "/api/settings",
    .method    = HTTP_POST,
    .handler   = api_settings_post_handler,
    .user_ctx  = NULL
};

// Función de inicio del servidor
void start_web_server(app_context_t *ctx) {
    global_ctx = ctx; // Guardar referencia globalmente para los handlers
    
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192; // Aumentar stack para manejar JSON

    ESP_LOGI(TAG, "Iniciando Servidor Web...");

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_status);
        httpd_register_uri_handler(server, &uri_settings);
        ESP_LOGI(TAG, "Servidor Web iniciado en puerto 80");
    } else {
        ESP_LOGE(TAG, "Error iniciando servidor web");
    }
}