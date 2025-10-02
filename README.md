# WiFi Geolocation with OLED Display - ESP8266

ESP8266 project that determines geographic location (latitude and longitude) using WiFi network triangulation with Google Geolocation API, displaying the information on an SSD1306 OLED display.

## Features

- **WiFi Network Scanning**: Detects available WiFi networks and their signal strength (RSSI)
- **Location Triangulation**: Uses Google Geolocation API to determine coordinates based on detected WiFi networks
- **OLED Display**: Shows latitude, longitude, and location accuracy on SSD1306 display (128x64)
- **Automatic Updates**: Updates location at configurable intervals (default: 300 seconds)
- **Configurable via menuconfig**: All credentials and settings stored in sdkconfig

## Hardware Requirements

- ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
- SSD1306 OLED Display 128x64 (I2C interface)
- Connection wires

### OLED Connections

| OLED Pin | ESP8266 Pin | GPIO  |
|----------|-------------|-------|
| VCC      | 3.3V        | -     |
| GND      | GND         | -     |
| SCL      | D5          | GPIO14|
| SDA      | D6          | GPIO12|

**Note**: If using different pins, change `OLED_SDA_PIN` and `OLED_SCL_PIN` definitions in [main/main.c](main/main.c).

## Configuration

This project uses ESP-IDF's configuration system (menuconfig) to manage credentials and settings.

### 1. Initial Setup

Copy the example configuration file:

```bash
cp sdkconfig.defaults.example sdkconfig.defaults
```

### 2. Configure via menuconfig

```bash
idf.py menuconfig
```

Navigate to **"WiFi Location Configuration"** and set:

- **WiFi SSID**: Your WiFi network name
- **WiFi Password**: Your WiFi password
- **Google Geolocation API Key**: Your Google API key (see below)
- **Location Update Interval**: Time between location updates (30-3600 seconds)

### 3. Or Edit sdkconfig.defaults Directly

Edit the [sdkconfig.defaults](sdkconfig.defaults) file (create from .example):

```ini
CONFIG_WIFI_SSID="YOUR_WIFI_SSID"
CONFIG_WIFI_PASSWORD="YOUR_WIFI_PASSWORD"
CONFIG_GOOGLE_API_KEY="YOUR_GOOGLE_API_KEY"
CONFIG_LOCATION_UPDATE_INTERVAL=300
```

### 4. Getting Google Geolocation API Key

1. Access [Google Cloud Console](https://console.cloud.google.com/)
2. Create a new project (or use an existing one)
3. Enable the **Geolocation API**
4. Create an API Key
5. Add the key to your configuration (see steps above)

**Important**: For security, the `sdkconfig.defaults` file is ignored by git. Use `sdkconfig.defaults.example` as a template.

### 5. OLED Pins (optional)

If needed, adjust OLED display pins in [main/main.c](main/main.c):

```c
#define OLED_SDA_PIN        12   // GPIO12 (D6 on NodeMCU)
#define OLED_SCL_PIN        14   // GPIO14 (D5 on NodeMCU)
```

## Building and Flashing

### Prerequisites

```bash
# Set up ESP8266 RTOS SDK environment
export IDF_PATH=/path/to/ESP8266_RTOS_SDK
source $IDF_PATH/export.sh
```

### Using idf.py (Recommended)

```bash
# Configure settings (first time or to change settings)
idf.py menuconfig

# Build, flash and monitor
idf.py build flash monitor
```

### Using Make (Legacy)

```bash
# Build
make

# Flash
make flash

# Monitor serial output
make monitor
```

## Project Structure

```
.
├── main/
│   ├── main.c                    # Main application code
│   ├── ssd1306.c/h              # OLED display driver
│   ├── google_geolocation.c/h   # Google API client
│   ├── Kconfig.projbuild        # Configuration menu definitions
│   ├── component.mk             # Component Makefile
│   └── CMakeLists.txt           # Component CMake file
├── Makefile                      # Main Makefile
├── CMakeLists.txt               # Main CMake file
├── sdkconfig.defaults.example   # Example configuration (template)
├── .gitignore                   # Git ignore rules
├── .gitattributes               # Git attributes
└── README.md                    # This file
```

## How It Works

1. **Initialization**: System initializes WiFi and OLED display
2. **WiFi Connection**: Connects to configured WiFi network
3. **Network Scan**: Scans for visible WiFi networks
4. **Data Collection**: Captures MAC address, RSSI (signal strength), and channel for each network
5. **API Request**: Sends data to Google Geolocation API via HTTPS
6. **Processing**: Google API returns latitude, longitude, and accuracy
7. **Display**: Shows information on OLED display
8. **Update Loop**: Repeats the process at configured interval

## Display Information

The OLED display shows:
- **Line 1**: "WiFi Location" (title)
- **Line 2**: Latitude (format: Lat: XX.XXXXXX)
- **Line 3**: Longitude (format: Lng: XX.XXXXXX)
- **Line 4**: Accuracy in meters (format: Acc: XXXm)

## Troubleshooting

### Display Not Working
- Check I2C connections (SDA/SCL)
- Confirm display I2C address (default: 0x3C)
- Test with I2C scanner to verify device detection

### Google API Error
- Verify API Key is correct
- Confirm Geolocation API is enabled in Google Cloud Console
- Check serial monitor logs for API response details

### WiFi Connection Issues
- Verify SSID and password
- Check if ESP8266 is within signal range
- Some routers may have restrictions for IoT devices

### Inaccurate Location
- Accuracy depends on number of detected WiFi networks
- Areas with few WiFi networks will have lower accuracy
- Typical accuracy ranges from 20-1000 meters

## Limitations

- Google API has free request limits (check Google documentation)
- Accuracy depends on Google's WiFi network database
- Requires Internet connection to function

## Security Notes

- **Never commit `sdkconfig.defaults`** with your credentials to version control
- The `.gitignore` file is configured to exclude sensitive files
- Use `sdkconfig.defaults.example` as a template for others

## License

This project is provided as an educational example. Use at your own risk.

## References

- [ESP8266 RTOS SDK](https://github.com/espressif/ESP8266_RTOS_SDK)
- [Google Geolocation API](https://developers.google.com/maps/documentation/geolocation/overview)
- [SSD1306 OLED Display](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Author

elbastos(at)gmail.com
Created for IoT geolocation experiments with ESP8266.
