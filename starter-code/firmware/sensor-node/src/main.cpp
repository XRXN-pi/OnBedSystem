// OnBedSystem — Sensor Node
// WC-1 (ประตู): นับคนด้วย KY-032 (ISR) + ตรวจว่ามีคนอยู่ในห้องด้วย ultrasonic
// WC-2 (พื้น) : ตรวจพื้นเปียกด้วย water level module และส่งทันทีเมื่อสถานะเปลี่ยน
// ทั้งคู่ส่งให้ gateway ผ่าน ESP-NOW ทุก 10 วินาที firmware เดียวกัน ต่างกันแค่ env ใน platformio.ini

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include "config.h"

// ===== ส่วนที่ 1: นับคนด้วย interrupt (E2) =====
#if HAS_BEAM
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
#endif

// ===== ส่วนที่ 2: ultrasonic (มีคนอยู่ในห้องไหม) =====
#if HAS_ULTRASONIC
// คืนระยะเป็นเซนติเมตร, -1 = ไม่มีเสียงสะท้อนกลับ (ไกลเกินหรือต่อสายผิด)
int readDistanceCm() {
  digitalWrite(US_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(US_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(US_TRIG_PIN, LOW);
  unsigned long us = pulseIn(US_ECHO_PIN, HIGH, 30000);  // รอไม่เกิน 30 ms
  if (us == 0) return -1;
  return (int)(us / 58);                                 // แปลงเวลาเป็น ซม.
}
#endif

// ===== ส่วนที่ 3: water level (พื้นเปียกไหม) =====
#if HAS_LEAK
int readLeak() {
  return analogRead(LEAK_PIN) > LEAK_THRESHOLD ? 1 : 0;
}
#endif

// ===== ส่วนที่ 4: ESP-NOW + ส่งซ้ำ (E4 ฝั่ง node) =====
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

// ===== ส่วนที่ 5: setup / loop =====
uint32_t lastReportMs = 0;
uint32_t lastPollMs = 0;

#if HAS_ULTRASONIC
int occupiedState = 0;
int distCm = -1;
#endif
#if HAS_LEAK
int leakState = 0;
#endif

// สร้างข้อความแล้วส่ง (ใช้ทั้งรอบปกติทุก 10 วินาที และตอนพื้นเปลี่ยนสถานะ)
void buildAndSend(const char *reason) {
#if HAS_BEAM
  uint32_t people = takeBeamCount();
#endif

  JsonDocument doc;
  doc["node_id"] = NODE_ID;
  doc["seq"] = seq;
#if HAS_BEAM
  doc["people"] = people;
#endif
#if HAS_ULTRASONIC
  doc["occupied"] = occupiedState;
  doc["dist_cm"] = distCm < 0 ? 0 : distCm;
#endif
#if HAS_LEAK
  doc["leak"] = leakState;
#endif

  char buf[160];
  size_t len = serializeJson(doc, buf);
  bool ok = sendWithRetry((uint8_t *)buf, len);
  Serial.printf("%s  %s%s\n", buf, ok ? "OK" : "FAILED", reason);
  seq++;

  if (ok) {
    failStreak = 0;
  } else {
#if HAS_BEAM
    returnBeamCount(people);  // เก็บจำนวนคนไว้ส่งรอบหน้า
#endif
    if (++failStreak >= FAILS_BEFORE_RESCAN) {
      Serial.println("Gateway unreachable, rescanning channel...");
      findGatewayChannel();
      failStreak = 0;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== Sensor node %s (KY-032: %s, ultrasonic: %s, water level: %s) ===\n",
                NODE_ID, HAS_BEAM ? "yes" : "no", HAS_ULTRASONIC ? "yes" : "no", HAS_LEAK ? "yes" : "no");

#if HAS_BEAM
  pinMode(BEAM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BEAM_PIN), onBeamBreak, FALLING);
#endif
#if HAS_ULTRASONIC
  pinMode(US_TRIG_PIN, OUTPUT);
  pinMode(US_ECHO_PIN, INPUT);
#endif
#if HAS_LEAK
  analogSetPinAttenuation(LEAK_PIN, ADC_11db);  // ให้อ่านแรงดันได้ถึง ~3.1 V
#endif

  WiFi.mode(WIFI_STA);
  Serial.print("My MAC: ");
  Serial.println(WiFi.macAddress());

  findGatewayChannel();
  lastReportMs = millis();
  lastPollMs = millis();
}

void loop() {
  // --- อ่านเซ็นเซอร์ทุก 0.5 วินาที ---
  if (millis() - lastPollMs >= SENSOR_POLL_MS) {
    lastPollMs = millis();

#if HAS_ULTRASONIC
    distCm = readDistanceCm();
    occupiedState = (distCm > 0 && distCm < OCCUPIED_CM) ? 1 : 0;
#endif

#if HAS_LEAK
    int leak = readLeak();
    if (leak != leakState) {
      // พื้นเพิ่งเปียกหรือเพิ่งแห้ง ส่งทันทีไม่รอครบ 10 วินาที เพื่อให้ gateway แจ้ง Discord ได้ทัน
      leakState = leak;
      buildAndSend(leak ? "  (leak!)" : "  (dry)");
      lastReportMs = millis();
    }
#endif
  }

  // --- ส่งตามรอบปกติทุก 10 วินาที ---
  if (millis() - lastReportMs >= REPORT_INTERVAL_MS) {
    lastReportMs = millis();
    buildAndSend("");
  }

  delay(10);
}
