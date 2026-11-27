#pragma once
#include <Arduino.h>

#ifdef NODE_IDN
#define __NODE_ID(ID) "WC-" #ID
#define _NODE_ID(ID) __NODE_ID(ID)
#define NODE_ID _NODE_ID(NODE_IDN)
#else
#define NODE_ID "TEST"
#define IR_BEAM
#define ULTRASONIC
#define WATER_LEVEL
#define OPT_TELEM
#endif

// ---------- Gateway ----------
// MAC address ของ gateway (gateway พิมพ์ออกมาตอนเปิดเครื่อง: "Gateway MAC: ...")
static const uint8_t GATEWAY_MAC[6] = { 0x48, 0x9D, 0x31, 0xE3, 0xC9, 0x78 };

// ชื่อ Wi-Fi (hotspot) ที่ gateway ต่ออยู่ ใช้หา channel เท่านั้น node ไม่ได้ต่อ Wi-Fi
#define WIFI_SSID "OnBed-Hotspot"

// ---------- WC-1 ----------
#if NODE_IDN == 1
#define IR_BEAM
#define ULTRASONIC
#endif

// ---------- WC-2 ----------
#if NODE_IDN == 2
#define WATER_LEVEL
#endif

// ---------- Sensors ----------
#ifdef IR_BEAM
#define BEAM_PIN          27  // KY-032 (obstacle avoidance) ขา OUT: LOW เมื่อมีคนผ่านหน้าเซ็นเซอร์
#define BEAM_DEBOUNCE_MS  500 // คนหนึ่งคนอาจทำให้สัญญาณกระพริบหลายครั้ง นับใหม่ได้หลัง 500 ms
#endif

#ifdef ULTRASONIC
#define US_TRIG_PIN       32  // Ultrasonic Trigger
#define US_ECHO_PIN       33  // Ultrasonic Echo
#define US_INTERVAL_MS    1000
#endif

#ifdef WATER_LEVEL
#define WATER_LEVEL_PIN       4   // Water level sensor
#define WATER_LEVEL_INTERVAL  500
#endif

// ---------- Telemetry ----------
#define REPORT_INTERVAL_MS 10000   // ส่งข้อมูลทุก 10 วินาที
#define MAX_ATTEMPTS       4       // ส่ง 1 ครั้ง + ส่งซ้ำได้อีก 3 ครั้ง
#define FAILS_BEFORE_RESCAN 3      // ส่งไม่ถึงติดกัน 3 รอบ → หา channel ใหม่
