#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AdafruitIO_WiFi.h"
#include <map>
#include "secrets.h"

// ---------------- WiFi & Adafruit IO ----------------
AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);

// ---------------- DHT22 Setup ----------------
#define DHTPIN D4 // GPIO2
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ---------------- Web Server Setup ----------------
ESP8266WebServer server(80);

// ---------------- Hub Feeds ----------------
AdafruitIO_Feed *feedHubTemp;

// ---------------- Node Data Storage ----------------
struct NodeData {
  float temp;
};
std::map<String, NodeData> nodes;

// ---------------- OLED Setup ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- Handle Node HTTP Request ----------------
void handleNodeData() {
  if (server.hasArg("id") && server.hasArg("temp")) {
    String nodeID = server.arg("id");
    float temp = server.arg("temp").toFloat();
    nodes[nodeID] = {temp}; // store/update node data
    Serial.printf("Received %s: Temp=%.2f\n", nodeID.c_str(), temp);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

// ---------------- Handle Status Webpage ----------------
void handleStatus() {
  String html = "<html><head><title>Hub Status</title></head><body>";
  html += "<h1>Hub Status</h1>";
  html += "<p>Hub IP: " + WiFi.localIP().toString() + "</p>";
  html += "<p>Hub Temp: " + String(dht.readTemperature(true)) + " °F</p>";
  
  html += "<h2>Node Temperatures</h2><ul>";
  for (auto const& [nodeID, data] : nodes) {
    html += "<li>" + nodeID + ": " + String(data.temp) + " °F</li>";
  }
  html += "</ul></body></html>";

  server.send(200, "text/html", html);
}

// ---------------- Update OLED Display ----------------
void updateDisplay(float hubTemp) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);

  display.println("Hub Temp:");
  display.print(hubTemp, 1);
  display.println(" F");
  display.println("----------------");

  display.println("Node Temps:");
  for (auto const& [nodeID, data] : nodes) {
    display.print(nodeID);
    display.print(": ");
    display.print(data.temp, 1);
    display.println(" F");
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // 0x3C is typical I2C address
    Serial.println(F("SSD1306 allocation failed"));
    while (true); // stop here if OLED failed
  }
  display.clearDisplay();
  display.display();

  // Start WiFi & AIO
  Serial.print("Connecting to WiFi & Adafruit IO");
  io.connect();

  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected to Adafruit IO!");

  // Start mDNS (hostname: "hub")
  if (MDNS.begin("hub")) {
    Serial.println("mDNS responder started: http://hub.local");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  // Create hub feed
  feedHubTemp = io.feed("hub-temp");

  // Start web server
  server.on("/nodeData", handleNodeData);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("Server started at port 80");
}

void loop() {
  io.run();          // keep AIO running
  server.handleClient();
  MDNS.update();     // required for mDNS

  // Read hub DHT22 temperature
  float hubTemp = dht.readTemperature(true);

  if (!isnan(hubTemp)) { // read, display and publish
    Serial.print("Hub Temp: ");
    Serial.print(hubTemp);
    Serial.println(" °F");

    feedHubTemp->save(hubTemp);
  }

  // Publish all node temperatures dynamically
  for (auto const& [nodeID, data] : nodes) {
    String feedTempName = nodeID + "-temp";
    AdafruitIO_Feed *nodeTempFeed = io.feed(feedTempName.c_str());
    nodeTempFeed->save(data.temp);

    Serial.print("Node ");
    Serial.print(nodeID);
    Serial.print(" Temp: ");
    Serial.println(data.temp);
  }

  // Update OLED display
  updateDisplay(hubTemp);

  delay(5000);
}
