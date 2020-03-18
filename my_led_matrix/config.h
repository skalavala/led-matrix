/* Sample configuration file. You will want to re-name or clone this file, and call it config.h
 * 
 * For a generic ESP8266 module, ensure that flash memory is configured properly. For OTA support, a
 * profile of 1 MB (128k SPIFFS) has been tested to work properly.
 * 
 * To control the WS2812B strip using Home Assistant, your configuration.yaml file should look like the example
 * included in the repository.
 * 
 */

#define DEBUG                                   1                   // Enables/disables debugging to the serial terminal. Not available over WiFi.

// WiFi Credentials
#define CONFIG_WIFI_SSID                        "xxx"               // Change this to your WiFi SSID
#define CONFIG_WIFI_PASS                        "xxx"               // Change to your WiFi password

// NTP definitions
#define NTP_SERVER_POOL                         "us.pool.ntp.org"   // USA. See http://www.pool.ntp.org/en/ to find a different server pool
#define NTP_OFFSET                              -14400              // Eastern Time offset in minutes
#define NTP_UPDATE_EVERY_N_MILLISECONDS         60000               // How long between clock updates?

// MQTT Connection Definitions
#define CONFIG_MQTT_HOST                        "192.168.1.xxx"       // MQTT host IP address
#define CONFIG_MQTT_USER                        "xxx"                 // MQTT user name
#define CONFIG_MQTT_PASS                        "xxx"                 // MQTT password
#define CONFIG_HOST_NAME                        "message_board"       // For mDNS and OTA identification.
#define CONFIG_MQTT_CLIENT_ID                   String(ESP.getChipId()).c_str();

#define MAX_DEVICES                             4                     // MAX7219 Definitions. See https://majicdesigns.github.io/MD_MAX72XX/ for information

// Pin assignments for Generic ESP8266 module
#define CLK_PIN                                 14                  // SCK    (D5 PIN)
#define DATA_PIN                                13                  // MOSI   (D7 PIN)
#define CS_PIN                                  16                  // CS     (D0 PIN)
#define WS_PIN                                  12                  // WS2812
#define LED_PIN                                 2

// MQTT Topics
#define CONFIG_MQTT_TOPIC_MESSAGE               "messageboard/messages"           // topic containing JSON object

// Global settings
#define CONFIG_BUF_SIZE                         512
#define CONFIG_SENSOR_DELAY                     60000                 // Milliseconds between sensor readings
#define MESSAGE_BRIGHTNESS_DEFAULT              5                     // Default brightness of MAX7219 when displaying a message
#define CLOCK_BRIGHTNESS_DEFAULT                1                     // Default brightness of MAX7219 when displaying time
#define FRAME_DELAY_DEFAULT                     50                    // Default animation speed of MAX7219
#define MESSAGE_DELAY_DEFAULT                   10000                 // Minimum milliseconds between message display
#define MESSAGE_REPEAT_DEFAULT                  3                     // Number of times to repeat the message unless otherwise specified.
#define CONFIG_DEFAULT_EFFECT                   PA_SCROLL_LEFT        // The default text effect for displaying messages
#define MESSAGE_PAUSE_DEFAULT                   0                     // The time to pause for each message display. For scrolling defaults, recommended to leave at 0
//#define MILLION                                 1000000               // Required for library

// Debugging print statements
// DO NOT MODIFY unless you know what you're doing!
#if  DEBUG
#define PRINT(s, x) { Serial.print(F(s)); Serial.print(x); }
#define PRINTS(x) Serial.print(F(x))
#define PRINTX(x) Serial.println(x, HEX)
#define PRINTLN(x) Serial.println(x)
#else
#define PRINT(s, x)
#define PRINTS(x)
#define PRINTX(x)
#define PRINTLN(x)
#endif
