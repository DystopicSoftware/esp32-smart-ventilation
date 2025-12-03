#include "hal_interfaces.h"
#include <driver/ledc.h>
#include <esp_log.h>

static const char *TAG = "FAN_DRIVER";

// Configuración PWM
#define FAN_GPIO        2       // LED integrado del ESP32 (Cambia a 18 cuando tengas el RGB con resistencia)
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO  FAN_GPIO
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_DUTY_RES   LEDC_TIMER_13_BIT // Resolución de 13 bits (0-8191)
#define LEDC_FREQUENCY  5000    // Frecuencia 5 kHz (buena para LEDs y motores)

esp_err_t fan_driver_init(void) {
    // 1. Configurar Timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY, 
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. Configurar Canal
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Iniciar apagado
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ESP_LOGI(TAG, "Fan Driver (PWM) inicializado en GPIO %d", FAN_GPIO);
    return ESP_OK;
}

esp_err_t fan_driver_set_duty(uint32_t percent) {
    if (percent > 100) percent = 100;

    // Convertir porcentaje (0-100) a resolución de bits (0-8191)
    uint32_t duty = (percent * 8191) / 100;

    // Actualizar PWM
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
    
    return ESP_OK;
}

const fan_interface_t fan_driver_impl = {
    .init = fan_driver_init,
    .set_duty = fan_driver_set_duty
};