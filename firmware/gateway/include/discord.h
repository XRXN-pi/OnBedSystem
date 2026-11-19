#pragma once
#include <Arduino.h>

// ข้อความแจ้งเตือนที่ gateway ส่งเอง (แก้ถ้อยคำได้ที่นี่)
#define ROOM_NAME     "ห้องน้ำทดสอบ"
#define MSG_LEAK      "⚠️ " ROOM_NAME ": ตรวจพบพื้นเปียก กรุณาเช็ดพื้นเพื่อป้องกันการลื่นล้ม"
#define MSG_LEAK_DRY  "✅ " ROOM_NAME ": พื้นแห้งแล้ว"
#define MSG_ESTOP_ON  "🛑 " ROOM_NAME ": มีการกดปุ่มหยุดพัดลมฉุกเฉิน"
#define MSG_ESTOP_OFF "▶️ " ROOM_NAME ": ยกเลิกหยุดฉุกเฉิน พัดลมกลับมาทำงาน"

// ส่งข้อความเข้า Discord channel ผ่าน webhook คืนค่า true ถ้าส่งสำเร็จ
bool sendDiscord(const String &text);
