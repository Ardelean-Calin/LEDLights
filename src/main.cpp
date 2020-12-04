/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com
*********/

// Load Wi-Fi library
#include <ArduinoOTA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include <TelnetStream.h>
#include <WiFi.h>
#include <math.h>

typedef struct RGBColor
{
  int r;
  int g;
  int b;
} RGBColor;

void setRGB(uint16_t r, uint16_t g, uint16_t b);
String getContentType(String filename);
bool handleFileRead(String path, AsyncWebServerRequest *request);
RGBColor hsv2rgb(float H, float S, float V);
void mqttCallback(char *topic, byte *payload, unsigned int length);
void reconnectMqtt();

// Replace with your network credentials
const char *ssid = "Calin-Network-2.4";
const char *password = "Trappist-1";
const char *hostname = "Lightstrip";

// MQTT configuration
const char *mqttServer = "192.168.1.243";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Set web server port number to 80
// AsyncWebServer server(80);

// Red, green, and blue pins for PWM control
#define PIN_RED   23 // 23 corresponds to GPIO23
#define PIN_GREEN 25 // 25 corresponds to GPIO25
#define PIN_BLUE  32 // 32 corresponds to GPIO32

// Setting PWM frequency, channels and bit resolution
#define LED_FREQUENCY (100U)
#define CHANNEL_RED   0
#define CHANNEL_GREEN 1
#define CHANNEL_BLUE  2

#define LED_RESOLUTION     (12U)
#define LED_MAX_BRIGHTNESS ((1 << LED_RESOLUTION) - 1)

#define H_MAX 360
#define S_MAX 100
#define V_MAX 100

#define RGB_UPDATE_FREQ           50
#define RGB_UPDATE_PERIOD         ((float)(1000.0 / RGB_UPDATE_FREQ))
#define RGB_ANIMATION_DURATION_MS 10000

uint8_t resetCount = 0;
float brightness = 0;
float h = 0.0, s = 100.0, v = brightness;
uint32_t lastCallTime = 0;
uint32_t currCallTime = 0;

void setup()
{
  Serial.begin(115200);
  // configure LED PWM functionalitites
  ledcSetup(CHANNEL_RED, LED_FREQUENCY, LED_RESOLUTION);
  ledcSetup(CHANNEL_GREEN, LED_FREQUENCY, LED_RESOLUTION);
  ledcSetup(CHANNEL_BLUE, LED_FREQUENCY, LED_RESOLUTION);

  // attach the channel to the GPIO to be controlled
  ledcAttachPin(PIN_RED, CHANNEL_RED);
  ledcAttachPin(PIN_GREEN, CHANNEL_GREEN);
  ledcAttachPin(PIN_BLUE, CHANNEL_BLUE);

  // Start fileserver
  SPIFFS.begin(true);

  // Connect to Wi-Fi network with SSID and password
  WiFi.begin(ssid, password);
  WiFi.setSleep(false); // this code solves my problem
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    resetCount++;
    if (resetCount >= 10)
    {
      ESP.restart();
    }
  }

  // Main webpage with LED control
  // server.onNotFound([](AsyncWebServerRequest *request) {   // If the client
  // requests any URI
  //   if (!handleFileRead(request->url(), request))          // send it if it
  //   exists
  //     request->send(404, "text/plain", "404: Not Found");  // otherwise,
  //     respond with a 404 (Not Found) error
  // });

  ArduinoOTA.onEnd([]() {
    // On upload end => restart ESP
    ESP.restart();
  });

  ArduinoOTA.begin();
  // Hostname needs to be set after OTA
  MDNS.begin(hostname);
  // server.begin();

  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(mqttCallback);

  TelnetStream.begin();

  // Default off
  setRGB(0, 0, 0);
}

void loop()
{
  ArduinoOTA.handle();
  if (!mqttClient.connected())
  {
    reconnectMqtt();
  }
  else
  {
    mqttClient.loop();
  }

  /* Runs every few milliseconds to take the RGB strip through all colors of the
   * rainbow */
  currCallTime = millis();
  if (currCallTime - lastCallTime > RGB_UPDATE_PERIOD)
  { // 50Hz
    // Periodic update of RGB LED value
    h += H_MAX / (RGB_ANIMATION_DURATION_MS / RGB_UPDATE_PERIOD);
    RGBColor xRGB = hsv2rgb(h, s, v);
    setRGB(xRGB.r, xRGB.g, xRGB.b);
    lastCallTime = currCallTime;
  }
}

void reconnectMqtt()
{
  uint8_t tries = 0;
  // Loop until we're reconnected, but only try 5 times
  while (!mqttClient.connected() && (tries < 5))
  {
    // Attempt to connect
    if (mqttClient.connect("ESP32Client"))
    {
      // Subscribe
      mqttClient.subscribe("rgbon");
      mqttClient.subscribe("rgbbrightness");
    }
    else
    {
      // Wait 5 seconds before retrying
    }
    tries++;
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  byte *data = (byte *)malloc(length * sizeof(byte) + 1);
  memcpy(data, payload, length);
  data[length] = '\0';

  if (String(topic) == "rgbon")
  {
    TelnetStream.println("rgbon");
    if (data[0] == '0')
      v = 0.0;
    else if (data[0] == '1')
      v = brightness;
  }
  else if (String(topic) == "rgbbrightness")
  {
    brightness = String((char *)data).toFloat();
    v = brightness;
  }

  free(data);
}

void setRGB(uint16_t r, uint16_t g, uint16_t b)
{
  ledcWrite(CHANNEL_RED, r);
  ledcWrite(CHANNEL_GREEN, g);
  ledcWrite(CHANNEL_BLUE, b);
}

RGBColor hsv2rgb(float H, float S, float V)
{
  float r, g, b;

  float h = H / H_MAX;
  float s = S / S_MAX;
  float v = V / V_MAX;

  int i = floor(h * 6);
  float f = h * 6 - i;
  float p = v * (1 - s);
  float q = v * (1 - f * s);
  float t = v * (1 - (1 - f) * s);

  switch (i % 6)
  {
  case 0:
    r = v, g = t, b = p;
    break;
  case 1:
    r = q, g = v, b = p;
    break;
  case 2:
    r = p, g = v, b = t;
    break;
  case 3:
    r = p, g = q, b = v;
    break;
  case 4:
    r = t, g = p, b = v;
    break;
  case 5:
    r = v, g = p, b = q;
    break;
  }

  RGBColor color;
  color.r = r * LED_MAX_BRIGHTNESS;
  color.g = g * LED_MAX_BRIGHTNESS;
  color.b = b * LED_MAX_BRIGHTNESS;

  return color;
}

String getContentType(String filename)
{
  if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path, AsyncWebServerRequest *request)
{ // send the right file to the client (if it exists)
  if (path.endsWith("/"))
    path += "index.html"; // If a folder is requested, send the index file
  String contentType = getContentType(path); // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  { // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz)) // If there's a compressed version available
      path += ".gz";
    AsyncWebServerResponse *response = request->beginResponse(
        SPIFFS, path, "text/html", false); // Send the file to the client
    if (SPIFFS.exists(pathWithGz))
      response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    return true;
  }
  return false; // If the file doesn't exist, return false
}
