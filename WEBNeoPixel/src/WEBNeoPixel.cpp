#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>
#include "../../wifi.h"

#define PIN 5 // D1 on WEMOS
#define ADDR_RED        0
#define ADDR_GREEN      1
#define ADDR_BLUE       2

const byte dim_curve[] = {
  0,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,   4,   4,
  4,   4,   4,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   6,   6,   6,
  6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   7,   8,   8,   8,   8,
  8,   8,   9,   9,   9,   9,   9,   9,   10,  10,  10,  10,  10,  11,  11,  11,
  11,  11,  12,  12,  12,  12,  12,  13,  13,  13,  13,  14,  14,  14,  14,  15,
  15,  15,  16,  16,  16,  16,  17,  17,  17,  18,  18,  18,  19,  19,  19,  20,
  20,  20,  21,  21,  22,  22,  22,  23,  23,  24,  24,  25,  25,  25,  26,  26,
  27,  27,  28,  28,  29,  29,  30,  30,  31,  32,  32,  33,  33,  34,  35,  35,
  36,  36,  37,  38,  38,  39,  40,  40,  41,  42,  43,  43,  44,  45,  46,  47,
  48,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
  63,  64,  65,  66,  68,  69,  70,  71,  73,  74,  75,  76,  78,  79,  81,  82,
  83,  85,  86,  88,  90,  91,  93,  94,  96,  98,  99,  101, 103, 105, 107, 109,
  110, 112, 114, 116, 118, 121, 123, 125, 127, 129, 132, 134, 136, 139, 141, 144,
  146, 149, 151, 154, 157, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 190,
  193, 196, 200, 203, 207, 211, 214, 218, 222, 226, 230, 234, 238, 242, 248, 255,
};
const char *project = "WEBNeoPixel";
char host[32];
enum showModes {
  MODE_ON, MODE_OFF, MODE_RAINBOW, MODE_STROBOSCOPE, MODE_THEATERCHASE,
  MODE_THEATERCHASERAINBOW
};

void handleColor();
void updateWeb();
void colorWipe(uint32_t c, uint8_t wait);
void rainbow(uint8_t wait);
void theaterChaseRainbow(uint8_t wait);
void theaterChase(uint32_t c, uint8_t wait);
void stroboscope(uint8_t wait);
uint32_t Wheel(byte WheelPos);


uint8_t showMode;
uint8_t color_index;
uint32_t color;
char webPage[1024];
ESP8266WebServer server(80);

Adafruit_NeoPixel strip = Adafruit_NeoPixel(24, PIN, NEO_GRB + NEO_KHZ800);
// Update these with values suitable for your network.
IPAddress MQTTserver(192, 168, 11, 21);

void setup() {
  Serial.begin(74880);
  Serial.println("Booting");
  sprintf(host, "%s-%s", project, String(random(0xffff), HEX).c_str());
  WiFi.hostname(host);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to sp8266-[ChipID]
  ArduinoOTA.setHostname(host);

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
      Serial.println("Start");
      });

  ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
      });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      });

  ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });

  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("Hostname: "); Serial.println(host);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  color = 0xFF00FF;
  colorWipe(color, 20);
  showMode = MODE_ON;

  server.on("/", [](){
    updateWeb();
    server.send(200, "text/html", webPage);
  });

  // turn on
  server.on("/on", [](){
    showMode = MODE_ON;
    colorWipe(color, 20);

    updateWeb();
    server.send(200, "text/html", webPage);
    Serial.println("on");
    delay(100);
  });
  // turn off
  server.on("/off", [](){
    showMode = MODE_OFF;
    colorWipe(0, 20);

    updateWeb();
    server.send(200, "text/html", webPage);
    Serial.println("off");
    delay(100);
  });

  // set color
  server.on("/color", handleColor);

  // rainbow mode
  server.on("/rainbow", [](){
    showMode = MODE_RAINBOW;

    updateWeb();
    server.send(200, "text/html", webPage);
    Serial.println("rainbow");
    delay(100);
  });

  // theaterChase mode
  server.on("/theaterChase", [](){
    showMode = MODE_THEATERCHASE;

    updateWeb();
    server.send(200, "text/html", webPage);
    Serial.println("theaterChase");
    delay(100);
  });

  // theaterChaseRainbow mode
  server.on("/theaterChaseRainbow", [](){
    showMode = MODE_THEATERCHASERAINBOW;

    updateWeb();
    server.send(200, "text/html", webPage);
    Serial.println("theaterChaseRainbow");
    delay(100);
  });

  // stroboscope mode
  server.on("/stroboscope", [](){
    showMode = MODE_STROBOSCOPE;

    updateWeb();
    server.send(200, "text/html", webPage);
    Serial.println("stroboscope");
    delay(100);
  });


  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  ArduinoOTA.handle();
  if (! WiFi.status() == WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  server.handleClient();
  switch(showMode) {
    case MODE_ON:
    case MODE_OFF:
    break;

    case MODE_RAINBOW:
    rainbow(50);
    break;

    case MODE_STROBOSCOPE:
    stroboscope(50);
    break;

    case MODE_THEATERCHASE:
    theaterChase(color, 50);
    break;

    case MODE_THEATERCHASERAINBOW:
    theaterChaseRainbow(50);
    break;
  }
}

void handleColor() {
  uint8_t color_red, color_green, color_blue;
  Serial.print("color");
  if (server.args() > 0) {
    uint8_t i;
    for ( i = 0; i < server.args(); i++ ) {
      if (server.argName(i) == "red") {
        color_red = server.arg(i).toInt() & 0xFF;
        Serial.print(" red: "); Serial.print(color_red);
      }
      else if (server.argName(i) == "green") {
        color_green = server.arg(i).toInt() & 0xFF;
        Serial.print(" green: "); Serial.print(color_green);
      }
      else if (server.argName(i) == "blue") {
        color_blue = server.arg(i).toInt() & 0xFF;
        Serial.print(" blue: "); Serial.print(color_blue);
      }
    }
  }
  Serial.println("");

  if ((color_red >= 0) && (color_green >= 0) && (color_blue >= 0)) {
    color = (color_blue) | (color_green << 8) | (color_red << 16);
    colorWipe(color, 20);
    showMode = MODE_ON;

    updateWeb();
    server.send(200, "text/html", webPage);
  }
  else server.send(200, "text/html", webPage);
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i;

  for(i=0; i<strip.numPixels(); i++) {
    color = Wheel((i+color_index) & 255);
    strip.setPixelColor(i, color);
  }
  color_index++;
  strip.show();
  delay(wait);
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int q=0; q < 3; q++) {
    for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
      strip.setPixelColor(i+q, c);    //turn every third pixel on
    }
    strip.show();
    delay(wait);

    for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
      strip.setPixelColor(i+q, 0);        //turn every third pixel off
    }
  }
}

// Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int q=0; q < 3; q++) {
    for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
      strip.setPixelColor(i+q, Wheel( (i+color_index) % 255));    //turn every third pixel on
    }
    strip.show();
    delay(wait);

    for (uint16_t i=0; i < strip.numPixels(); i=i+3) {
      strip.setPixelColor(i+q, 0);        //turn every third pixel off
    }
  }
  color_index++;
}

void stroboscope(uint8_t wait) {
  colorWipe(strip.Color(0, 0, 0), 0);
  delay(wait);
  colorWipe(strip.Color(255, 255, 255), 0);
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void updateWeb() {
  uint8_t color_red, color_green, color_blue;
  color_red = (color >> 16) & 0xFF;
  color_green = (color >> 8) & 0xFF;
  color_blue = color & 0xFF;
  sprintf(webPage, "<html><head><title>TrixiPixel</title></head><body><h1>TrixiPixel </h1>");
  sprintf(webPage, "%s <p><h2>power</h2></p>", webPage);
  sprintf(webPage, "%s <p><a href=\"on\"><button>turn On (%3d/%3d/%3d)</button></a>&nbsp;<a href=\"off\"><button>turn Off</button></a></p>", webPage, color_red, color_green, color_blue);
  sprintf(webPage, "%s <p><h2>effects</h2></p>", webPage);
  sprintf(webPage, "%s <p><a href=\"rainbow\"><button>rainbow</button></a>&nbsp;<a href=\"stroboscope\"><button>stroboscope</button></a>", webPage);
  sprintf(webPage, "%s <a href=\"theaterChase\"><button>theaterChase</button></a>&nbsp;<a href=\"theaterChaseRainbow\"><button>theaterChaseRainbow</button></a></p>", webPage);
  sprintf(webPage, "%s <p><h2>set color (0 to 255)</h2></p>", webPage);
  sprintf(webPage, "%s <form action=\"color\" method=\"POST\"><p>red: <input type=\"text\" name=\"red\" size=\"3\" maxlength=\"3\" value=\"%d\"> green: <input type=\"text\" name=\"green\" size=\"3\" maxlength=\"3\" value=\"%d\"> blue: <input type=\"text\" name=\"blue\" maxlength=\"3\" value=\"%d\"></p><p><input type=\"submit\" value=\"set\"></form>", webPage, color_red, color_green, color_blue);
  sprintf(webPage, "%s </body></html>", webPage);
}
