#include "discord.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

bool sendDiscord(const String &text) {
  if (strlen(DISCORD_WEBHOOK_URL) == 0 || WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();  // เข้ารหัส HTTPS แต่ไม่ตรวจใบรับรองของ Discord
  HTTPClient http;
  if (!http.begin(client, DISCORD_WEBHOOK_URL)) return false;
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);

  JsonDocument doc;
  doc["content"] = text;  // ข้อความ (ไม่เกิน 2000 ตัวอักษร)
  String body;
  serializeJson(doc, body);

  int code = http.POST(body);
  http.end();
  Serial.printf("[DISCORD] HTTP %d\n", code);
  return code == 204 || code == 200;  // Discord ตอบ 204 (No Content) เมื่อส่งสำเร็จ
}
