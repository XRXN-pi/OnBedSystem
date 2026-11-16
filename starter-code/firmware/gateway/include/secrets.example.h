#pragma once
// คัดลอกไฟล์นี้เป็น secrets.h แล้วใส่ค่าจริง  (secrets.h ถูก .gitignore ไว้ ห้าม commit)

#define WIFI_SSID     "OnBed-Hotspot"   // ต้องตรงกับ WIFI_SSID ใน sensor-node/include/config.h
#define WIFI_PASSWORD "your-password"

// จาก NETPIE portal → device "gateway"
#define NETPIE_CLIENT_ID "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
#define NETPIE_TOKEN     "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define NETPIE_SECRET    "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

#define OTA_PASSWORD "onbed-ota-change-me"

// Discord webhook URL จากคนทำ dashboard (เว้นว่าง "" ได้ถ้ายังไม่มี ระบบจะข้ามการแจ้งเตือน)
// หน้าตา: https://discord.com/api/webhooks/<ตัวเลข>/<รหัสยาว>  ใครได้ URL นี้ไปก็ส่งข้อความเข้า channel ได้ ห้าม commit
#define DISCORD_WEBHOOK_URL ""

// ใช้เมื่อ USE_TLS = 1 (ดูวิธีหาในข้อ 6.3)
#define NETPIE_CA_CERT \
  "-----BEGIN CERTIFICATE-----\n" \
  "...ใส่ใบรับรองตรงนี้ทีละบรรทัด...\n" \
  "-----END CERTIFICATE-----\n"
