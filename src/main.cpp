/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com  
*********/

// Load Wi-Fi library
#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <WebSocketsServer.h>

// Function prototypes
void processWebsocketData(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
void setRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);

// Replace with your network credentials
const char* ssid     = "Calin-Network-2.4";
const char* password = "Trappist-1";
const char* hostname = "Lightstrip";

// Set web server port number to 80
AsyncWebServer server(80);
// And a websocket server on port 81
WebSocketsServer ws_server(81);

// Red, green, and blue pins for PWM control
const int redPin = 23;     // 13 corresponds to GPIO13
const int greenPin = 25;   // 12 corresponds to GPIO12
const int bluePin = 32;    // 14 corresponds to GPIO14

// Setting PWM frequency, channels and bit resolution
const int freq = 5000;
const int redChannel = 0;
const int greenChannel = 1;
const int blueChannel = 2;
// Bit resolution 2^12 = 4096
const int resolution = 12;

uint8_t resetCount = 0;

const char* startIndex = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\" /><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" /><link rel=\"stylesheet\" href=\"https://cdn.jsdelivr.net/npm/@simonwep/pickr/dist/themes/nano.min.css\" /> <script src=\"https://cdn.jsdelivr.net/npm/@simonwep/pickr/dist/pickr.es5.min.js\"></script> <style>.container{display:flex;flex-direction:column;justify-content:start;align-items:center;padding:1rem}.pcr-app[data-theme=nano]{width:100%;height:50%}.brightness-slider{margin-top:3rem;width:100%}input[type=range]{width:100%}</style><title>LED Lights</title></head><body><div class=\"container\"><div class=\"color-picker\"></div><div class=\"brightness-slider\"> <input type=\"range\" name=\"brightness\" id=\"brightness\" min=0 max=100 value=100 step=1 oninput=\"setBrightness(this.value)\"><div style=\"text-align: center; font-family: -apple-system, BlinkMacSystemFont, sans-serif; font-size: 1.5rem;\">Brightness</div></div><div class=\"control\" style=\"position: absolute; bottom: 2rem\"> <button onclick=\"turnOff()\">Turn off</button></div></div></body> <script>var red=0;var green=0;var blue=0;var brightness=100;const webSocket=new WebSocket(\"ws://Lightstrip.local:81\");webSocket.addEventListener(\"error\",function(err){console.log(\"Websocket error:\",err);});webSocket.addEventListener(\"open\",function(event){console.log(\"Socket opened!\");});webSocket.addEventListener(\"message\",function(event){console.log(\"LED Strip:\",event.data);});const pickr=Pickr.create({el:\".color-picker\",theme:\"nano\",default:\"#EFEBD8\",useAsButton:true,swatches:[\"#EFEBD8\",\"#F44336\",\"#9C27B0\",\"#3F51B5\",\"#2196F3\",\"#03A9F4\",\"#00BCD4\",\"#009688\",\"#4CAF50\",\"#FDD835\"],inline:true,showAlways:true,position:\"middle\",padding:0,components:{preview:true,hue:true,interaction:{hex:true,rgba:false,hsla:false,hsva:false,cmyk:false,input:true,clear:false,save:false,},},});pickr.on(\"change\",function(colorObj,instance){const rgbaColor=colorObj.toRGBA();red=Math.floor(rgbaColor[0]);green=Math.floor(rgbaColor[1]);blue=Math.floor(rgbaColor[2]);webSocket.send(`${red.toString(16)} ${green.toString(16)} ${blue.toString(16)} ${brightness.toString(16)}`);});function setBrightness(value){brightness=parseInt(value);webSocket.send(`${red.toString(16)} ${green.toString(16)} ${blue.toString(16)} ${brightness.toString(16)}`);};function turnOff() {webSocket.send(\"00 00 00 00\");}</script> </html>";

void setup() {
  Serial.begin(115200);
  // configure LED PWM functionalitites
  ledcSetup(redChannel, freq, resolution);
  ledcSetup(greenChannel, freq, resolution);
  ledcSetup(blueChannel, freq, resolution);
  
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(redPin, redChannel);
  ledcAttachPin(greenPin, greenChannel);
  ledcAttachPin(bluePin, blueChannel);
  
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);// this code solves my problem
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    resetCount++;
    if (resetCount >= 2)
    {
      ESP.restart();
    }
  }

  MDNS.begin(hostname);
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());  

  // Main webpage with LED control
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", startIndex);
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  ws_server.begin(); // Start listening on port 81
  ws_server.onEvent(processWebsocketData);
  // Default color: warm white
  setRGB(0xef, 0xeb, 0xd8, 100);
}

void loop(){
  AsyncElegantOTA.loop();
  ws_server.loop();
}

void processWebsocketData(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
  switch(type) {
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
            char * pEnd;
            uint8_t r = strtol((const char*)payload, &pEnd, 16);
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
			      break;  
    }
}

void setRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
  // Brightness goes from 0 to 100;
  uint16_t maxBrightness = (uint16_t)map(brightness, 0, 100, 0, 4095);
  // Limit the duty cycle depending on brightness
  uint16_t _r = (uint16_t)map(r, 0, 255, 0, maxBrightness);
  uint16_t _g = (uint16_t)map(g, 0, 255, 0, maxBrightness);
  uint16_t _b = (uint16_t)map(b, 0, 255, 0, maxBrightness);

  ledcWrite(redChannel, _r);
  ledcWrite(greenChannel, _g);
  ledcWrite(blueChannel, _b);
}