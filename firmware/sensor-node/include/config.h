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

// ---------- ขา GPIO ----------
#define MQ_PIN   34   // MQ-137 ขา AO (ผ่าน voltage divider)
#define BEAM_PIN 27   // KY-032 (obstacle avoidance) ขา OUT: LOW เมื่อมีคนผ่านหน้าเซ็นเซอร์

// ---------- MQ-137 ----------
// node ไหนมี MQ-137 กำหนดใน platformio.ini (HAS_MQ=1 มี, HAS_MQ=0 ไม่มี) — โปรเจคนี้ WC-2 (โซนโถ) มี, WC-1 (ทางเข้า) ไม่มี
#ifndef HAS_MQ
#define HAS_MQ 1
#endif
#define DIVIDER_RATIO 1.5f   // R บน 10k, R ล่าง 20k → (10+20)/20 = 1.5
#define MQ_VCC        5.0f   // ไฟเลี้ยงโมดูล MQ-137
// ค่า R0 จากโหมด calibrate ใส่แยกของแต่ละตัวใน platformio.ini (MQ_R0_VALUE) ถ้าไม่ใส่จะใช้ 1.0
#ifdef MQ_R0_VALUE
#define MQ_R0 MQ_R0_VALUE
#else
#define MQ_R0 1.0f
#endif

// ---------- เวลา ----------
#define REPORT_INTERVAL_MS 10000   // ส่งข้อมูลทุก 10 วินาที
#define BEAM_DEBOUNCE_MS   500     // คนหนึ่งคนอาจทำให้สัญญาณกระพริบหลายครั้ง นับใหม่ได้หลัง 500 ms
#define MAX_ATTEMPTS       4       // ส่ง 1 ครั้ง + ส่งซ้ำได้อีก 3 ครั้ง
#define FAILS_BEFORE_RESCAN 3      // ส่งไม่ถึงติดกัน 3 รอบ → หา channel ใหม่
