#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include "config.h"

#ifdef IR_BEAM

#endif

#ifdef ULTRASONIC
#include <Ultrasonic.h>

Ultrasonic us(US_TRIG_PIN, US_ECHO_PIN);
TaskHandle_t UltrasonicTask;
QueueHandle_t UltrasonicQueue = NULL;

void UltrasonicTaskCode(void* pvParameters) {
  UltrasonicQueue = xQueueCreate(REPORT_INTERVAL_MS / US_INTERVAL_MS * (MAX_ATTEMPTS + 2), sizeof(int));

  for (;;) {
    int value = us.read(CM);
    xQueueSend(UltrasonicQueue, &value, (TickType_t)10);
    delay(US_INTERVAL_MS);
  }
}
#endif

#ifdef WATER_LEVEL

#endif

#pragma region telemetry
TaskHandle_t TelemetryTask;

volatile bool sendDone = false;
volatile bool sendOk = false;
uint32_t seq = 0;
int failStreak = 0;

void buildDocument(JsonDocument& doc) {
#ifdef IR_BEAM
#endif
#ifdef ULTRASONIC
  if (UltrasonicQueue != NULL) {
    int value;
    while (xQueueReceive(UltrasonicQueue, &value, (TickType_t)10) == pdTRUE) {
      doc["ultrasonic"].add(value);
    }
  }
#endif
#ifdef WATER_LEVEL
#endif
}

// ESP-NOW เรียกฟังก์ชันนี้หลังส่งแต่ละครั้ง บอกว่า gateway ตอบรับ (ACK) หรือไม่
void onDataSent(const uint8_t* mac, esp_now_send_status_t status) {
  sendOk = (status == ESP_NOW_SEND_SUCCESS);
  sendDone = true;
}

// หา Wi-Fi channel ของ hotspot ที่ gateway ต่ออยู่ (ESP-NOW ต้องใช้ channel เดียวกัน)
int scanForChannel() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  int found = -1;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      found = WiFi.channel(i);
      break;
    }
  }
  WiFi.scanDelete();
  return found;
}

void startEspNow(int channel) {
  esp_now_deinit();  // ถ้าเคยเปิดไว้ ปิดก่อน
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed, restarting...");
    delay(1000);
    ESP.restart();
  }
  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, GATEWAY_MAC, 6);
  peer.channel = channel;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
  Serial.printf("ESP-NOW ready on channel %d\n", channel);
}

void findGatewayChannel() {
  int ch;
  while ((ch = scanForChannel()) < 0) {
#ifndef OPT_TELEM
    Serial.printf("WiFi '%s' not found, retry in 5 s\n", WIFI_SSID);
    delay(5000);
#else
    Serial.printf("WiFi '%s' not found, canceling\n", WIFI_SSID);
    return;
#endif
  }
  startEspNow(ch);
}

bool sendWithRetry(const uint8_t* data, size_t len) {
  for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    sendDone = false;
    sendOk = false;
    if (esp_now_send(GATEWAY_MAC, data, len) == ESP_OK) {
      uint32_t t0 = millis();
      while (!sendDone && millis() - t0 < 100) delay(1);  // รอผลไม่เกิน 100 ms
      if (sendDone && sendOk) return true;
    }
    Serial.printf("  send failed (attempt %d/%d)\n", attempt, MAX_ATTEMPTS);
    delay(50 * attempt);  // รอนานขึ้นเรื่อยๆ ก่อนส่งซ้ำ
  }
  return false;
}

void TelemetryTaskCode(void* pvParameters) {
  for (;;) {
    delay(REPORT_INTERVAL_MS);

    JsonDocument doc;
    doc["node_id"] = NODE_ID;
    doc["seq"] = seq;
    buildDocument(doc);

    unsigned char buf[512];
    size_t len = serializeJson(doc, buf);
    bool ok = sendWithRetry(buf, len);
    Serial.printf("%s  %s\n", buf, ok ? "OK" : "FAILED");
    seq++;

    if (ok) {
      failStreak = 0;
    } else {
      if (++failStreak >= FAILS_BEFORE_RESCAN) {
        Serial.println("Gateway unreachable, rescanning channel...");
        findGatewayChannel();
        failStreak = 0;
      }
    }
  }
}
#pragma endregion telemetry

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== Sensor node %s ===\n", NODE_ID);

  WiFi.mode(WIFI_STA);
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  findGatewayChannel();

  xTaskCreate(
    TelemetryTaskCode,
    "TelemetryTask",
    10000,
    NULL,
    1,
    &TelemetryTask
  );

#ifdef ULTRASONIC
  xTaskCreate(
    UltrasonicTaskCode,
    "TelemetryTask",
    10000,
    NULL,
    2,
    &UltrasonicTask
  );
#endif
}

void loop() {

}

