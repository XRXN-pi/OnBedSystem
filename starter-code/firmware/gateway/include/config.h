#pragma once
#include <Arduino.h>

// ---------- ขา GPIO ----------
#define DHT_PIN     4    // KY-015 ขา S (สัญญาณ)
#define DHT_TYPE    DHT11 // KY-015 ใช้ชิป DHT11
#define MQ_PIN      34   // MQ-137 ขา AO (ผ่าน voltage divider 10k/20k, ต้องเป็น ADC1)
#define FAN_PIN    25   // ไป MOSFET module ขา SIG/IN
#define BUTTON_PIN 14   // ปุ่มหยุดฉุกเฉิน (อีกขาต่อ GND)
// ultrasonic อยู่ที่ node WC-1 และ water level อยู่ที่ node WC-2 ค่าทั้งสองส่งมาทาง ESP-NOW

// ---------- พัดลม ----------
#define FAN_CH        0       // ช่อง PWM ของ ESP32
#define FAN_PWM_FREQ  25000   // 25 kHz ไม่มีเสียงหวีด
static const uint8_t FAN_DUTY[4] = {0, 110, 180, 255};  // ระดับ 0=ปิด 1=low 2=mid 3=high
#define FAN_HIGH_MAX_MS (15UL * 60 * 1000)  // ระดับ 3 ต่อเนื่องได้ไม่เกิน 15 นาที (ความปลอดภัย M7)

// ---------- MQ-137 ----------
#define DIVIDER_RATIO 1.5f   // R บน 10k, R ล่าง 20k → (10+20)/20 = 1.5
#define MQ_VCC        5.0f   // ไฟเลี้ยงโมดูล MQ-137
// ค่า R0 จาก env:gateway_calibrate ใส่ใน platformio.ini (MQ_R0_VALUE) ถ้าไม่ใส่จะใช้ 1.0
#ifdef MQ_R0_VALUE
#define MQ_R0 MQ_R0_VALUE
#else
#define MQ_R0 1.0f
#endif

// ---------- เกณฑ์ ----------
#define ODOR_MID  1.5f        // fallback: ค่ากลิ่นเกินนี้ → พัดลมระดับ 2
#define ODOR_HIGH 2.5f        // fallback: ค่ากลิ่นเกินนี้ → พัดลมระดับ 3

// ---------- เวลา ----------
#define PUBLISH_INTERVAL_MS 10000   // ส่งขึ้น cloud ทุก 10 วินาที
#define NODE_TIMEOUT_MS     60000   // node เงียบเกิน 60 วินาที = offline
#define AI_TIMEOUT_MS       120000  // ไม่ได้คำสั่ง AI เกิน 2 นาที → ใช้ fallback

// ---------- อื่นๆ ----------
#define MAX_NODES 4     // รองรับ sensor node ได้สูงสุด 4 ตัว
#define RING_SIZE 360   // buffer เก็บข้อมูลตอนเน็ตหลุดได้ 360 รอบ = 1 ชั่วโมง

// ---------- MQTT ----------
#define USE_TLS 0   // 0 = port 1883 (ช่วงแรก), 1 = TLS port 1884 ของ NETPIE (ข้อ E5)
#define MQTT_HOST "mqtt.netpie.io"
#if USE_TLS
#define MQTT_PORT 1884   // NETPIE ใช้ 1884 สำหรับ MQTTS (TLS)
#else
#define MQTT_PORT 1883
#endif
#define TOPIC_SHADOW "@shadow/data/update"
#define TOPIC_RAW    "@msg/onbed/raw"
#define TOPIC_CMD    "@msg/onbed/cmd"
