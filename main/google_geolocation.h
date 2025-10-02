#ifndef GOOGLE_GEOLOCATION_H
#define GOOGLE_GEOLOCATION_H

#include "esp_err.h"
#include "esp_wifi.h"

typedef struct {
    double latitude;
    double longitude;
    float accuracy;
} location_t;

/**
 * Obtém a localização usando a API do Google Geolocation
 * @param api_key Chave da API do Google
 * @param ap_records Array de APs WiFi detectados
 * @param ap_count Número de APs no array
 * @param location Ponteiro para armazenar a localização obtida
 * @return ESP_OK se sucesso, erro caso contrário
 */
esp_err_t google_geolocation_get_location(const char *api_key,
                                          wifi_ap_record_t *ap_records,
                                          uint16_t ap_count,
                                          location_t *location);

#endif // GOOGLE_GEOLOCATION_H
