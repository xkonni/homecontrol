#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DHT.h>
#include "../../wifi.h"

extern "C" {
#include "user_interface.h"
}

#define PIN         4     // D2 on WEMOS
#define DHTTYPE     DHT22 // DHT11 or DHT22
#define DEEPSLEEP   600   // deep sleep for 600 sec

const char *project = "esp8266DHT";
char host[32];

os_timer_t myTimer;

DHT dht(PIN, DHTTYPE);
WiFiClient wclient;
// Update these with values suitable for your network.
IPAddress MQTTserver(192, 168, 11, 21);
//PubSubClient client(wclient);
PubSubClient client(MQTTserver, 1883, wclient);

void callback (char* topic, byte* payload, uint8_t length) {
  // handle message arrived
  char* myPayload = (char *) malloc(sizeof(char) * (length + 1));
  strncpy(myPayload, (char *) payload, length);
  myPayload[length] = '\0';
  Serial.print(topic);
  Serial.print(" => ");
  Serial.println(myPayload);
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

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(host);

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

  dht.begin();
}

void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect(host)) {
        client.subscribe("home/konni/temperature");
        client.loop();
        client.subscribe("home/konni/humidity");
        client.loop();
      }
    }

    if (client.connected()) {
      // read
      float t = dht.readTemperature();
      float h = dht.readHumidity();

      // make sure we're receiving a valid number
      if (isnan(t) || (isnan(h)) || (t == 0)) {
        Serial.println("DHT: reading failed, retrying in 5sec");
        delay(5000);
        return;
      }

      // publish
      client.publish("home/konni/temperature", String(t).c_str());
      client.loop();
      client.publish("home/konni/humidity", String(h).c_str());
      client.loop();

      // go to deep sleep mode
      // make sure to connect D0 with RST
      // device restarts at setup()!
      Serial.print("going to deep sleep for "); Serial.print(DEEPSLEEP);
      Serial.println(" seconds");
      ESP.deepSleep(DEEPSLEEP*1000000, WAKE_RF_DEFAULT);
    }
  }
}
