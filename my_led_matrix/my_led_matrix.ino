/*
   MQTT Clock & Message Board v 0.2.0
   Project by James Petersen, copyright 2018.
   This code was taken from https://github.com/jptrsn/clock-message-board and modified to fit my needs
*/
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <MD_Parola.h>
#include <SPI.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <FastLED.h>

#include "config.h"

// MD_MAX72XX::HARDWARE_SPI - use one of the values from below based on the chip:
// PAROLA_HW, GENERIC_HW, ICSTATION_HW, FC16_HW

MD_Parola P = MD_Parola(MD_MAX72XX::ICSTATION_HW, CS_PIN, MAX_DEVICES); // Works for upstairs LED Matrix
// MD_Parola P = MD_Parola(MD_MAX72XX::FC16_HW, CS_PIN, MAX_DEVICES); // works for 8 led matrix and frontroom

// WiFi login parameters - network name and password
const char* wifi_ssid         = CONFIG_WIFI_SSID;
const char* wifi_password     = CONFIG_WIFI_PASS;
const char* wifi_host_name    = CONFIG_HOST_NAME;

const char* mqtt_server       = CONFIG_MQTT_HOST;
const char* mqtt_username     = CONFIG_MQTT_USER;
const char* mqtt_password     = CONFIG_MQTT_PASS;
const char* mqtt_client_id    = CONFIG_MQTT_CLIENT_ID;
const char* message_topic     = CONFIG_MQTT_TOPIC_MESSAGE;

WiFiClient espClient;
PubSubClient client(espClient);

WiFiUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};    //Daylight time = UTC - 4 hours
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};     //Standard time = UTC - 5 hours
Timezone myTZ(myDST, mySTD);
TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev
time_t local;

uint8_t frameDelay = FRAME_DELAY_DEFAULT;  // default frame delay value
textEffect_t  effects[] =
{
  PA_PRINT,             // 0
  PA_SCAN_HORIZ,        // 1
  PA_SCROLL_LEFT,       // 2
  PA_WIPE,              // 3
  PA_SCROLL_UP_LEFT,    // 4
  PA_SCROLL_UP,         // 5
  PA_FADE,              // 6
  PA_OPENING_CURSOR,    // 7
  PA_GROW_UP,           // 8
  PA_SCROLL_UP_RIGHT,   // 9
  PA_BLINDS,            // 10
  PA_CLOSING,           // 11
  PA_GROW_DOWN,         // 12
  PA_SCAN_VERT,         // 13
  PA_SCROLL_DOWN_LEFT,  // 14
  PA_WIPE_CURSOR,       // 15
  PA_DISSOLVE,          // 16
  PA_MESH,              // 17
  PA_OPENING,           // 18
  PA_CLOSING_CURSOR,    // 19
  PA_SCROLL_DOWN_RIGHT, // 20
  PA_SCROLL_RIGHT,      // 21
  PA_SLICE,             // 22
  PA_SCROLL_DOWN,       // 23
};

// Global message buffers shared by Wifi and Scrolling functions
char curMessage[CONFIG_BUF_SIZE];
char newMessage[CONFIG_BUF_SIZE];
bool newMessageAvailable = false;
char currentTime[CONFIG_BUF_SIZE];

// Global variables that need to be available to code
textEffect_t scrollEffect = CONFIG_DEFAULT_EFFECT;
textEffect_t effectIn = effects[2];
textEffect_t effectOut = effects[2];
uint16_t messagePause = 0;

unsigned long lastMessageDisplayed;
bool updateLastMessageDisplayed;

byte lastMinute;
byte machineState = 1;
byte repeatMessage = MESSAGE_REPEAT_DEFAULT;
byte i = 0;
byte clockBrightness = CLOCK_BRIGHTNESS_DEFAULT;
byte messageBrightness = MESSAGE_BRIGHTNESS_DEFAULT;
int delayBetweenMessages = MESSAGE_DELAY_DEFAULT;

void setup() {
  delay(500);

  pinMode(LED_PIN, OUTPUT);

  lastMessageDisplayed = millis();
  if (DEBUG) {
    Serial.begin(115200);
  }

  P.begin();
  P.displayClear();
  P.setIntensity(clockBrightness);
  P.setTextEffect(PA_SCROLL_UP, PA_SCROLL_DOWN);

  P.displayScroll(curMessage, PA_CENTER, scrollEffect, FRAME_DELAY_DEFAULT);
  P.displayReset();
  curMessage[0] = newMessage[0] = '\0';

  // Connect to and initialise WiFi network
  PRINT("\nConnecting to ", wifi_ssid);
  sprintf(curMessage, "%s %s %s", "Connecting to ", wifi_ssid, "network...");

  WiFi.begin(wifi_ssid, wifi_password);
  WiFi.hostname(wifi_host_name);
  waitForMessageComplete(true);

  handleWifi(false);

  configureOTA();

  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(120);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  PRINTS("\nWiFi connected");
  getNtpTime();
  updateCurrentTime();

  P.displayReset();
  sprintf(curMessage, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  PRINT("\nAssigned IP ", curMessage);
  waitForMessageComplete(false);

  digitalWrite(LED_PIN, HIGH);
}

void loop() {

  ArduinoOTA.handle();

  if (client.connected()) {
    client.loop();
  }

  if (P.displayAnimate()) {

    // Ensure WiFi is connected. Enable error output to the MAX7219 display.
    handleWifi(true);

    // If we're not connected to the MQTT message topic, attempt to reconnect
    if (!client.connected()) {
      reconnect();
    } else {
      client.loop();
    }

    // If the flag is set to update the timestamp of the last message display, do so. If there's a new message
    // to display, do not reset the flag
    if (updateLastMessageDisplayed && !newMessageAvailable) {
      lastMessageDisplayed = millis();
      updateLastMessageDisplayed = false;
    }

    // Run state machine evaluations
    handlemachineState();
  }
}

/***********************************************************************************/
//                              State Machine methods

void handlemachineState() {
  // Write the machine state to the serial monitor if it is not in static-display mode (state 0)
  if (DEBUG && machineState) {
    PRINTS("machineState: ");
    PRINTLN(machineState);
  }

  switch (machineState) {
    case 0: { // Clock is showing the current time
        if (i > 0 && millis() - lastMessageDisplayed > delayBetweenMessages && !newMessageAvailable) {
          PRINTLN("---");
          PRINT("---------------- Message repeat ", i);
          PRINTLN(" ----------------");
          machineState = 3;
        } else if (newMessageAvailable && machineState != 1) {
          strcpy(curMessage, newMessage);
          machineState = 3;
        } 
        else 
        {
          if (clockMinuteChanged(lastMinute)) 
          {
            machineState = 5;
          } 
        }
        break;
      }
    case 1: { // Clock is transitioning from hidden to showing current time
        showTime();
        machineState = 0;
        break;
      }
    case 2: {

        break;
      }
    case 3: { // Clock is transitioning from showing current time to hidden, in order to display a new message
        PRINT("repeatMessage: ", repeatMessage);
        PRINTLN(" ");
        if (i < repeatMessage) {
          hideTime();
          newMessageAvailable = false;
          machineState = 4;
        } else {
          repeatMessage = MESSAGE_REPEAT_DEFAULT;
          frameDelay = FRAME_DELAY_DEFAULT;
          messagePause = MESSAGE_PAUSE_DEFAULT;
          P.setSpeed(frameDelay);
          i = 0;
          machineState = 0;
          PRINTLN("Do not display");
        }
        break;
      }
    case 4: { // New message received, needs to be animated
        PRINTS("display message - ");
        PRINTLN(curMessage);
        //        P.displayScroll(curMessage, PA_CENTER, PA_SCROLL_LEFT, frameDelay);
        P.displayText(curMessage, PA_CENTER, frameDelay, messagePause, effectIn, effectOut);
        i++;
        updateLastMessageDisplayed = true;
        machineState = 6;
        break;
      }
    case 5: { // Clock is displaying exit animation
        hideTime();
        machineState = 6;
        break;
      }
    case 6: { // Clock display is blank, value can be changed
        updateCurrentTime();
        machineState = 1;
        break;
      }
  }
}

/***********************************************************************************/
//                              Clock methods

bool clockMinuteChanged(byte& thisMinute) {
  if (minute() != thisMinute) {
    //    PRINTLN("Time changed");
    thisMinute = minute();
    return true;
  }
  return false;
}

void updateCurrentTime() {
  int hour12 = 0;
  //  PRINTLN("updateCurrentTime");
  local = myTZ.toLocal(now(), &tcr);
  hour12 = hour(local);
  if ( hour12 > 12)
  {
    hour12 = hour12 -12;
    if (MAX_DEVICES <= 4)
    {
      sprintf(currentTime, "%d:%02d p", hour12, minute(local));
    }
    else
    {
      sprintf(currentTime, "%d:%02d pm", hour12, minute(local));
    }
  }
  else
  {
    if (MAX_DEVICES <= 4)
    {
      sprintf(currentTime, "%d:%02d a", hour12, minute(local));
    }
    else
    {
      sprintf(currentTime, "%d:%02d am", hour12, minute(local));
    }
  }

  PRINTLN(currentTime);
  //  getNtpTime();
}

// Animate the current time off the display
void hideTime() {
  size_t arrLength = sizeof(effects)/sizeof(effects[0]);
  P.displayText(currentTime, PA_CENTER, FRAME_DELAY_DEFAULT * 2, 0, PA_PRINT, effects[random(arrLength)]);
}

// Animate the current time onto the display
void showTime() {
  size_t arrLength = sizeof(effects)/sizeof(effects[0]);
  P.displayText(currentTime, PA_CENTER, FRAME_DELAY_DEFAULT * 2, 500, effects[random(arrLength)]);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  // get a random server from the pool
  WiFi.hostByName(NTP_SERVER_POOL, ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL;
    }
  }
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

/***********************************************************************************/
//                              MQTT methods
void callback(char* topic, byte* payload, unsigned int length) {
  PRINTS("New message arrived: ");
  PRINTLN(topic);
    char message[CONFIG_BUF_SIZE];
    for (int i = 0; i <CONFIG_BUF_SIZE; i++)
      message[i] = '\0';
    
    for (int j = 0; j < length; j++) {
      message[j] = (char)payload[j];
      if ( j >=  CONFIG_BUF_SIZE)
        return;
    }
    message[length] = '\0';
    strcpy(newMessage, message);
    newMessageAvailable = true;

    PRINTS("New message arrived: ");
    PRINTLN(newMessage);
    PRINTLN(message);
}

void reconnect() {
  if (!client.connected()) {
    PRINTS("Attempting MQTT connection...");
    client.connect(mqtt_client_id, mqtt_username, mqtt_password);

    P.displayScroll("Connecting to MQTT...", PA_CENTER, scrollEffect, FRAME_DELAY_DEFAULT);
    waitForMessageComplete(false);

    if (client.connected()) {
      PRINTS("connected");
      client.subscribe(message_topic);

      P.displayScroll("Connected to MQTT", PA_CENTER, scrollEffect, FRAME_DELAY_DEFAULT);
      waitForMessageComplete(false);

    } else {
      PRINTS("failed, rc=");
      PRINTLN(client.state());
    }
    machineState = 6;
  }
}

/******************************* Utility methods ***********************************/
void handleWifi(bool displayErr) 
{
  char* errMessage;
  byte count = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    if (++count > 10) {
      ESP.restart();
    }

    digitalWrite(LED_PIN, LOW);

    char* err = err2Str(WiFi.status());
    PRINT("\n", err);
    PRINTLN("");

    if (P.displayAnimate() && displayErr) {
      sprintf(errMessage, "Error: %s", err);
      P.displayScroll(errMessage, PA_CENTER, scrollEffect, FRAME_DELAY_DEFAULT);
    }
    yield();

    digitalWrite(LED_PIN, HIGH);

    if (WiFi.status() != WL_CONNECTED) {
      PRINTLN("Retrying in .5s");
      delay(500);
    }
  }
}

char *err2Str(wl_status_t code) {
  switch (code)
  {
    case WL_IDLE_STATUS:    return ("IDLE");           break; // WiFi is in process of changing between statuses
    case WL_NO_SSID_AVAIL:  return ("NO_SSID_AVAIL");  break; // case configured SSID cannot be reached
    case WL_CONNECTED:      return ("CONNECTED");      break; // successful connection is established
    case WL_CONNECT_FAILED: return ("WRONG_PASSWORD"); break; // password is incorrect
    case WL_DISCONNECTED:   return ("CONNECT_FAILED"); break; // module is not configured in station mode
    default: return ("??");
  }
}

void waitForMessageComplete(bool blinkStatusLed) {
  int a = 0;
  PRINTLN(" ");
  bool state = 0;
  while (!P.displayAnimate()) {
    a++;
    if (a % 2500 == 0) {
      PRINTS(".");
      state = ! state;
      if (blinkStatusLed) {
        digitalWrite(LED_PIN, state);
      }
    }
    yield();
    ArduinoOTA.handle();
  }
}

void configureOTA() {
  ArduinoOTA.onStart([]() {
    PRINTS("OTA Start");
    P.displayScroll("Updating Firmware", PA_CENTER, scrollEffect, FRAME_DELAY_DEFAULT);
    waitForMessageComplete(false);
  });
  ArduinoOTA.onEnd([]() {
    PRINTS("\nEnd");
    P.displayScroll("Firmware Updated!", PA_CENTER, scrollEffect, FRAME_DELAY_DEFAULT);
    waitForMessageComplete(false);
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char* m = "-";
    if (P.displayAnimate()) {
      int percent = (progress / (total / 100));
      char* prog = "-/|\\";
      m[0] = prog[percent % 4];
      P.displayText(m, PA_CENTER, 10, 10, PA_PRINT);
    }
    PRINT("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    PRINT("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) PRINTS("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) PRINTS("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) PRINTS("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) PRINTS("Receive Failed");
    else if (error == OTA_END_ERROR) PRINTS("End Failed");
  });
  ArduinoOTA.setHostname(wifi_host_name);
  ArduinoOTA.begin();
}
