// Author: Ryan Chiang
// Date: 5/21/2026
// Project: NFC Garage Opener
// 		ESP32-based local garage trigger using an official LiftMaster remote.
// 		Triggered via iPhone NFC Shortcut -> HTTP POST -> GPIO pulse.
//
// Design goals:
// - No cloud dependency
// - No garage opener modification
// - Preserve LiftMaster rolling-code RF authentication behavior
// - LAN-only with token check for network safety

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

const char* WIFI_SSID     = "MY_WIFI_NAME";
const char* WIFI_PASSWORD = "MY_WIFI_PASSWORD";
const char* AUTH_TOKEN    = "MY_TOKEN";
const char* MDNS_NAME     = "garage";

// GPIO driving the transistor that electronically 
// "presses" the LiftMaster remote button.
const int TOGGLE_PIN      = 23; 

// ON time for the "button push"
const int PULSE_MS        = 300;

// Minimum delay between accepted trigger requests.
const int COOLDOWN_MS     = 5000;

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

WebServer server(80);
unsigned long lastTriggerMs = 0;

void pulseButton() {
  digitalWrite(TOGGLE_PIN, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(PULSE_MS);
  digitalWrite(TOGGLE_PIN, LOW);
  digitalWrite(LED_BUILTIN, LOW);
}

void handleToggle() {
  // Must have good token to proceed
  if (!server.hasHeader("X-Auth-Token") || server.header("X-Auth-Token") != AUTH_TOKEN) {
    Serial.println("Rejected: bad or missing token");
    server.send(401, "text/plain", "unauthorized\n");
    return;
  }

  // Make sure no back-to-back triggers
  unsigned long now = millis();
  if (now - lastTriggerMs < COOLDOWN_MS) {
    Serial.println("Rejected: cooldown active");
    server.send(429, "text/plain", "cooldown\n");
    return;
  }
  lastTriggerMs = now;

  // Simulate one momentary press of the LiftMaster remote button.
  Serial.println("Pulsing button");
  pulseButton();
  server.send(200, "text/plain", "ok\n");
}

void handleStatus() {
  server.send(200, "text/plain", "alive\n");
}

void setup() {
  Serial.begin(115200); // Set baud rate
  delay(100);

  // Initialize trigger output LOW immediately to prevent
  // unintended activation during boot or reset.
  pinMode(TOGGLE_PIN, OUTPUT);
  digitalWrite(TOGGLE_PIN, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Connect to local Wi-Fi network.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.print("MAC: "); Serial.println(WiFi.macAddress());

  if (MDNS.begin(MDNS_NAME)) {
    Serial.printf("mDNS: http://%s.local\n", MDNS_NAME);
  } else {
    Serial.println("mDNS failed -- use IP directly");
  }

  // Register expected auth header so WebServer can read it.
  const char* headerKeys[] = {"X-Auth-Token"};
  server.collectHeaders(headerKeys, 1);
  
  // Register HTTP endpoints
  server.on("/toggle", HTTP_POST, handleToggle);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
