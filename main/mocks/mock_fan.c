#include "hal_interfaces.h"
#include <esp_log.h>

static const char *TAG = "MOCK_FAN";

esp_err_t mock_fan_set_duty(uint32_t percent) {
    // Aquí verás en la consola la salida del sistema
    ESP_LOGI(TAG, ">> FAN OUTPUT SET TO: %lu %% <<", percent);
    return ESP_OK;
}

esp_err_t mock_fan_init(void) { return ESP_OK; }

const fan_interface_t fan_mock_impl = { .init = mock_fan_init, .set_duty = mock_fan_set_duty };