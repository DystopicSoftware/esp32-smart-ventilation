#include "system_common.h"
#include "web_page.h" 
#include <esp_http_server.h>
#include <esp_log.h>
#include <cJSON.h>
#include <esp_system.h>

static const char *TAG = "WEB_SERVER";
static app_context_t *global_ctx = NULL;

extern esp_err_t config_manager_save(const system_config_t *source_config);

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t api_status_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();

    if (xSemaphoreTake(global_ctx->config_mutex, portMAX_DELAY)) {
        cJSON_AddNumberToObject(root, "mode", global_ctx->shared_config->operation_mode);
        cJSON_AddNumberToObject(root, "manual_duty", global_ctx->shared_config->manual_duty);
        cJSON_AddNumberToObject(root, "temp", global_ctx->shared_state->current_temp);
        cJSON_AddBoolToObject(root, "pir", global_ctx->shared_state->presence);
        cJSON_AddNumberToObject(root, "pwm", global_ctx->shared_state->current_pwm);
        cJSON_AddStringToObject(root, "time", global_ctx->shared_state->current_time_str);

        cJSON *schedules = cJSON_CreateArray();
        for(int i=0; i<3; i++) {
            cJSON *item = cJSON_CreateObject();
            schedule_reg_t *reg = &global_ctx->shared_config->schedules[i];
            cJSON_AddBoolToObject(item, "act", reg->active);
            cJSON_AddNumberToObject(item, "sh", reg->start_hour);
            cJSON_AddNumberToObject(item, "sm", reg->start_min);
            cJSON_AddNumberToObject(item, "eh", reg->end_hour);
            cJSON_AddNumberToObject(item, "em", reg->end_min);
            cJSON_AddNumberToObject(item, "tmin", reg->temp_min_0_percent);
            cJSON_AddNumberToObject(item, "tmax", reg->temp_max_100_percent);
            cJSON_AddItemToArray(schedules, item);
        }
        cJSON_AddItemToObject(root, "schedules", schedules);
        xSemaphoreGive(global_ctx->config_mutex);
    }

    const char *json_response = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
    free((void *)json_response);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t api_settings_post_handler(httpd_req_t *req) {
    char buf[1024]; 
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) { httpd_resp_send_500(req); return ESP_FAIL; }
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid JSON"); return ESP_FAIL; }

    if (xSemaphoreTake(global_ctx->config_mutex, portMAX_DELAY)) {
        cJSON *mode = cJSON_GetObjectItem(root, "mode");
        cJSON *duty = cJSON_GetObjectItem(root, "manual_duty");
        cJSON *idx  = cJSON_GetObjectItem(root, "sched_idx");

        if (mode) global_ctx->shared_config->operation_mode = mode->valueint;
        if (duty) global_ctx->shared_config->manual_duty = duty->valueint;

        if (idx) {
            int i = idx->valueint;
            if (i >= 0 && i < 3) {
                schedule_reg_t *reg = &global_ctx->shared_config->schedules[i];
                cJSON *act = cJSON_GetObjectItem(root, "act");
                cJSON *sh = cJSON_GetObjectItem(root, "sh");
                cJSON *sm = cJSON_GetObjectItem(root, "sm");
                cJSON *eh = cJSON_GetObjectItem(root, "eh");
                cJSON *em = cJSON_GetObjectItem(root, "em");
                cJSON *tmin = cJSON_GetObjectItem(root, "tmin");
                cJSON *tmax = cJSON_GetObjectItem(root, "tmax");

                if (act) reg->active = cJSON_IsTrue(act);
                if (sh) reg->start_hour = sh->valueint;
                if (sm) reg->start_min = sm->valueint;
                if (eh) reg->end_hour = eh->valueint;
                if (em) reg->end_min = em->valueint;
                if (tmin) reg->temp_min_0_percent = (float)tmin->valuedouble;
                if (tmax) reg->temp_max_100_percent = (float)tmax->valuedouble;
            }
        }
        config_manager_save(global_ctx->shared_config);
        xSemaphoreGive(global_ctx->config_mutex);
    }
    cJSON_Delete(root);
    httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL };
static const httpd_uri_t uri_status = { .uri = "/api/status", .method = HTTP_GET, .handler = api_status_get_handler, .user_ctx = NULL };
static const httpd_uri_t uri_settings = { .uri = "/api/settings", .method = HTTP_POST, .handler = api_settings_post_handler, .user_ctx = NULL };

void start_web_server(app_context_t *ctx) {
    global_ctx = ctx;
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192; // Necesario para JSON grande
    config.max_uri_handlers = 8;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_root);
        httpd_register_uri_handler(server, &uri_status);
        httpd_register_uri_handler(server, &uri_settings);
        ESP_LOGI(TAG, "Web Server OK");
    }
}