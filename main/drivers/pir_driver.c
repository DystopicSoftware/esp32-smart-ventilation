#include "hal_interfaces.h"
#include <driver/gpio.h>
#include <esp_log.h>

static const char *TAG = "PIR_DRIVER";
#define PIR_GPIO    14  // Conecta el pin OUT del PIR al GPIO 14

esp_err_t pir_driver_init(void) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIR_GPIO);
    io_conf.pull_down_en = 0; // El HW-416 suele tener salida activa, no necesita pull
    io_conf.pull_up_en = 0;
    
    esp_err_t err = gpio_config(&io_conf);
    ESP_LOGI(TAG, "PIR Driver inicializado en GPIO %d", PIR_GPIO);
    return err;
}

bool pir_driver_read(void) {
    // Retorna true si hay movimiento (HIGH), false si no (LOW)
    return gpio_get_level(PIR_GPIO) == 1;
}

const pir_sensor_interface_t pir_driver_impl = {
    .init = pir_driver_init,
    .is_motion_detected = pir_driver_read
};