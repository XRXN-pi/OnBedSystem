#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Sensor node 1 ===\n");

  WiFi.mode(WIFI_STA);
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  
}
