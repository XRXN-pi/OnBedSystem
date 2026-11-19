// OnBedSystem — Gateway
// รับข้อมูลจาก sensor node (ESP-NOW) + อ่านเซ็นเซอร์ของตัวเอง (KY-015, ultrasonic, water level) + คุมพัดลม + ส่งขึ้น NETPIE
// ใช้ FreeRTOS 3 task (E1), buffer ตอนเน็ตหลุด (E4), TLS (E5), OTA (E6)

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DHT.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"
#include "secrets.h"
#include "discord.h"

// =====================================================================
// ส่วนที่ 1: ข้อมูลที่ใช้ร่วมกันระหว่าง task
// =====================================================================

// ข้อความดิบจาก ESP-NOW ที่ส่งผ่าน queue
struct RawPacket {
  uint8_t data[250];
  int len;
};

// สถานะของ sensor node แต่ละตัว
struct NodeState {
  bool used = false;
  char id[12] = "";       // เช่น "WC-1"
  char key[12] = "";      // เช่น "wc1" ใช้เป็นชื่อ field บน cloud
  float odor = 0;
  bool hasOdor = false;       // node นี้มี MQ-137 ไหม (node ที่ไม่มีจะไม่ส่ง field odor มา)
  uint32_t peopleAccum = 0;  // จำนวนคนสะสมตั้งแต่ส่งขึ้น cloud ครั้งก่อน
  uint32_t lastSeq = 0;
  uint32_t received = 0;     // จำนวนข้อความที่ได้รับทั้งหมดตั้งแต่เปิดเครื่อง
  uint32_t lastSeenMs = 0;
};

// ภาพรวมระบบ ณ เวลาหนึ่ง (เก็บใน buffer ได้)
struct NodeSnap {
  float odor;
  bool hasOdor;
  uint16_t people;
  uint32_t seq;
  uint32_t recv;
  uint8_t online;
};
struct Snapshot {
  uint32_t ts;  // เวลา (Unix time, วินาที)
  uint16_t ms;  // มิลลิวินาที (ใช้วัด latency)
  NodeSnap node[MAX_NODES];
  float temp, hum;
  uint16_t dist;
  uint8_t occupied, leak, fan, status, aiOnline, estop;
};

NodeState nodes[MAX_NODES];
float temperature = NAN, humidity = NAN;
int occupiedState = 0, leakState = 0;
int distCm = -1;              // ระยะจาก ultrasonic (-1 = อ่านไม่ได้)
volatile int fanLevel = 0;
int aiFanLevel = -1;          // ระดับพัดลมที่ AI สั่งล่าสุด (-1 = ยังไม่เคยได้รับ)
uint32_t lastAiCmdMs = 0;
uint8_t statusCode = 0;       // 0=ok 1=soon 2=now
const char *STATUS_NAMES[] = {"ok", "soon", "now"};

volatile bool emergencyStop = false;
volatile bool estopChanged = false;
volatile bool leakAlertPending = false;
volatile bool dryAlertPending = false;

QueueHandle_t rawQueue;        // ESP-NOW callback → espNowTask
SemaphoreHandle_t stateMutex;  // กันสอง task เขียนข้อมูลชนกัน

#if USE_TLS
WiFiClientSecure net;
#else
WiFiClient net;
#endif
PubSubClient mqtt(net);
DHT dht(DHT_PIN, DHT_TYPE);

// =====================================================================
// ส่วนที่ 2: จัดการ node หลายตัว (Scalability)
// =====================================================================

// "WC-1" → "wc1"
void makeKey(const char *id, char *out, size_t outSize) {
  size_t j = 0;
  for (size_t i = 0; id[i] && j < outSize - 1; i++) {
    if (id[i] != '-' && id[i] != '_' && id[i] != ' ') out[j++] = tolower(id[i]);
  }
  out[j] = 0;
}

// หา node จากชื่อ ถ้ายังไม่เคยเห็นให้ลงทะเบียนใหม่ (เรียกตอนถือ mutex อยู่)
NodeState *findOrAddNode(const char *id) {
  for (auto &n : nodes)
    if (n.used && strcmp(n.id, id) == 0) return &n;
  for (auto &n : nodes) {
    if (!n.used) {
      strlcpy(n.id, id, sizeof(n.id));
      makeKey(id, n.key, sizeof(n.key));
      n.used = true;
      Serial.printf("[NODE] new node registered: %s (key %s)\n", n.id, n.key);
      return &n;
    }
  }
  return nullptr;  // เต็มแล้ว
}

// =====================================================================
// ส่วนที่ 3: interrupt และ callback
// =====================================================================

// ESP-NOW เรียกเมื่อมีข้อความเข้า — ห้ามทำงานหนักที่นี่ แค่ copy ใส่ queue
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  RawPacket p;
  p.len = min(len, (int)sizeof(p.data));
  memcpy(p.data, data, p.len);
  xQueueSend(rawQueue, &p, 0);  // คิวเต็มก็ทิ้ง ห้ามรอ
}

// ปุ่มหยุดฉุกเฉิน: กดครั้งแรก = หยุด, กดอีกครั้ง = กลับมาทำงาน
volatile uint32_t lastButtonMs = 0;
void IRAM_ATTR onButton() {
  uint32_t now = millis();
  if (now - lastButtonMs > 300) {
    emergencyStop = !emergencyStop;
    estopChanged = true;
    lastButtonMs = now;
  }
}

// รับคำสั่งจาก AI service ผ่าน MQTT  เช่น {"fan":2,"status":"soon"}
void onMqttMessage(char *topic, byte *payload, unsigned int len) {
  if (strcmp(topic, TOPIC_CMD) != 0) return;
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) return;
  int f = doc["fan"] | -1;
  const char *s = doc["status"] | "";
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (f >= 0 && f <= 3) {
    aiFanLevel = f;
    lastAiCmdMs = millis();
  }
  for (uint8_t i = 0; i < 3; i++)
    if (strcmp(s, STATUS_NAMES[i]) == 0) statusCode = i;
  xSemaphoreGive(stateMutex);
  Serial.printf("[AI] fan=%d status=%s\n", f, s);
}

// =====================================================================
// ส่วนที่ 4: พัดลม
// =====================================================================

// วัดระยะด้วย ultrasonic (ซม.) คืน -1 ถ้าไม่มีเสียงสะท้อนกลับภายในเวลา (ไกลเกิน ~5 เมตร หรือสายหลุด)
int readDistanceCm() {
  digitalWrite(US_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(US_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(US_TRIG_PIN, LOW);
  unsigned long us = pulseIn(US_ECHO_PIN, HIGH, 30000);  // รอไม่เกิน 30 ms
  if (us == 0) return -1;
  return (int)(us / 58);  // เสียงเดินทางไป-กลับ 58 µs ต่อ 1 ซม.
}

int fallbackLevel(float maxOdor) {
  if (maxOdor >= ODOR_HIGH) return 3;
  if (maxOdor >= ODOR_MID) return 2;
  return 1;
}

void applyFan(int level) {
  if (level == fanLevel) return;
  if (fanLevel == 0 && level > 0) {
    ledcWrite(FAN_CH, 255);  // กระตุกเต็มแรงครึ่งวินาทีให้พัดลมเริ่มหมุน
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  ledcWrite(FAN_CH, FAN_DUTY[level]);
  fanLevel = level;
  Serial.printf("[FAN] level %d\n", level);
}

// =====================================================================
// ส่วนที่ 5: buffer ตอนเน็ตหลุด (store-and-forward, E4)
// =====================================================================

Snapshot ring[RING_SIZE];
int ringHead = 0;   // ตำแหน่งข้อมูลเก่าสุด
int ringCount = 0;

void bufferPush(const Snapshot &s) {
  int idx = (ringHead + ringCount) % RING_SIZE;
  ring[idx] = s;
  if (ringCount < RING_SIZE) ringCount++;
  else ringHead = (ringHead + 1) % RING_SIZE;  // เต็มแล้ว ทับข้อมูลเก่าสุด
}

// =====================================================================
// ส่วนที่ 6: สร้าง JSON และส่งขึ้น cloud
// =====================================================================

Snapshot takeSnapshot() {
  Snapshot s = {};
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  s.ts = (uint32_t)tv.tv_sec;
  s.ms = (uint16_t)(tv.tv_usec / 1000);
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  uint32_t now = millis();
  for (int i = 0; i < MAX_NODES; i++) {
    NodeState &n = nodes[i];
    if (!n.used) continue;
    s.node[i].odor = n.odor;
    s.node[i].hasOdor = n.hasOdor;
    s.node[i].people = n.peopleAccum;
    s.node[i].seq = n.lastSeq;
    s.node[i].recv = n.received;
    s.node[i].online = (now - n.lastSeenMs < NODE_TIMEOUT_MS) ? 1 : 0;
    n.peopleAccum = 0;  // เริ่มนับรอบใหม่
  }
  s.temp = temperature;
  s.hum = humidity;
  s.occupied = occupiedState;
  s.dist = distCm < 0 ? 0 : distCm;
  s.leak = leakState;
  s.fan = fanLevel;
  s.status = statusCode;
  s.aiOnline = (aiFanLevel >= 0 && now - lastAiCmdMs < AI_TIMEOUT_MS) ? 1 : 0;
  s.estop = emergencyStop ? 1 : 0;
  xSemaphoreGive(stateMutex);
  return s;
}

void snapshotToJson(const Snapshot &s, JsonObject d, bool buffered) {
  d["ts"] = s.ts;
  d["ts_ms"] = s.ms;
  for (int i = 0; i < MAX_NODES; i++) {
    if (!nodes[i].used) continue;
    String k = nodes[i].key;
    if (s.node[i].hasOdor) d[k + "_odor"] = s.node[i].odor;  // node ที่ไม่มี MQ-137 ไม่ส่งค่ากลิ่น
    d[k + "_people"] = s.node[i].people;
    d[k + "_seq"] = s.node[i].seq;
    d[k + "_recv"] = s.node[i].recv;
    d[k + "_online"] = s.node[i].online;
  }
  if (!isnan(s.temp)) d["temp"] = roundf(s.temp * 10) / 10.0f;
  if (!isnan(s.hum)) d["hum"] = roundf(s.hum * 10) / 10.0f;
  d["occupied"] = s.occupied;
  d["dist_cm"] = s.dist;
  d["leak"] = s.leak;
  d["fan_level"] = s.fan;
  d["clean_status"] = STATUS_NAMES[s.status];
  d["ai_online"] = s.aiOnline;
  d["estop"] = s.estop;
  if (buffered) d["buffered"] = 1;
}

bool publishSnapshot(const Snapshot &s, bool buffered) {
  JsonDocument doc;
  snapshotToJson(s, doc["data"].to<JsonObject>(), buffered);
  // บอก NETPIE ว่าข้อมูลนี้เป็นของเวลาไหน (ms) ข้อมูลย้อนหลังจาก buffer จะถูกเก็บลง feed ตามเวลาจริง
  // และไม่ไปทับค่าล่าสุดใน shadow (ถ้ายังไม่ได้เวลาจาก NTP จะไม่ใส่)
  if (s.ts > 1600000000UL) doc["timestamp"] = (uint64_t)s.ts * 1000ULL + s.ms;
  char buf[900];
  serializeJson(doc, buf, sizeof(buf));
  bool ok = mqtt.publish(TOPIC_SHADOW, buf);
  ok = mqtt.publish(TOPIC_RAW, buf) && ok;
  return ok;
}

// ส่งข้อมูลค้างใน buffer ทีละไม่เกิน 20 รายการต่อรอบ จะได้ไม่บล็อกนาน
void flushBuffer() {
  int sent = 0;
  while (ringCount > 0 && mqtt.connected() && sent < 20) {
    if (!publishSnapshot(ring[ringHead], true)) break;
    ringHead = (ringHead + 1) % RING_SIZE;
    ringCount--;
    sent++;
    mqtt.loop();
  }
  if (sent) Serial.printf("[BUF] flushed %d, remaining %d\n", sent, ringCount);
}

uint32_t lastMqttTryMs = 0;
void connectMqtt() {
  if (millis() - lastMqttTryMs < 5000) return;  // ลองใหม่ทุก 5 วินาที
  lastMqttTryMs = millis();
#if USE_TLS
  // TLS ต้องรู้เวลาจริงก่อน (ไว้ตรวจวันหมดอายุของใบรับรอง) รอ NTP ก่อน
  if (time(nullptr) < 1600000000) {
    Serial.println("[MQTT] waiting for NTP time before TLS...");
    return;
  }
#endif
  Serial.print("[MQTT] connecting... ");
  if (mqtt.connect(NETPIE_CLIENT_ID, NETPIE_TOKEN, NETPIE_SECRET)) {
    Serial.println("OK");
    mqtt.subscribe(TOPIC_CMD);
  } else {
    Serial.printf("failed, rc=%d\n", mqtt.state());
  }
}

// =====================================================================
// ส่วนที่ 7: FreeRTOS tasks (E1)
// =====================================================================

// Priority 3 (สูงสุด): รับข้อมูลจาก node — ต้องรีบดึงออกจาก queue ไม่ให้ข้อมูลหาย
void espNowTask(void *) {
  RawPacket p;
  for (;;) {
    if (xQueueReceive(rawQueue, &p, portMAX_DELAY) != pdTRUE) continue;
    JsonDocument doc;
    if (deserializeJson(doc, p.data, p.len)) {
      Serial.println("[ESP-NOW] bad packet");
      continue;
    }
    const char *id = doc["node_id"] | "";
    if (strlen(id) == 0) continue;

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    NodeState *n = findOrAddNode(id);
    if (n) {
      if (doc["odor"].is<float>()) {
        n->odor = doc["odor"];
        n->hasOdor = true;
      }
      n->peopleAccum += doc["people"] | 0;
      n->lastSeq = doc["seq"] | 0;
      n->received++;
      n->lastSeenMs = millis();
    }
    xSemaphoreGive(stateMutex);
    if (n) {
      if (doc["odor"].is<float>())
        Serial.printf("[ESP-NOW] %s seq=%u odor=%.2f people=%u\n", id, (unsigned)(doc["seq"] | 0),
                      (float)doc["odor"], (unsigned)(doc["people"] | 0));
      else
        Serial.printf("[ESP-NOW] %s seq=%u people=%u (no MQ-137)\n", id, (unsigned)(doc["seq"] | 0),
                      (unsigned)(doc["people"] | 0));
    }
  }
}

// Priority 2: อ่านเซ็นเซอร์ของ gateway (ultrasonic, water level, KY-015) + คุมพัดลม — ต้องตอบสนองทันทีโดยไม่รอเน็ต
void fanTask(void *) {
  uint32_t lastDhtMs = 0, highSinceMs = 0;
  int prevLeak = 0;
  for (;;) {
    int dist = readDistanceCm();
    int occupied = (dist > 0 && dist < OCCUPIED_CM) ? 1 : 0;
    int leak = analogRead(LEAK_PIN) > LEAK_THRESHOLD ? 1 : 0;
    float t = NAN, h = NAN;
    if (millis() - lastDhtMs >= 2000) {  // DHT11 อ่านได้ไม่ถี่กว่า 1 วินาที อ่านทุก 2 วินาทีพอ
      lastDhtMs = millis();
      t = dht.readTemperature();
      h = dht.readHumidity();
    }

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    occupiedState = occupied;
    distCm = dist;
    leakState = leak;
    if (!isnan(t) && !isnan(h)) {
      temperature = t;
      humidity = h;
    }
    float maxOdor = 0;
    uint32_t now = millis();
    for (auto &n : nodes)
      if (n.used && n.hasOdor && now - n.lastSeenMs < NODE_TIMEOUT_MS && n.odor > maxOdor) maxOdor = n.odor;
    bool aiFresh = aiFanLevel >= 0 && now - lastAiCmdMs < AI_TIMEOUT_MS;
    int target = aiFresh ? aiFanLevel : fallbackLevel(maxOdor);  // AI ไม่ตอบ → ตัดสินใจเอง
    xSemaphoreGive(stateMutex);

    // ความปลอดภัย: ระดับสูงสุดต่อเนื่องนานเกินไป → ลดลงเหลือระดับ 2
    if (target == 3) {
      if (highSinceMs == 0) highSinceMs = millis();
      if (millis() - highSinceMs > FAN_HIGH_MAX_MS) target = 2;
    } else {
      highSinceMs = 0;
    }
    if (emergencyStop) target = 0;
    applyFan(target);

    if (leak && !prevLeak) leakAlertPending = true;   // เพิ่งเปียก
    if (!leak && prevLeak) dryAlertPending = true;    // เพิ่งแห้ง
    prevLeak = leak;

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// Priority 1 (ต่ำสุด): เน็ต, MQTT, แจ้งเตือน, OTA — งานที่รอได้ ไม่ควรไปบล็อกงานอื่น
void cloudTask(void *) {
  uint32_t lastPublishMs = 0;
  bool otaStarted = false;
  for (;;) {
    bool wifiOk = WiFi.status() == WL_CONNECTED;
    if (wifiOk) {
      if (!otaStarted) {
        ArduinoOTA.begin();
        otaStarted = true;
        Serial.printf("[OTA] ready at %s\n", WiFi.localIP().toString().c_str());
      }
      ArduinoOTA.handle();
      if (!mqtt.connected()) connectMqtt();
      mqtt.loop();
    }

    if (millis() - lastPublishMs >= PUBLISH_INTERVAL_MS) {
      lastPublishMs = millis();
      Snapshot s = takeSnapshot();
      if (mqtt.connected()) {
        flushBuffer();
        if (!publishSnapshot(s, false)) bufferPush(s);
      } else {
        bufferPush(s);
        Serial.printf("[BUF] offline, buffered %d\n", ringCount);
      }
    }

    if (wifiOk) {
      if (leakAlertPending) { leakAlertPending = false; sendDiscord(MSG_LEAK); }
      if (dryAlertPending)  { dryAlertPending = false;  sendDiscord(MSG_LEAK_DRY); }
      if (estopChanged)     { estopChanged = false;     sendDiscord(emergencyStop ? MSG_ESTOP_ON : MSG_ESTOP_OFF); }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

#ifdef FAKE_NODES
// โหมดทดสอบ: สร้างข้อมูล node ปลอมเข้า queue เหมือนได้รับจาก ESP-NOW จริง
void fakeNodeTask(void *) {
  uint32_t seq = 0;
  for (;;) {
    RawPacket p;
    // WC-1 (ทางเข้า) มีแค่ KY-032 → ไม่มี odor
    p.len = snprintf((char *)p.data, sizeof(p.data), "{\"node_id\":\"WC-1\",\"seq\":%u,\"people\":%u}",
                     (unsigned)seq, (unsigned)(esp_random() % 4));
    xQueueSend(rawQueue, &p, 0);
    // WC-2 (โซนโถ) มี MQ-137 + KY-032
    p.len = snprintf((char *)p.data, sizeof(p.data),
                     "{\"node_id\":\"WC-2\",\"seq\":%u,\"odor\":%.2f,\"people\":%u}",
                     (unsigned)seq, 1.0f + (esp_random() % 150) / 100.0f, (unsigned)(esp_random() % 4));
    xQueueSend(rawQueue, &p, 0);
    seq++;
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
#endif

// =====================================================================
// ส่วนที่ 8: setup
// =====================================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== OnBedSystem Gateway ===");

  pinMode(US_TRIG_PIN, OUTPUT);
  pinMode(US_ECHO_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButton, FALLING);
  analogSetPinAttenuation(LEAK_PIN, ADC_11db);
  ledcSetup(FAN_CH, FAN_PWM_FREQ, 8);
  ledcAttachPin(FAN_PIN, FAN_CH);
  ledcWrite(FAN_CH, 0);
  dht.begin();

  stateMutex = xSemaphoreCreateMutex();
  rawQueue = xQueueCreate(20, sizeof(RawPacket));

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // ปิดโหมดประหยัดไฟ ไม่งั้นพลาดข้อความ ESP-NOW
  WiFi.setAutoReconnect(true);   // เน็ตหลุดแล้วต่อใหม่เอง
  Serial.print("Gateway MAC: ");
  Serial.println(WiFi.macAddress());
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("\nWiFi OK  channel=%d  IP=%s\n", WiFi.channel(), WiFi.localIP().toString().c_str());
  else
    Serial.println("\nWiFi not connected yet (will keep trying)");

  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");  // เวลาไทย

  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed, restarting");
    delay(1000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onDataRecv);

  // MQTT
#if USE_TLS
  net.setCACert(NETPIE_CA_CERT);
#endif
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(1024);
  mqtt.setKeepAlive(30);
  mqtt.setCallback(onMqttMessage);

  // OTA
  ArduinoOTA.setHostname("onbed-gateway");
  ArduinoOTA.setPassword(OTA_PASSWORD);

  // Tasks: ชื่อ, stack, priority
  xTaskCreatePinnedToCore(espNowTask, "espnow", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(fanTask, "fan", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(cloudTask, "cloud", 10240, NULL, 1, NULL, 1);
#ifdef FAKE_NODES
  xTaskCreatePinnedToCore(fakeNodeTask, "fake", 3072, NULL, 1, NULL, 1);
  Serial.println("*** FAKE_NODES test mode ***");
#endif
}

void loop() {
  vTaskDelete(NULL);  // ไม่ใช้ loop() งานทั้งหมดอยู่ใน task
}
