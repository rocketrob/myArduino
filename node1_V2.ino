#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <DHT.h>
#include "secrets.h"

// ------------ NODE SETTINGS -------------
#define NODE_ID "node1"      // MUST be unique per node
#define HUB_HOST "hub.local" // Use mDNS hostname of hub
#define HUB_PORT 80

// ------------ DHT22 ---------------------
#define DHTPIN D4   // GPIO2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ------------ WiFi ----------------------
WiFiClient client;
HTTPClient http;

void setup() {
  Serial.begin(115200);
  dht.begin();

  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("Node IP: ");
  Serial.println(WiFi.localIP());

  // Start mDNS for hub discovery
  if (MDNS.begin("node1")) { // optional if you want node.local
    Serial.println("mDNS responder started");
  }
}

void loop() {
  // Reconnect WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost. Reconnecting...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nWiFi reconnected");
  }

  float temp = dht.readTemperature(true); // true = Fahrenheit

  if (isnan(temp)) {
    Serial.println("Failed to read DHT22");
    delay(5000);
    return;
  }

  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.println(" F");

  // Build request URL using mDNS hostname
  String url = "http://" + String(HUB_HOST) +
               "/nodeData?id=" + NODE_ID +
               "&temp=" + String(temp, 2);

  http.begin(client, url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.print("Hub response: ");
    Serial.println(httpCode);
  } else {
    Serial.print("Error sending data: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();

  delay(5000);  // send every 5 seconds
}
