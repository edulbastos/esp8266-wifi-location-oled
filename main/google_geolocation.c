#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "google_geolocation.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "GEOLOCATION";

#define GOOGLE_GEOLOCATION_URL "https://www.googleapis.com/geolocation/v1/geolocate?key=%s"
#define MAX_HTTP_RECV_BUFFER 2048
#define MAX_HTTP_SEND_BUFFER 1536

static char http_recv_buffer[MAX_HTTP_RECV_BUFFER];
static int http_recv_len = 0;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (http_recv_len + evt->data_len < MAX_HTTP_RECV_BUFFER) {
                memcpy(http_recv_buffer + http_recv_len, evt->data, evt->data_len);
                http_recv_len += evt->data_len;
                http_recv_buffer[http_recv_len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Função simples para extrair valor double de JSON
static double parse_json_double(const char *json, const char *key)
{
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);

    char *pos = strstr(json, search_key);
    if (!pos) {
        return 0.0;
    }

    pos += strlen(search_key);
    while (*pos == ' ' || *pos == '\t') pos++;

    return atof(pos);
}

// Função para construir JSON manualmente
static int build_request_json(char *buffer, size_t buffer_size,
                              wifi_ap_record_t *ap_records, uint16_t ap_count)
{
    int offset = 0;

    offset += snprintf(buffer + offset, buffer_size - offset,
                      "{\"considerIp\":false,\"wifiAccessPoints\":[");

    int max_aps = (ap_count > 10) ? 10 : ap_count;
    for (int i = 0; i < max_aps; i++) {
        if (i > 0) {
            offset += snprintf(buffer + offset, buffer_size - offset, ",");
        }

        offset += snprintf(buffer + offset, buffer_size - offset,
                          "{\"macAddress\":\"%02X:%02X:%02X:%02X:%02X:%02X\","
                          "\"signalStrength\":%d,\"channel\":%d}",
                          ap_records[i].bssid[0], ap_records[i].bssid[1],
                          ap_records[i].bssid[2], ap_records[i].bssid[3],
                          ap_records[i].bssid[4], ap_records[i].bssid[5],
                          ap_records[i].rssi,
                          ap_records[i].primary);
    }

    offset += snprintf(buffer + offset, buffer_size - offset, "]}");

    return offset;
}

esp_err_t google_geolocation_get_location(const char *api_key,
                                          wifi_ap_record_t *ap_records,
                                          uint16_t ap_count,
                                          location_t *location)
{
    if (!api_key || !ap_records || ap_count == 0 || !location) {
        return ESP_ERR_INVALID_ARG;
    }

    // Construir JSON com dados dos APs WiFi
    char *json_buffer = malloc(MAX_HTTP_SEND_BUFFER);
    if (!json_buffer) {
        return ESP_ERR_NO_MEM;
    }

    int json_len = build_request_json(json_buffer, MAX_HTTP_SEND_BUFFER,
                                      ap_records, ap_count);

    ESP_LOGI(TAG, "Request JSON (%d bytes): %s", json_len, json_buffer);

    // Preparar URL com API key
    char url[256];
    snprintf(url, sizeof(url), GOOGLE_GEOLOCATION_URL, api_key);

    // Configurar cliente HTTP
    http_recv_len = 0;
    memset(http_recv_buffer, 0, sizeof(http_recv_buffer));

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
        .buffer_size = 1024,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(json_buffer);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_buffer, json_len);

    esp_err_t err = esp_http_client_perform(client);

    free(json_buffer);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP POST request failed with status code: %d", status_code);
        ESP_LOGE(TAG, "Response: %s", http_recv_buffer);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Response: %s", http_recv_buffer);

    // Parse manual da resposta JSON
    location->latitude = parse_json_double(http_recv_buffer, "lat");
    location->longitude = parse_json_double(http_recv_buffer, "lng");
    location->accuracy = parse_json_double(http_recv_buffer, "accuracy");

    if (location->latitude == 0.0 && location->longitude == 0.0) {
        ESP_LOGE(TAG, "Failed to parse location from response");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Location: lat=%.6f, lng=%.6f, accuracy=%.1fm",
             location->latitude, location->longitude, location->accuracy);

    return ESP_OK;
}
