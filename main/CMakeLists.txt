idf_component_register(
    SRCS "main.c" "ssd1306.c" "google_geolocation.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash pthread esp8266 esp_http_client 
)

target_link_libraries(${COMPONENT_LIB} "-u _printf_float")