#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "ssd1306.h"
#include "google_geolocation.h"

static const char *TAG = "MAIN";

// Configurações de WiFi e API (definidas no sdkconfig via menuconfig)
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASSWORD
#define GOOGLE_API_KEY CONFIG_GOOGLE_API_KEY

// Configurações de pinos do OLED (ajuste conforme seu hardware)
#define OLED_SDA_PIN        12   // GPIO12 (D6 no NodeMCU v3 ESP8266 com OLED)
#define OLED_SCL_PIN        14   // GPIO14 (D5 no NodeMCU v3 ESP8266 com OLED)

// Intervalo de atualização da localização (em segundos, definido no sdkconfig)
#define LOCATION_UPDATE_INTERVAL_SEC    CONFIG_LOCATION_UPDATE_INTERVAL

// Event group para sincronização WiFi
static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;

// Armazena a última localização obtida
static location_t current_location = {0};
static bool location_valid = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        // Configurar servidores DNS explicitamente
        tcpip_adapter_dns_info_t dns_info;

        // DNS primário: Google DNS (8.8.8.8)
        dns_info.ip.addr = ipaddr_addr("8.8.8.8");
        tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_MAIN, &dns_info);

        // DNS secundário: Cloudflare DNS (1.1.1.1)
        dns_info.ip.addr = ipaddr_addr("1.1.1.1");
        tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_BACKUP, &dns_info);

        ESP_LOGI(TAG, "DNS configured: 8.8.8.8 (primary), 1.1.1.1 (backup)");

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi init finished. Connecting to SSID: %s", WIFI_SSID);

    return ESP_OK;
}

static void display_location_info(void)
{
    ssd1306_clear();

    if (!location_valid) {
        ssd1306_draw_string(0, 0, "WiFi Location - Wait...");
        // ssd1306_draw_string(0, 12, "Aguardando...");
    } else {
        char buf[32];

        ssd1306_draw_string(0, 0, "WiFi Location");

        snprintf(buf, sizeof(buf), "Lat: %.7f", current_location.latitude);
        ssd1306_draw_string(0, 20, buf);

        snprintf(buf, sizeof(buf), "Lng: %.7f", current_location.longitude);
        ssd1306_draw_string(0, 32, buf);

        snprintf(buf, sizeof(buf), "Acc: %.2fm", current_location.accuracy);
        ssd1306_draw_string(0, 44, buf);
    }

    ssd1306_display();
}

static void location_task(void *pvParameters)
{
    wifi_ap_record_t ap_records[20];
    uint16_t ap_count = 0;

    while (1) {
        // Aguardar conexão WiFi
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                            false, true, portMAX_DELAY);

        ESP_LOGI(TAG, "Starting WiFi scan (while connected)...");

        // Fazer scan SEM desconectar (evita o erro "add mismatch")
        wifi_scan_config_t scan_config = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = true,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        };

        esp_err_t err = esp_wifi_scan_start(&scan_config, true);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // Obter resultados do scan
        ap_count = sizeof(ap_records) / sizeof(ap_records[0]);
        err = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TAG, "Found %d access points", ap_count);

        // Mostrar APs detectados
        for (int i = 0; i < ap_count && i < 10; i++) {
            ESP_LOGI(TAG, "AP %d: %s (RSSI: %d, Channel: %d, MAC: %02X:%02X:%02X:%02X:%02X:%02X)",
                     i,
                     ap_records[i].ssid,
                     ap_records[i].rssi,
                     ap_records[i].primary,
                     ap_records[i].bssid[0], ap_records[i].bssid[1],
                     ap_records[i].bssid[2], ap_records[i].bssid[3],
                     ap_records[i].bssid[4], ap_records[i].bssid[5]);
        }

        // Obter localização usando Google Geolocation API
        if (ap_count > 0) {
            location_t new_location;
            err = google_geolocation_get_location(GOOGLE_API_KEY,
                                                  ap_records,
                                                  ap_count,
                                                  &new_location);

            if (err == ESP_OK) {
                current_location = new_location;
                location_valid = true;
                ESP_LOGI(TAG, "Location updated successfully");

                // Atualizar display
                display_location_info();
            } else {
                ESP_LOGE(TAG, "Failed to get location");
                // Manter localização anterior se houver
            }
        }

        // Aguardar próximo ciclo
        vTaskDelay(pdMS_TO_TICKS(LOCATION_UPDATE_INTERVAL_SEC * 1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "WiFi Location with OLED Display");

    // Inicializar NVS (necessário para WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializar display OLED
    ret = ssd1306_init(OLED_SDA_PIN, OLED_SCL_PIN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OLED display");
        return;
    }

    // Mostrar mensagem inicial
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "WiFi Location - Wait!");
    // ssd1306_draw_string(0, 12, "Iniciando...");
    ssd1306_display();

    // Inicializar WiFi
    wifi_init_sta();

    // Aguardar conexão inicial
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Criar task de localização (6KB stack para HTTPS/TLS)
    xTaskCreate(location_task, "location_task", 6144, NULL, 5, NULL);

    ESP_LOGI(TAG, "System initialized");
}
