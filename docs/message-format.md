# รูปแบบข้อความ (Message Format)

## 1. Sensor node → Gateway (ESP-NOW, ทุก 10 วินาที)
```json
{"node_id": "WC-2", "seq": 1024, "odor": 1.42, "people": 5}
{"node_id": "WC-1", "seq": 1030, "people": 3}
```
| field | ชนิด | ความหมาย |
| --- | --- | --- |
| node_id | string | "WC-1" = โซนทางเข้า, "WC-2" = โซนโถ |
| seq | int | เลขลำดับข้อความ เพิ่มทีละ 1 |
| odor | float | ค่ากลิ่น R0/Rs (อากาศสะอาด ≈ 1.0, ยิ่งเหม็นยิ่งสูง, -1 = เซ็นเซอร์หลุด) มีเฉพาะ WC-2 เพราะ WC-1 ไม่มี MQ-137 |
| people | int | จำนวนครั้งที่มีคนเดินผ่านใน 10 วินาทีที่ผ่านมา |

## 2. Gateway → NETPIE (MQTT, ทุก 10 วินาที)
```json
{"data": {
  "ts": 1763280000, "ts_ms": 123,
  "wc1_people": 5, "wc1_seq": 1024, "wc1_recv": 1010, "wc1_online": 1,
  "wc2_odor": 1.10, "wc2_people": 2, "wc2_seq": 998,  "wc2_recv": 990,  "wc2_online": 1,
  "temp": 29.5, "hum": 78.0, "occupied": 1, "dist_cm": 45, "leak": 0,
  "fan_level": 1, "clean_status": "ok", "ai_online": 1, "estop": 0
 },
 "timestamp": 1763280000123}
```

ชื่อ field ของ node สร้างอัตโนมัติจาก `node_id` (`WC-1` → `wc1`) เพิ่ม node ตัวที่ 3 ก็จะได้ `wc3_...` เองโดยไม่ต้องแก้โค้ด gateway

| field | ความหมาย |
| --- | --- |
| ts, ts_ms | เวลาที่ gateway สร้างข้อมูล (วินาที + มิลลิวินาที) |
| wcN_odor, wcN_people | ค่ากลิ่นและจำนวนคนของ node N |
| wcN_seq, wcN_recv | seq ล่าสุดของ node N และจำนวนข้อความที่ gateway ได้รับจาก node N ทั้งหมด |
| wcN_online | 1 = node N ส่งข้อมูลมาใน 60 วินาทีล่าสุด |
| temp, hum | อุณหภูมิ (°C) ความชื้น (%) จาก KY-015 |
| occupied | 1 = ultrasonic เจอคนในระยะ (มีคนอยู่ในห้อง) |
| dist_cm | ระยะที่ ultrasonic วัดได้ (ซม.) |
| leak | 1 = พื้นเปียก |
| fan_level | ระดับพัดลม 0–3 |
| clean_status | ok / soon / now |
| ai_online | 1 = ได้รับคำสั่งจาก AI ใน 2 นาทีล่าสุด |
| estop | 1 = กดปุ่มหยุดฉุกเฉินอยู่ |
| buffered | 1 = ข้อมูลย้อนหลังที่ค้างตอนเน็ตหลุด |
| timestamp | เวลาของข้อมูล (มิลลิวินาที) ให้ NETPIE เก็บลง feed ตามเวลาจริง |

## 3. AI service → Gateway (MQTT)
```json
{"fan": 2, "status": "soon", "pred": 2.41}
```

## 4. MQTT topic
| Topic | จาก → ถึง |
| --- | --- |
| @shadow/data/update | gateway → NETPIE (dashboard + feed) |
| @msg/onbed/raw | gateway → AI service |
| @msg/onbed/cmd | AI service → gateway |

## 5. เหตุผลการออกแบบ
- ใช้ JSON เพราะอ่านง่าย debug ง่าย ข้อความจาก node ยาวไม่ถึง 100 byte ไม่เกินขีดจำกัด 250 byte ของ ESP-NOW
- มี node_id ทุกข้อความ เพื่อให้ gateway และ dashboard แยกข้อมูลแต่ละ node ได้ และเพิ่ม node ใหม่ได้โดยไม่ต้องแก้โค้ด gateway
- มี seq เพื่อตรวจว่าข้อความหายไหม และใช้คำนวณ packet delivery rate
- ส่ง timestamp ขึ้น NETPIE เพื่อให้ข้อมูลย้อนหลังที่ค้างใน buffer ตอนเน็ตหลุดถูกเก็บตามเวลาจริง
