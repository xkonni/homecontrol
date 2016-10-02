#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include "../../wifi.h"

// Transmitter is connected to Arduino Pin #4
#define PIN 4
#define MQTT_KEEPALIVE 45
#define NUM_TOPICS 8

const char* host = "esp8266PowerSwitchLR";
// const char* host = "esp8266PowerSwitchKO";
char clientId[32];

const char *topics[NUM_TOPICS] = {
  "PowerSwitch1A", "PowerSwitch1B", "PowerSwitch1C", "PowerSwitch1D",
  "PowerSwitch2A", "PowerSwitch2B", "PowerSwitch2C", "PowerSwitch2D" };
char *channels[NUM_TOPICS] {
  "00001", "00001", "00001", "00001",
  "00010", "00010", "00010", "00010" };
char *switches[NUM_TOPICS] {
  "10000", "01000", "00100", "00010",
  "10000", "01000", "00100", "00010" };

RCSwitch mySwitch = RCSwitch();
WiFiClient wclient;
// Server running MQTT
IPAddress MQTTserver(192, 168, 11, 21);
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

  int t;
  for (t = 0; t < NUM_TOPICS; t++) {
    if (! strcmp(topic, topics[t])) {
      if (! strncmp((char*) payload, "true", length)) {
        mySwitch.switchOn(channels[t], switches[t]);
        Serial.print(topics[t]); Serial.println(" on");
      }
      else {
        mySwitch.switchOff(channels[t], switches[t]);
        Serial.print(topics[t]); Serial.println(" off");
      }
    }
  }
}

void setup() {
  /*
   * serial
   */
  Serial.begin(76800);
  Serial.println("Booting");
  sprintf(clientId, "%s-%s", host, String(random(0xffff), HEX).c_str());

  /*
   * rcswitch
   */
  mySwitch.enableTransmit(PIN);
  // Optional set pulse length.
  // mySwitch.setPulseLength(320);
  // Optional set protocol (default is 1, will work for most outlets)
  // mySwitch.setProtocol(2);
  // Optional set number of transmission repetitions.
  mySwitch.setRepeatTransmit(5);

  /*
   * WiFi
   */
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
  Serial.print("ClientId: "); Serial.println(clientId);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // MQTT callback
  client.setCallback(callback);
}

void loop() {

  // handle OTA updates
  ArduinoOTA.handle();

  // handle client connections
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      Serial.print("connect clientId: ");
      Serial.println(clientId);
      if (client.connect(clientId)) {
        int t;
        for (t = 0; t < NUM_TOPICS; t++) {
          client.subscribe(topics[t]);
          Serial.print("Subscribe ["); Serial.print(t);
          Serial.print("] "); Serial.println(topics[t]);
          client.loop();
        }
      }
    }
    if (client.connected()) client.loop();
  }
}
