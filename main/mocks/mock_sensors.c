#include "hal_interfaces.h"
#include <stdlib.h> // para rand()
#include <esp_log.h>

static const char *TAG = "MOCK_SENSORS";

// Simula temperatura variando levemente alrededor de 25°C
float mock_temp_read(void) {
    float fluctuation = ((float)(rand() % 200) / 100.0f) - 1.0f; // -1.0 a +1.0
    float val = 25.0f + fluctuation;
    ESP_LOGD(TAG, "Mock Temp Reading: %.2f", val);
    return val;
}

// Simula movimiento aleatorio (20% de probabilidad de cambio)
static bool current_motion = false;
bool mock_pir_read(void) {
    if ((rand() % 100) < 20) {
        current_motion = !current_motion;
    }
    ESP_LOGD(TAG, "Mock PIR: %s", current_motion ? "YES" : "NO");
    return current_motion;
}

esp_err_t mock_init(void) { return ESP_OK; }

// Instancias de interfaces llenas con funciones mock
const temp_sensor_interface_t temp_mock_impl = { .init = mock_init, .read_celsius = mock_temp_read };
const pir_sensor_interface_t pir_mock_impl = { .init = mock_init, .is_motion_detected = mock_pir_read };