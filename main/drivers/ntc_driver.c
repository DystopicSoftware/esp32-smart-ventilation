#include "hal_interfaces.h"
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <math.h>

static const char *TAG = "NTC_DRIVER";

// --- CONFIGURACIÓN DEL SENSOR ---
#define ADC_UNIT       ADC_UNIT_1
#define ADC_CHANNEL    ADC_CHANNEL_6   // GPIO 34 corresponde al Canal 6 del ADC1
#define ADC_ATTEN      ADC_ATTEN_DB_12 // Permite medir hasta ~3.3V (aprox)

// --- CONSTANTES MATEMÁTICAS (Ajustar según tu NTC) ---
// Para tu 47D-15, la resistencia nominal es 47. Para uno normal es 10000.
#define R_NOMINAL      47.0f   
#define T_NOMINAL      25.0f   
#define B_COEFFICIENT  3000.0f // Valor Beta típico (ajustar si tienes datasheet)
#define R_SERIES       220.0f  // Valor de la resistencia fija (R1) que pongas en el circuito

static adc_oneshot_unit_handle_t adc_handle = NULL;

esp_err_t ntc_init(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &config));
    
    ESP_LOGI(TAG, "ADC Inicializado en GPIO 34");
    return ESP_OK;
}

float ntc_read_celsius(void) {
    int adc_raw = 0;
    // Leer valor crudo (0 - 4095)
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw));

    if (adc_raw == 0 || adc_raw == 4095) {
        ESP_LOGW(TAG, "Lectura ADC invalida: %d", adc_raw);
        return -99.0f; // Error
    }

    // 1. Convertir ADC a Voltaje (Aprox)
    // ESP32 ADC de 12 bits -> 4096 pasos. Vref aprox 3.3V
    float voltage = (adc_raw * 3.3f) / 4095.0f;

    // 2. Calcular Resistencia del NTC usando Divisor de Tensión
    // Vout = Vcc * R_ntc / (R_series + R_ntc)
    // Despejando R_ntc:
    float r_ntc = (voltage * R_SERIES) / (3.3f - voltage);

    // 3. Ecuación Beta (Steinhart-Hart simplificada)
    // 1/T = 1/To + 1/B * ln(R/Ro)
    float steinhart;
    steinhart = r_ntc / R_NOMINAL;      // (R/Ro)
    steinhart = log(steinhart);         // ln(R/Ro)
    steinhart /= B_COEFFICIENT;         // 1/B * ln(R/Ro)
    steinhart += 1.0f / (T_NOMINAL + 273.15f); // + 1/To
    steinhart = 1.0f / steinhart;       // Invertir
    steinhart -= 273.15f;               // Convertir Kelvin a Celsius

    // Filtro simple para evitar saltos locos en el log
    ESP_LOGD(TAG, "Raw: %d | V: %.2f | R_ntc: %.1f | Temp: %.2f", adc_raw, voltage, r_ntc, steinhart);
    
    return steinhart;
}

// Interfaz pública
const temp_sensor_interface_t ntc_sensor_impl = {
    .init = ntc_init,
    .read_celsius = ntc_read_celsius
};