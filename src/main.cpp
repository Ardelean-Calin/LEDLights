/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com
*********/

// Load Wi-Fi library
#include <ArduinoOTA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <SPIFFS.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

// Function prototypes
void processWebsocketData(uint8_t num, WStype_t type, uint8_t *payload,
                          size_t length);
void setRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
String getContentType(String filename);
bool handleFileRead(String path, AsyncWebServerRequest *request);

// Replace with your network credentials
const char *ssid = "Calin-Network-2.4";
const char *password = "Trappist-1";
const char *hostname = "Lightstrip";

// Set web server port number to 80
AsyncWebServer server(80);
// And a websocket server on port 81
WebSocketsServer ws_server(81);

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

uint8_t resetCount = 0;

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
    if (resetCount >= 2)
    {
      ESP.restart();
    }
  }

  // Main webpage with LED control
  server.onNotFound([](AsyncWebServerRequest *request) {   // If the client requests any URI
    if (!handleFileRead(request->url(), request))          // send it if it exists
      request->send(404, "text/plain", "404: Not Found");  // otherwise, respond with a 404 (Not Found) error
  });

  ArduinoOTA.onEnd([]() {
    // On upload end => restart ESP
    ESP.restart();
  });

  ArduinoOTA.begin();
  // Hostname needs to be set after OTA
  MDNS.begin(hostname);
  server.begin();
  ws_server.begin(); // Start listening on port 81
  ws_server.onEvent(processWebsocketData);
  // Default off
  setRGB(0, 0, 0, 0);
}

void loop()
{
  ArduinoOTA.handle();
  ws_server.loop();
}

void processWebsocketData(uint8_t num, WStype_t type, uint8_t *payload,
                          size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    break;
  case WStype_CONNECTED:
  {
    // send message to client
    ws_server.sendTXT(num, "WebSocket Connected!");
  }
  break;
  case WStype_TEXT:
  {
    char *pEnd;
    uint8_t r = strtol((const char *)payload, &pEnd, 16);
    uint8_t g = strtol(pEnd, &pEnd, 16);
    uint8_t b = strtol(pEnd, &pEnd, 16);
    uint8_t l = strtol(pEnd, &pEnd, 16);
    setRGB(r, g, b, l);
    break;
  }
  case WStype_BIN:
  {
    // webSocket.sendBIN(num, payload, length);
    break;
  }
  case WStype_ERROR:
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
  case WStype_PING:
  case WStype_PONG:
    break;
    break;
    break;
  }
}

String getContentType(String filename){
  if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path, AsyncWebServerRequest *request){  // send the right file to the client (if it exists)
  if(path.endsWith("/")) path += "index.html";           // If a folder is requested, send the index file
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){  // If the file exists, either as a compressed archive, or normal
    if(SPIFFS.exists(pathWithGz))                          // If there's a compressed version available
      path += ".gz";    
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, path, "text/html", false)   ;        // Send the file to the client
    if (SPIFFS.exists(pathWithGz))
      response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    return true;
  }
  return false;                                          // If the file doesn't exist, return false
}

void setRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
  // Brightness goes from 0 to 100;
  uint16_t maxBrightness =
      (uint16_t)map(brightness, 0, 100, 0, LED_MAX_BRIGHTNESS);
  // Limit the duty cycle depending on brightness
  uint16_t _r = (uint16_t)map(r, 0, 255, 0, maxBrightness);
  uint16_t _g = (uint16_t)map(g, 0, 255, 0, maxBrightness);
  uint16_t _b = (uint16_t)map(b, 0, 255, 0, maxBrightness);

  ledcWrite(CHANNEL_RED, _r);
  ledcWrite(CHANNEL_GREEN, _g);
  ledcWrite(CHANNEL_BLUE, _b);
}