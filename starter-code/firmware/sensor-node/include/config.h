#pragma once
#include <Arduino.h>

// ---------- ชื่อ node ----------
// ไม่ต้องแก้ตรงนี้ ชื่อถูกกำหนดจาก platformio.ini (env:wc1 / env:wc2)
#ifndef NODE_ID
#define NODE_ID "WC-1"
#endif

// ---------- Gateway ----------
// MAC address ของ gateway (gateway พิมพ์ออกมาตอนเปิดเครื่อง: "Gateway MAC: ...")
// ตัวอย่าง: ถ้า gateway พิมพ์ว่า 24:6F:28:AA:BB:CC ให้ใส่ {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC}
static const uint8_t GATEWAY_MAC[6] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC};

// ชื่อ Wi-Fi (hotspot) ที่ gateway ต่ออยู่ ใช้หา channel เท่านั้น node ไม่ได้ต่อ Wi-Fi
#define WIFI_SSID "OnBed-Hotspot"

// ---------- เซ็นเซอร์ของแต่ละ node ----------
// กำหนดใน platformio.ini:
//   env:wc1 (ประตู) = KY-032 นับคน + ultrasonic ตรวจว่ามีคนอยู่ในห้อง
//   env:wc2 (พื้น)  = water level ตรวจพื้นเปียก
#ifndef HAS_BEAM
#define HAS_BEAM 1
#endif
#ifndef HAS_ULTRASONIC
#define HAS_ULTRASONIC 1
#endif
#ifndef HAS_LEAK
#define HAS_LEAK 0
#endif

// ---------- ขา GPIO ----------
#define BEAM_PIN     27   // KY-032 (obstacle avoidance) ขา OUT: LOW เมื่อมีคนผ่านหน้าเซ็นเซอร์
#define US_TRIG_PIN  26   // Ultrasonic HC-SR04 ขา Trig
#define US_ECHO_PIN  33   // Ultrasonic HC-SR04 ขา Echo (ผ่าน voltage divider 10k/20k)
#define LEAK_PIN     35   // Water level module ขา S (ADC1 เท่านั้น เพราะเปิด Wi-Fi อยู่)

// ---------- เกณฑ์ ----------
#define OCCUPIED_CM     60    // ultrasonic วัดได้ใกล้กว่านี้ = มีคนยืนอยู่ (ตั้งใหม่ตามห้องจริง)
#define LEAK_THRESHOLD  1000  // ค่า ADC มากกว่านี้ = พื้นเปียก (หาค่ากลางระหว่างแห้งกับเปียก)

// ---------- เวลา ----------
#define REPORT_INTERVAL_MS 10000   // ส่งข้อมูลทุก 10 วินาที
#define SENSOR_POLL_MS     500     // อ่าน ultrasonic / water level ทุก 0.5 วินาที
#define BEAM_DEBOUNCE_MS   500     // คนหนึ่งคนอาจทำให้สัญญาณกระพริบหลายครั้ง นับใหม่ได้หลัง 500 ms
#define MAX_ATTEMPTS       4       // ส่ง 1 ครั้ง + ส่งซ้ำได้อีก 3 ครั้ง
#define FAILS_BEFORE_RESCAN 3      // ส่งไม่ถึงติดกัน 3 รอบ → หา channel ใหม่
