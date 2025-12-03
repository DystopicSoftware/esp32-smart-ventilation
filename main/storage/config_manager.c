#include "system_common.h"
#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h> // para memcpy

static const char *TAG = "NVS_MGR";
static const char *NVS_NAMESPACE = "vent_config";
static const char *KEY_CONFIG = "sys_cfg";

// Configuración por defecto (Si es la primera vez que arranca)
// Configuración por defecto
static const system_config_t default_config = {
    .operation_mode = 2, // <--- CAMBIADO A 2 (MODE_SCHEDULE)
    .manual_duty = 50,
    .schedules = {
        // REGISTRO 0: Activo todo el día (00:00 a 23:59) para pruebas
        { 
            .active = true, 
            .start_hour=0, .start_min=0, 
            .end_hour=23, .end_min=59, 
            .temp_min_0_percent=23.0,   // Empezar a girar a los 23°C
            .temp_max_100_percent=26.0  // Máximo a los 26°C
        },
        {0}, {0}
    }
};

esp_err_t config_manager_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t config_manager_load(system_config_t *target_config) {
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    size_t required_size = sizeof(system_config_t);
    
    // Intentar leer la estructura completa
    err = nvs_get_blob(my_handle, KEY_CONFIG, target_config, &required_size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "No config found, loading defaults...");
        memcpy(target_config, &default_config, sizeof(system_config_t));
        // Guardar la default inmediatamente
        nvs_set_blob(my_handle, KEY_CONFIG, target_config, sizeof(system_config_t));
        nvs_commit(my_handle);
    } else if (err == ESP_OK) {
        ESP_LOGI(TAG, "Configuration loaded from NVS");
    }

    nvs_close(my_handle);
    return ESP_OK; // Si no se encontró, devolvemos OK porque ya cargamos default
}

esp_err_t config_manager_save(const system_config_t *source_config) {
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(my_handle, KEY_CONFIG, source_config, sizeof(system_config_t));
    if (err == ESP_OK) {
        err = nvs_commit(my_handle);
        ESP_LOGI(TAG, "Configuration saved to NVS");
    }
    nvs_close(my_handle);
    return err;
}