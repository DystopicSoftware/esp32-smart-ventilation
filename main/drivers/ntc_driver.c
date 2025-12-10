#include "hal_interfaces.h"
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <math.h>

static const char *TAG = "NTC_DRIVER";

// --- CONFIGURACIÓN DEL SENSOR ---
#define ADC_UNIT       ADC_UNIT_1
#define ADC_CHANNEL    ADC_CHANNEL_6   // GPIO 34 corresponde al Canal 6 del ADC1
#define ADC_ATTEN      ADC_ATTEN_DB_12 // Permite medir hasta ~3.3V (aprox)

// --- CONSTANTES AJUSTADAS PARA TU HARDWARE ---
#define R_NOMINAL      47.0f   // Tu NTC es de 47 Ohmios
#define T_NOMINAL      25.0f   // A 25 grados centígrados
#define B_COEFFICIENT  3000.0f // Valor estimado para NTCs de potencia (no es perfecto, pero sirve)

// ¡IMPORTANTE! Pon aquí el valor de la resistencia que encontraste (ej: 100, 220, 330)
#define R_SERIES       100.0f  

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
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_raw));

    if (adc_raw == 0 || adc_raw == 4095) {
        ESP_LOGW(TAG, "Lectura ADC invalida: %d", adc_raw);
        return -99.0f;
    }

    // 1. Convertir ADC a Voltaje
    float voltage = (adc_raw * 3.3f) / 4095.0f;

    // 2. Calcular Resistencia del NTC
    float r_ntc = (voltage * R_SERIES) / (3.3f - voltage);

    // 3. Ecuación Beta
    float steinhart;
    steinhart = r_ntc / R_NOMINAL;      
    steinhart = log(steinhart);         
    steinhart /= B_COEFFICIENT;         
    steinhart += 1.0f / (T_NOMINAL + 273.15f); 
    steinhart = 1.0f / steinhart;       
    steinhart -= 273.15f;               

    // --- CALIBRACIÓN (AQUÍ ESTÁ EL CAMBIO) ---
    // Restamos la diferencia: 31.5 (medido) - 19.0 (real) = 9.5
    steinhart = steinhart - 9.5f; 

    ESP_LOGD(TAG, "Raw: %d | V: %.2f | R: %.1f | Temp Calc: %.2f", adc_raw, voltage, r_ntc, steinhart);
    
    return steinhart;
}

// Interfaz pública
const temp_sensor_interface_t ntc_sensor_impl = {
    .init = ntc_init,
    .read_celsius = ntc_read_celsius
};