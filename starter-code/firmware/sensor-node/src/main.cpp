// OnBedSystem — Sensor Node
// นับคนเดินผ่าน (KY-032) + อ่านค่ากลิ่น (MQ-137 เฉพาะ node ที่ HAS_MQ=1) แล้วส่งให้ gateway ผ่าน ESP-NOW ทุก 10 วินาที

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include "config.h"

// ===== ส่วนที่ 1: นับคนด้วย interrupt (E2) =====
volatile uint32_t beamCount = 0;      // จำนวนครั้งที่มีคนผ่านหน้า KY-032
volatile uint32_t lastBeamMs = 0;     // เวลาที่นับครั้งล่าสุด (ใช้ debounce)
portMUX_TYPE beamMux = portMUX_INITIALIZER_UNLOCKED;

// ISR ทำงานทันทีที่ขา OUT ของ KY-032 เปลี่ยนจาก HIGH เป็น LOW (มีคนเข้ามาในระยะ) ต้องสั้นที่สุด ห้าม Serial.print ห้าม delay
void IRAM_ATTR onBeamBreak() {
  uint32_t now = millis();
  portENTER_CRITICAL_ISR(&beamMux);
  if (now - lastBeamMs > BEAM_DEBOUNCE_MS) {
    beamCount++;
    lastBeamMs = now;
  }
  portEXIT_CRITICAL_ISR(&beamMux);
}

// ดึงจำนวนคนออกมาแล้วรีเซ็ตเป็น 0 (deferred processing: ประมวลผลนอก ISR)
uint32_t takeBeamCount() {
  portENTER_CRITICAL(&beamMux);
  uint32_t n = beamCount;
  beamCount = 0;
  portEXIT_CRITICAL(&beamMux);
  return n;
}

// ถ้าส่งไม่สำเร็จ คืนจำนวนคนกลับไป จะได้ไม่หาย
void returnBeamCount(uint32_t n) {
  portENTER_CRITICAL(&beamMux);
  beamCount += n;
  portEXIT_CRITICAL(&beamMux);
}

// ===== ส่วนที่ 2: อ่านค่ากลิ่น =====
// แรงดันที่ขา AO ของโมดูล (V) เฉลี่ย 20 ครั้งให้ค่านิ่ง
float readSensorVolt() {
  uint32_t sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += analogReadMilliVolts(MQ_PIN);
    delay(2);
  }
  float vPin = sum / 20.0f / 1000.0f;  // แรงดันที่ขา ESP32
  return vPin * DIVIDER_RATIO;         // คูณกลับ → แรงดันจริงที่ขา AO
}

// ค่า Rs ของเซ็นเซอร์แบบไม่มีหน่วย (ไม่ต้องรู้ค่า RL เพราะตัดกันตอนหารด้วย R0)
float readRs(float v) {
  return (MQ_VCC - v) / v;
}

// odor = R0 / Rs  → อากาศสะอาด ≈ 1.0, ยิ่งเหม็นยิ่งมากกว่า 1
// คืน -1 ถ้าสายหลุด (แรงดันเกือบ 0)
float readOdor() {
  float v = readSensorVolt();
  if (v < 0.05f) return -1.0f;
  return MQ_R0 / readRs(v);
}

// ===== ส่วนที่ 3: ESP-NOW + ส่งซ้ำ (E4 ฝั่ง node) =====
volatile bool sendDone = false;
volatile bool sendOk = false;
uint32_t seq = 0;
int failStreak = 0;

// ESP-NOW เรียกฟังก์ชันนี้หลังส่งแต่ละครั้ง บอกว่า gateway ตอบรับ (ACK) หรือไม่
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
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
    Serial.printf("WiFi '%s' not found, retry in 5 s\n", WIFI_SSID);
    delay(5000);
  }
  startEspNow(ch);
}

bool sendWithRetry(const uint8_t *data, size_t len) {
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

// ===== ส่วนที่ 4: setup / loop =====
uint32_t lastReportMs = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== Sensor node %s (MQ-137: %s) ===\n", NODE_ID, HAS_MQ ? "yes" : "no");

#if HAS_MQ
  analogSetPinAttenuation(MQ_PIN, ADC_11db);  // ให้อ่านแรงดันได้ถึง ~3.1 V
#endif

  pinMode(BEAM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BEAM_PIN), onBeamBreak, FALLING);

  WiFi.mode(WIFI_STA);
  Serial.print("My MAC: ");
  Serial.println(WiFi.macAddress());

#ifndef CALIBRATE
  findGatewayChannel();
#endif
  lastReportMs = millis();
}

void loop() {
#ifdef CALIBRATE
  // โหมด calibrate: พิมพ์ค่า Rs ทุก 1 วินาที ไม่ส่งข้อมูล
  float v = readSensorVolt();
  Serial.printf("V=%.3f  Rs=%.4f  beam=%u\n", v, v > 0.05f ? readRs(v) : -1.0f, beamCount);
  delay(1000);
  return;
#endif

  if (millis() - lastReportMs < REPORT_INTERVAL_MS) {
    delay(10);
    return;
  }
  lastReportMs = millis();

  uint32_t people = takeBeamCount();

  JsonDocument doc;
  doc["node_id"] = NODE_ID;
  doc["seq"] = seq;
#if HAS_MQ
  float odor = readOdor();
  doc["odor"] = roundf(odor * 100) / 100.0f;  // node ที่ไม่มี MQ-137 จะไม่มี field นี้
#endif
  doc["people"] = people;

  char buf[128];
  size_t len = serializeJson(doc, buf);
  bool ok = sendWithRetry((uint8_t *)buf, len);
  Serial.printf("%s  %s\n", buf, ok ? "OK" : "FAILED");
  seq++;

  if (ok) {
    failStreak = 0;
  } else {
    returnBeamCount(people);  // เก็บจำนวนคนไว้ส่งรอบหน้า
    if (++failStreak >= FAILS_BEFORE_RESCAN) {
      Serial.println("Gateway unreachable, rescanning channel...");
      findGatewayChannel();
      failStreak = 0;
    }
  }
}
