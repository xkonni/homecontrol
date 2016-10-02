#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include "../../wifi.h"

#define PIN 5 // D1 on WEMOS

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
const char *project = "esp8266NeoPixel";
char host[32];

uint32_t getRGB(int hue, int sat, int val);

uint32_t color;
int hue = 0;
int brightness = 255;
float saturation = 255;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(24, PIN, NEO_GRB + NEO_KHZ800);
WiFiClient wclient;
// Update these with values suitable for your network.
IPAddress MQTTserver(192, 168, 11, 21);
//PubSubClient client(wclient);
PubSubClient client(MQTTserver, 1883, wclient);

void callback (char* topic, byte* payload, uint8_t length) {
  uint16_t i;

  // handle message arrived
  char* myPayload = (char *) malloc(sizeof(char) * (length + 1));
  strncpy(myPayload, (char *) payload, length);
  myPayload[length] = '\0';
  Serial.print(topic);
  Serial.print(" => ");
  Serial.println(myPayload);


  if (! strcmp(topic, "NeoPixel")) {
    if (! strncmp((char*) payload, "true", length)) {
      // reset brightness if zero
      if (brightness == 0) brightness = 255;
      // reset saturation
      if (saturation == 0) saturation = 255;

      color = getRGB(hue, saturation, brightness);
      for(i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, color);
      }
      strip.show();
    }
    else {
      // turn off, set brightness to 0
      brightness = 0;

      color = getRGB(hue, saturation, brightness);
      for (i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, color);
      }
      strip.show();
    }
  }

  else if (! strcmp(topic, "NeoPixelBrightness")) {
    // [0; 100] -> [0; 255]
    brightness = atoi(myPayload) * 255 / 100;
    color = getRGB(hue, saturation, brightness);
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, color);
    }
    strip.show();
  }
  else if (! strcmp(topic, "NeoPixelHue")) {
    // [0; 360]
    hue = atoi(myPayload);
    color = getRGB(hue, saturation, brightness);
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, color);
    }
    strip.show();
  }

  else if (! strcmp(topic, "NeoPixelSaturation")) {
    // [0; 100] -> [0; 255]
    saturation = atoi(myPayload) * 255 / 100;
    color = getRGB(hue, saturation, brightness);
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, color);
    }
    strip.show();
  }
}

void setup() {
  Serial.begin(115200);
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

  // Hostname defaults to esp8266-[ChipID]
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

  // MQTT callback
  client.setCallback(callback);

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
}

void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect(host)) {
        client.subscribe("NeoPixel");
        client.loop();
        client.subscribe("NeoPixelBrightness");
        client.loop();
        client.subscribe("NeoPixelHue");
        client.loop();
        client.subscribe("NeoPixelSaturation");
        client.loop();
        // client.subscribe("getNeoPixel");
        // client.loop();
        // client.subscribe("getNeoPixelBrightness");
        // client.loop();
        // client.subscribe("getNeoPixelSaturation");
        // client.loop();
        // client.subscribe("getNeoPixelHue");
        // client.loop();
        Serial.println("connected.");
      }
    }

    if (client.connected()) client.loop();
  }
}

uint32_t getRGB(int hue, int sat, int val) {
  uint32_t color = 0;

  val = dim_curve[val];
  sat = 255-dim_curve[255-sat];

  int r;
  int g;
  int b;
  int base;

  if (sat == 0) { // Acromatic color (gray). Hue doesn't mind.
    r = val;
    g = val;
    b = val;
  } else  {

    base = ((255 - sat) * val)>>8;

    switch(hue/60) {
      case 0:
        r = val;
        g = (((val-base)*hue)/60)+base;
        b = base;
        break;

      case 1:
        r = (((val-base)*(60-(hue%60)))/60)+base;
        g = val;
        b = base;
        break;

      case 2:
        r = base;
        g = val;
        b = (((val-base)*(hue%60))/60)+base;
        break;

      case 3:
        r = base;
        g = (((val-base)*(60-(hue%60)))/60)+base;
        b = val;
        break;

      case 4:
        r = (((val-base)*(hue%60))/60)+base;
        g = base;
        b = val;
        break;

      case 5:
        r = val;
        g = base;
        b = (((val-base)*(60-(hue%60)))/60)+base;
        break;
    }
  }
  color = (r << 16) | (g << 8) | b;
  // Serial.printf("colors: %u %u %u, color: 0x%06x\n", r, g, b, color);
  return color;
}
