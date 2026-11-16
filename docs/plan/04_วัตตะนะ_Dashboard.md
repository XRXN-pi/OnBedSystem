# 04 — วัตตะนะ: UI/UX & Dashboard Developer

อ่าน `00_ภาพรวม_ทุกคนอ่านก่อน.md` ข้อ 2 และ 3 ก่อน

## สิ่งที่ต้องส่งมอบ

- **Dashboard บน NETPIE** ที่แม่บ้านและเจ้าหน้าที่อาคารดูแล้วเข้าใจทันทีโดยไม่ต้องอธิบาย
- **การเก็บข้อมูลดิบใน NETPIE feed** (ข้อ M5)
- **ระบบแจ้งเตือน Discord** ทั้งฝั่ง gateway (พื้นเปียก) และฝั่ง Python (ถึงเวลาทำความสะอาด, อุปกรณ์หลุด) พร้อมกันแจ้งเตือนรัว (ข้อ M6)
- ได้คะแนน: **M5, M6**, **User Experience 3/3**

ไฟล์ของคุณใน starter code:
- `firmware/gateway/include/discord.h`, `firmware/gateway/src/discord.cpp` (ส่ง Discord จาก gateway)
- `ai/notifier.py` (ส่ง Discord จาก Python + ถ้อยคำแจ้งเตือน)
- `tools/fake_gateway.py` (ใช้ทดสอบ dashboard ไม่ต้องมีบอร์ด)
- `dashboard/README.md` (คุณสร้าง: วิธีตั้งค่า + schema + รูป)

---

## แผนรายวัน

| วัน | งาน | เวลา | เสร็จเมื่อ |
| --- | --- | --- | --- |
| จ. 16 | ลง Python (00 ข้อ 3.2), ขั้นที่ 1 (Discord webhook) | 1.5 ชม. | `python notifier.py` ส่งข้อความเข้า channel ได้ |
| อ. 17 | ขั้นที่ 2–3 (Schema/Feed + ข้อมูลจำลอง) | 1.5 ชม. | Shadow ของ gateway อัปเดตจาก `fake_gateway.py` |
| พ. 18 | ขั้นที่ 4 (สร้าง dashboard) | 2 ชม. | widget ครบทุกค่า |
| พฤ. 19 | ขั้นที่ 5 + commit | 1 ชม. | `dashboard/README.md` + รูป, commit ไฟล์แจ้งเตือน Discord |
| **ศ. 20** | ขั้นที่ 6 (รวมระบบ) | 1 ชม. | dashboard แสดงค่าจริงจาก node |
| ส.–อา. 21–22 | ขั้นที่ 7 (แจ้งเตือนจริง) | 2 ชม. | พื้นเปียกแล้ว Discord เด้ง |
| 23–27 | ขั้นที่ 8 (ปรับ UX + ทดสอบกับคนจริง) | 3–4 ชม. รวม | ผลทดสอบอยู่ใน `dashboard/ux-test.md`, screenshot ใหม่ commit แล้ว |
| ส.–อา. 28–29 | ขั้นที่ 10 (หาข้อมูลระบบที่คล้ายกัน) | 2 ชม. | `docs/related-work.md` มี 3–4 ระบบพร้อมลิงก์ |
| จ. 30 พ.ย. | ขั้นที่ 7 ส่วนที่ 2 (ทดสอบแจ้งเตือนฝั่ง Python หลัง `service.py` เริ่มรัน) | 1 ชม. | ได้ข้อความ 4 แบบใน Discord |
| 1–3 ธ.ค. | ขั้นที่ 9 (วัด latency) + ทดสอบ T3, T5, T6 | 2.5 ชม. | `docs/metrics/latency_e2e.csv` 30 แถว, ผล T3/T5/T6 ใน `docs/test-plan.md` |
| 4–5 ธ.ค. | ข้อ 11 (เขียนรายงานข้อ 2, 3.4, ภาคผนวก ค) | 2–3 ชม. | เขียนลง Google Doc รายงานเสร็จภายใน ส. 5 ธ.ค. 23:59 |
| จ. 7 | ข้อ 12 (ซ้อมตอบคำถาม) | 1 ชม. | ตอบคำถามในข้อ 12 ได้โดยไม่ดูโน้ต |

---

## ขั้นที่ 1: สร้าง Discord webhook (จ. 16)

Webhook คือ "ลิงก์ลับ" ของ channel ใน Discord โปรแกรมไหนส่งข้อความไปที่ลิงก์นี้ ข้อความจะขึ้นใน channel ทันที ไม่ต้องสร้างบอท

1. ติดตั้งแอป Discord ในมือถือและคอม แล้วสมัคร/ล็อกอิน
2. สร้าง server: กดปุ่ม **+** (Add a Server) แถบซ้าย → **Create My Own** → **For me and my friends** → ตั้งชื่อ `OnBedSystem` → Create
3. สร้าง text channel ชื่อ `แจ้งเตือนห้องน้ำ` (กด **+** ข้างหัวข้อ Text Channels)
4. เอาเมาส์ไปที่ชื่อ channel แล้วกดรูปเฟือง (**Edit Channel**) → **Integrations** → **Webhooks** → **New Webhook**
5. กดที่ webhook ที่เพิ่งสร้าง ตั้งชื่อ `OnBed Alert` (ชื่อนี้จะขึ้นเป็นผู้ส่งข้อความ) → กด **Copy Webhook URL** → **Save Changes**
6. URL ที่ได้หน้าตาแบบ `https://discord.com/api/webhooks/1234567890/AbCdEf...` นี่คือ **`DISCORD_WEBHOOK_URL`**
   - **URL นี้เป็นความลับ** ใครได้ไปก็ส่งข้อความเข้า channel ได้ ส่งให้เพื่อนทางแชทส่วนตัวเท่านั้น ห้ามใส่ในไฟล์ที่ commit
   - ถ้าเผลอหลุด: กลับไปหน้า Webhooks → กด **Delete Webhook** แล้วสร้างใหม่
7. ชวนสมาชิกทุกคน (และแม่บ้านถ้าจะทดลองจริง) เข้า server: คลิกขวาชื่อ server → **Invite People** → ส่งลิงก์
8. ให้แจ้งเตือนเด้งบนมือถือ: คลิกขวาชื่อ server → **Server Settings** → **Overview** → **Default Notification Settings** เลือก **All Messages** (ถ้าเป็น "Only @mentions" มือถือจะไม่เด้ง)
8. ทดสอบจาก Python:
   - copy `starter-code/ai/` ไปไว้ใน `onbedsystem/ai/` (ถ้าณัฐณิชาใส่แล้วก็ `git pull` พอ)
   - copy `ai/config_secrets.example.py` เป็น `ai/config_secrets.py` แล้วใส่ `DISCORD_WEBHOOK_URL`
   - ใน `ai/` รัน:
     ```bash
     python notifier.py
     ```
   - ต้องขึ้น `ส่งทดสอบ: True` และ channel `แจ้งเตือนห้องน้ำ` ได้ข้อความ "ทดสอบแจ้งเตือนจาก ห้องน้ำทดสอบ"

## ขั้นที่ 2: ตั้ง Schema ให้ NETPIE เก็บข้อมูลลง Feed (อ. 17)

NETPIE จะเก็บข้อมูลย้อนหลัง (feed) เฉพาะ field ที่ระบุ `operation.store` ไว้ใน Data Schema ([docs.netpie.io](https://docs.netpie.io/en/device-config.html))

1. เข้า portal.netpie.io ด้วยบัญชีกลุ่ม → project `onbedsystem` → device **`gateway`**
2. ไปแท็บ **Schema** แล้ววาง JSON นี้ แล้วกด Save:
```json
{
  "additionalProperties": true,
  "properties": {
    "wc1_people": {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "wc1_seq":    {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "wc1_recv":   {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "wc1_online": {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "wc2_odor":   {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "wc2_people": {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "wc2_seq":    {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "wc2_recv":   {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "wc2_online": {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "temp":       {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "hum":        {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "occupied":   {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "dist_cm":    {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "leak":       {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "fan_level":  {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "ai_online":  {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "estop":      {"type": "number", "operation": {"store": {"ttl": "60d"}}},
    "clean_status": {"type": "string"}
  }
}
```
3. `ttl: 60d` = เก็บ 60 วัน (พอถึงวันส่งงาน)
4. ถ้าหน้า Schema ฟ้อง error ให้เทียบรูปแบบกับตัวอย่างใน docs.netpie.io หัวข้อ Data Schema
5. ถ้ามีปุ่มเปิด/ปิด **Feed** ในหน้า device ให้เปิดด้วย
6. บันทึก JSON นี้เป็นไฟล์ `dashboard/netpie-schema.json` (จะใส่ในรายงานภาคผนวก ค) แล้ว commit ตอนขั้นที่ 5

## ขั้นที่ 3: ส่งข้อมูลจำลองเข้า NETPIE (อ. 17)

ไม่ต้องรอบอร์ดจริง ใช้ `fake_gateway.py` ส่งข้อมูลที่หน้าตาเหมือน gateway จริง

1. copy `starter-code/tools/` ไปไว้ที่ `onbedsystem/tools/`
2. copy `tools/gateway_secrets.example.py` เป็น `tools/gateway_secrets.py` แล้วใส่ Client ID / Token / Secret ของ device **`gateway`**
3. **ตรวจว่า gateway จริงปิดอยู่** (Client ID เดียวกันต่อพร้อมกันไม่ได้ ถ้าสุรสิทธิ์กำลังทดสอบอยู่ให้รอ)
4. ใน `tools/` (activate venv ของ `ai/` ก่อน):
   ```bash
   python fake_gateway.py --live
   ```
   หน้าจอจะพิมพ์ JSON ทุก 10 วินาที ถ้าอยากให้เร็วขึ้นตอนจัดหน้า ใช้ `--interval 2`
5. ใน portal → device `gateway` → **Shadow** ต้องเห็นค่าเปลี่ยนตลอด
6. หยุดด้วย `Ctrl+C`

## ขั้นที่ 4: สร้าง Dashboard (พ. 18)

### 4.1 สร้างและผูกข้อมูล
1. portal.netpie.io → เมนู **Dashboard** (หรือ Freeboard) → สร้างใหม่ ชื่อ `OnBed — ห้องน้ำทดสอบ`
2. กด **Datasources → ADD** → เลือกชนิด **NETPIE2020** (หรือชนิดที่ผูกกับ device ใน project) → ตั้งชื่อ `gw` → เลือก/ใส่ข้อมูล device **`gateway`** → Save
3. เวลาเพิ่ม widget ตรงช่อง value ให้กด **`+ DATASOURCE`** แล้วเลือก `gw` → `shadow` → ชื่อ field ระบบจะใส่ path ให้เอง (หน้าตาประมาณ `datasources["gw"]["shadow"]["wc2_odor"]`) ใช้วิธีนี้ดีกว่าพิมพ์เอง จะได้ไม่ผิด syntax
4. **ตรวจว่า dashboard ไม่ทำให้ gateway หลุด:** เปิด dashboard ค้างไว้ระหว่าง `fake_gateway.py --live` ทำงาน ถ้า fake_gateway ยังพิมพ์และ shadow ยังอัปเดต แปลว่าใช้ร่วมกันได้

### 4.2 Layout (จัด 3 แถว)

**แถว 1 — สถานะ (สิ่งที่แม่บ้านต้องเห็นก่อน ตัวใหญ่)**
| Widget | ชนิด | ค่า |
| --- | --- | --- |
| สถานะห้องน้ำ | Text (ตัวใหญ่) | ใช้ JS แปลงเป็นข้อความ (ข้อ 4.3) |
| พื้น | Indicator Light | `leak` (1 = เปียก แสดงไฟแดง) |
| มีคนอยู่ | Indicator Light | `occupied` (1 = มีคน) |
| พัดลม | Text | `fan_level` แปลงเป็น ปิด/ต่ำ/กลาง/สูง |

**แถว 2 — แต่ละโซน**
| Widget | ชนิด | ค่า |
| --- | --- | --- |
| กลิ่นโซนโถ (WC-2) | Gauge (min 0.5, max 4) | `wc2_odor` |
| คนเข้า 10 วินาทีล่าสุด | Text | `wc1_people` |
| คนเข้าโซนโถ 10 วินาทีล่าสุด | Text | `wc2_people` |
| สถานะเซ็นเซอร์ | Text | `wc1_online`, `wc2_online`, `ai_online` (ออนไลน์/หลุด) |

**แถว 3 — แนวโน้ม**
| Widget | ชนิด | ค่า |
| --- | --- | --- |
| กราฟกลิ่นย้อนหลัง | Feed View / Chart (ดึงจาก feed) | `wc2_odor` ย้อนหลัง 24 ชม. |
| อุณหภูมิ | Text / Sparkline | `temp` (°C) |
| ความชื้น | Text / Sparkline | `hum` (%) |

### 4.3 แปลงค่าเป็นข้อความภาษาไทย
ในช่อง value ของ widget Text กดปุ่ม **JS EDITOR** (หรือ `.JS`) แล้วเขียน เช่น

สถานะห้องน้ำ:
```javascript
var s = datasources["gw"]["shadow"]["clean_status"];
if (s == "now")  return "🔴 ต้องทำความสะอาดทันที";
if (s == "soon") return "🟡 ใกล้ถึงเวลาทำความสะอาด";
return "🟢 สะอาด";
```
พัดลม:
```javascript
var names = ["ปิด", "ต่ำ", "กลาง", "สูง"];
var lv = datasources["gw"]["shadow"]["fan_level"];
var stop = datasources["gw"]["shadow"]["estop"] == 1 ? " (หยุดฉุกเฉิน)" : "";
return "พัดลม: " + names[lv] + stop;
```
สถานะเซ็นเซอร์:
```javascript
var sh = datasources["gw"]["shadow"];
function st(v) { return v == 1 ? "✅" : "❌"; }
return "โซนทางเข้า " + st(sh["wc1_online"]) + "   โซนโถ " + st(sh["wc2_online"]) + "   AI " + st(sh["ai_online"]);
```
(ถ้า path ของ datasource ที่ระบบใส่ให้ในข้อ 4.1 ต่างจากนี้ ให้เปลี่ยน `datasources["gw"]["shadow"]` ให้ตรง)

**ค่ากลิ่นคืออะไร (ใส่เป็นคำอธิบายเล็กๆ ใต้ gauge):** "1.0 = อากาศปกติ, ยิ่งสูงยิ่งมีกลิ่น"

## ขั้นที่ 5: เก็บงานและ commit (พฤ. 19)

1. capture หน้าจอ dashboard เก็บไว้ที่ `dashboard/screenshots/`
2. สร้าง `dashboard/README.md` เขียน: ชื่อ dashboard, datasource ที่ใช้, widget แต่ละตัวผูกกับ field อะไร, โค้ด JS ข้อ 4.3, ลิงก์ dashboard (ถ้าแชร์แบบ public ได้)
3. ถ้าระบบให้ export dashboard เป็นไฟล์ JSON ได้ ให้ export ไว้ที่ `dashboard/dashboard-export.json` (เผื่อพังจะได้ import กลับ)
4. commit ไฟล์ของคุณทั้งหมด:
   ```bash
   git add dashboard/ firmware/gateway/include/discord.h firmware/gateway/src/discord.cpp ai/notifier.py tools/fake_gateway.py tools/gateway_secrets.example.py
   git commit -m "dashboard: NETPIE dashboard, schema, Discord notifier"
   git push
   ```
   ตรวจ `git status` ว่าไม่มี `config_secrets.py` / `gateway_secrets.py` ติดไปด้วย

## ขั้นที่ 6: วันรวมระบบ (ศ. 20)

1. ปิด `fake_gateway.py` (ห้ามเปิดพร้อม gateway จริง)
2. เปิด dashboard ค้างไว้ เมื่อ gateway จริงเริ่มส่ง ค่าต้องเปลี่ยนตามจริง:
   - เอามือผ่านหน้า KY-032 → ค่าจำนวนคนเพิ่ม
   - เป่าลมหรือเปิดขวดน้ำยาเช็ดกระจกใกล้ MQ-137 → gauge กลิ่นขึ้น
   - เป่าลมหายใจใส่ KY-015 → ความชื้นขึ้น
   - ยืนหน้า ultrasonic → ไฟ "มีคนอยู่" ติด
3. ช่วยถ่ายหน้าจอ dashboard ตอนอัดวิดีโอ checkpoint

## ขั้นที่ 7: แจ้งเตือนจริง (ส.–อา. 21–22)

1. ส่ง `DISCORD_WEBHOOK_URL` (ทางแชทส่วนตัว) ให้สุรสิทธิ์ใส่ใน `secrets.h` ของ gateway และให้ณัฐณิชาใส่ใน `ai/config_secrets.py`
2. ทดสอบฝั่ง gateway: วางฟองน้ำเปียกบน leak sensor → ต้องได้ "⚠️ ตรวจพบพื้นเปียก" และพอแห้งได้ "✅ พื้นแห้งแล้ว"
3. **ทดสอบว่ากันแจ้งเตือนรัวได้:** ใน `ai/` รัน
   ```bash
   python -c "from notifier import Notifier; n=Notifier(); print(n.send('x','ทดสอบครั้งที่ 1')); print(n.send('x','ทดสอบครั้งที่ 2'))"
   ```
   ต้องพิมพ์ `True` แล้ว `False` และchannel Discord ได้ข้อความแค่ "ทดสอบครั้งที่ 1" (เรื่องเดิมจะไม่ส่งซ้ำภายใน 15 นาที)
4. **เปลี่ยนชื่อห้อง:** แก้ `ROOM_NAME` เป็นชื่อห้องน้ำจริง (เช่น `"ห้องน้ำหญิง ชั้น 3 ตึก 4"`) ทั้งใน `ai/notifier.py` และ `firmware/gateway/include/discord.h` ถ้าอยากปรับถ้อยคำอื่น แก้ฟังก์ชัน `msg_...` ใน `notifier.py` และ `MSG_...` ใน `discord.h`
5. commit:
   ```bash
   git add ai/notifier.py firmware/gateway/include/discord.h
   git commit -m "alerts: set room name and alert wording"
   git push
   ```
   แล้วบอกสุรสิทธิ์ให้ `git pull` และแฟลช gateway ใหม่ (ข้อความใน `discord.h` จะเปลี่ยนเมื่อแฟลชใหม่เท่านั้น)

### ส่วนที่ 2: ทดสอบแจ้งเตือนฝั่ง Python (จ. 30 พ.ย. หลังณัฐณิชาเปิด `service.py` วันที่ 29)
1. ปิดปลั๊ก gateway 6 นาที → channel Discord ต้องได้ "🔴 ระบบขาดการเชื่อมต่อมา 5 นาที..." แล้วเสียบกลับ → ได้ "🟢 ระบบกลับมาออนไลน์แล้ว"
2. ถอดสาย AO ของ MQ-137 บน node WC-2 → ภายใน 1 นาทีได้ "🟠 เซ็นเซอร์กลิ่นของ WC2 อ่านค่าไม่ได้" แล้วเสียบสายกลับ
3. ถอดปลั๊ก node WC-1 รอ 2 นาที → ได้ "🟠 เซ็นเซอร์ WC1 ไม่ส่งข้อมูล" แล้วเสียบกลับ
4. ถ้าข้อไหนไม่ได้ข้อความ ดูหน้าจอ `service.py` ว่าขึ้น `[ALERT ไม่ได้ส่ง ยังไม่มี webhook URL]` ไหม (แปลว่า `config_secrets.py` ของณัฐณิชายังไม่ได้ใส่ `DISCORD_WEBHOOK_URL`) ถ้าขึ้น `[DISCORD] error 404` แปลว่า URL ผิดหรือ webhook ถูกลบ
5. capture หน้าจอ Discord ที่มีข้อความครบทุกแบบ เก็บไว้ที่ `dashboard/screenshots/discord-alerts.png` แล้ว commit (ใช้ในรายงานข้อ 3.4)

## ขั้นที่ 8: ปรับ UX (23–27)

1. ให้คน 3 คนที่ไม่รู้จักโปรเจค (เพื่อนคนอื่น คนในบ้าน) ดู dashboard บนมือถือ 30 วินาทีโดยไม่อธิบายอะไร แล้วถาม 3 ข้อ:
   - "ห้องน้ำนี้ต้องทำความสะอาดไหม"
   - "พัดลมเปิดอยู่ระดับไหน"
   - "เซ็นเซอร์ทุกตัวทำงานปกติไหม"
2. จดผลลงไฟล์ใหม่ `dashboard/ux-test.md`:
   ```markdown
   | คนที่ | ข้อ 1 ตอบถูก? | ข้อ 2 ตอบถูก? | ข้อ 3 ตอบถูก? | ความเห็น | แก้อะไรหลังจากนี้ |
   | --- | --- | --- | --- | --- | --- |
   | 1 | ✔ | ✘ | ✔ | ไม่รู้ว่าเลข 2 คือระดับพัดลม | เปลี่ยนเป็นคำว่า "พัดลม: กลาง" |
   ```
   ข้อไหนมีคนตอบผิด ให้แก้ dashboard จุดนั้น แล้วทดสอบกับคนใหม่อีกรอบ (ไฟล์นี้ใช้ในรายงานข้อ 3.4 เป็นหลักฐานคะแนน User Experience)
3. ใช้สีให้สม่ำเสมอ: เขียว = ปกติ, เหลือง = ใกล้ถึงเวลา, แดง = ต้องทำทันที/มีปัญหา
4. ตัดค่าที่แม่บ้านไม่ต้องใช้ (`seq`, `recv`) ออกจากหน้าหลัก ย้ายไปหน้า "สำหรับผู้ดูแลระบบ" แยก (ถ้าระบบมีหลายหน้า)
5. ทดสอบเปิดบนมือถือ เพราะแม่บ้านจะดูจากมือถือ
6. ข้อความ Discord: อ่านแล้วต้องรู้ทันทีว่า **ห้องไหน เกิดอะไร ต้องทำอะไร** ถ้าข้อความไหนไม่ครบ 3 อย่างนี้ ให้แก้ใน `notifier.py` / `discord.h`
7. เสร็จแล้ว capture หน้าจอ dashboard ใหม่ (มือถือ + คอม) ทับไฟล์เดิมใน `dashboard/screenshots/` อัปเดต `dashboard/README.md` ให้ตรง แล้ว commit ทั้งโฟลเดอร์ `dashboard/`

## ขั้นที่ 9: วัด latency (30 พ.ย.–3 ธ.ค. ร่วมกับณัฐณิชา)

**วิธีวัด end-to-end (sensor → dashboard) ด้วยการอัดหน้าจอ:**
1. ต่อ node WC-1 เข้าคอมด้วย USB เปิด Serial Monitor ไว้ครึ่งจอซ้าย และ dashboard ไว้ครึ่งจอขวา
2. อัดหน้าจอที่ 30 fps ขึ้นไป (Windows: Xbox Game Bar `Win+G`, Mac: `Cmd+Shift+5`)
3. เอามือผ่านหน้า KY-032 แล้วรอดู: บรรทัด `... OK` ใน Serial (node ส่งแล้ว) → ค่าบน dashboard เปลี่ยน
4. ทำ 30 ครั้ง
5. เปิดวิดีโอทีละเฟรม นับเฟรมจากบรรทัด `OK` โผล่จนถึงค่าบน dashboard เปลี่ยน แล้วหารด้วย fps = latency (วินาที)
6. บันทึกลงไฟล์ `docs/metrics/latency_e2e.csv` (เปิดด้วย Notepad/VS Code ชื่อคอลัมน์ต้องเป็นภาษาอังกฤษตามนี้ เพราะ `metrics.py` อ่านไฟล์นี้):
   ```
   trial,frames,fps
   1,215,30
   2,301,30
   ```
   แล้ว commit (ณัฐณิชารัน `metrics.py` จะได้ค่าเฉลี่ย/ต่ำสุด/สูงสุดให้อัตโนมัติ)
7. หมายเหตุ: gateway ส่งขึ้น cloud ทุก 10 วินาที ค่า latency จึงมีเวลารอรอบส่ง 0–10 วินาทีรวมอยู่ด้วย ต้องเขียนอธิบายในรายงาน

### ทดสอบ T3, T5, T6 (1–3 ธ.ค. บนห้องน้ำจำลอง)
ทำแต่ละข้อ 3 รอบ แล้วเขียนผลจริงในคอลัมน์ "ผลจริง" ของ `docs/test-plan.md` แล้ว commit
| ID | ทำอะไร | ดูอะไร |
| --- | --- | --- |
| T3 | วางฟองน้ำเปียกบน leak sensor พร้อมกดจับเวลา | กี่วินาทีจน Discord ได้ "พื้นเปียก" และ dashboard ไฟแดง |
| T5 | พ่นน้ำยาเช็ดกระจกในห้องจำลองจนสถานะบน dashboard เป็น 🔴 แล้วยืนหน้า ultrasonic ไว้ (ห่างไม่เกิน 50 ซม.) | ระหว่างที่ `occupied` = 1 ต้อง**ไม่มี**ข้อความ "ควรทำความสะอาด" พอถอยออกจน `occupied` = 0 ข้อความต้องมาภายใน 1–2 นาที |
| T6 | ทำให้สถานะเป็น 🔴 ค้างไว้ 30 นาที (พ่นน้ำยาซ้ำทุก 5 นาที) | นับจำนวนข้อความ "ควรทำความสะอาด" ต้องไม่เกิน 2 ข้อความ (cooldown 15 นาที) |

## ขั้นที่ 10: หาข้อมูลระบบที่คล้ายกัน (28–29)

ใช้เขียนรายงานข้อ 2 (1 คะแนน) และณัฐณิชาใช้เปรียบเทียบในรายงานข้อ 4 (คะแนน Analysis ต้องเทียบกับระบบที่มีอยู่)
1. ค้นใน Google และ Google Scholar ด้วยคำว่า "smart restroom", "smart washroom monitoring", "restroom odor sensor ammonia", "ห้องน้ำอัจฉริยะ"
2. เลือก **3–4 ระบบ** ให้มีทั้งผลิตภัณฑ์ที่ขายจริงอย่างน้อย 1 ระบบ และงานวิจัยอย่างน้อย 1 งาน
3. สร้างไฟล์ `docs/related-work.md` เป็นตารางนี้ (เติมทุกช่องที่หาได้ ช่องที่หาไม่ได้ใส่ "ไม่ระบุ"):
   ```markdown
   | ระบบ | ประเภท (ผลิตภัณฑ์/งานวิจัย) | เซ็นเซอร์ที่ใช้ | นับคนไหม | แจ้งเตือนยังไง | ใช้ AI/ทำนายไหม | คุมพัดลมเองไหม | ราคา | ลิงก์ |
   | --- | --- | --- | --- | --- | --- | --- | --- | --- |
   | (ชื่อระบบ) | ผลิตภัณฑ์ | NH3, H2S, คนเข้า | ✔ | แอป, dashboard | ✘ | ✘ | ไม่ระบุ | https://... |
   | **OnBedSystem (ของเรา)** | โครงงาน | NH3 (MQ-137), KY-032, KY-015, ultrasonic, น้ำ | ✔ | Discord | ✔ ทำนายล่วงหน้า 30 นาที | ✔ | ประมาณ 2,000–3,000 บาท | |
   ```
4. ใต้ตาราง เขียน 3–4 ประโยคว่าระบบเราต่างจากระบบอื่นตรงไหน (เช่น ทำนายล่วงหน้าแล้วเร่งพัดลมก่อนกลิ่นแรง, ราคาถูกกว่า, ไม่ใช้กล้องจึงไม่มีปัญหาความเป็นส่วนตัว)
5. commit `docs/related-work.md`

## ข้อ 11: เขียนรายงาน (4–5 ธ.ค.)

เขียนใน Google Doc `OnBedSystem_Final_Report` ใต้หัวข้อของคุณ เสร็จภายใน **ส. 5 ธ.ค. 23:59**

### 11.1 หัวข้อ 2 ระบบที่มีอยู่และงานที่เกี่ยวข้อง (ประมาณ 1 หน้า)
1. ย่อหน้าเปิด 2 ประโยค: ปัจจุบันมีระบบห้องน้ำอัจฉริยะแบบไหนบ้าง
2. แต่ละระบบเขียน 2–3 ประโยค: เป็นอะไร ใช้เซ็นเซอร์อะไร ทำอะไรได้ (จากตาราง `docs/related-work.md`) และใส่เลขอ้างอิง เช่น [1]
3. ใส่ตารางเปรียบเทียบจาก `docs/related-work.md` (ตารางที่ X)
4. ปิดด้วยย่อหน้าว่าระบบเราต่างจากระบบอื่นตรงไหน (จากข้อ 4 ของขั้นที่ 10)
5. ใส่ลิงก์ทุกระบบในส่วน "อ้างอิง" ท้ายรายงาน ตามเลข [1], [2], ...

### 11.2 หัวข้อ 3.4 Cloud, dashboard และการแจ้งเตือน (ประมาณ ½ หน้า)
1. **การเก็บข้อมูล (2 ประโยค):** "gateway ส่งข้อมูลเข้า shadow ของ device บน NETPIE2020 ทุก 10 วินาที และตั้ง Data Schema ให้เก็บทุก field ลง feed 60 วัน ข้อมูลย้อนหลังใช้แสดงกราฟและวิเคราะห์ผล"
2. **Dashboard (3 ประโยค + รูป):** แบ่งเป็น 3 แถว (สถานะ, แต่ละโซน, แนวโน้ม) เหตุผลที่จัดแบบนี้ (สิ่งที่แม่บ้านต้องรู้อยู่บนสุด) ใส่ screenshot 1 รูป ใต้รูปเขียน "รูปที่ X dashboard ของระบบ"
3. **ผลทดสอบกับคนจริง (2 ประโยค):** สรุปจาก `dashboard/ux-test.md` เช่น "ทดสอบกับผู้ที่ไม่รู้จักระบบ 3 คน ตอบคำถามสถานะห้องน้ำถูก 3/3 หลังปรับปรุง"
4. **การแจ้งเตือน (ตาราง):**
   | เหตุการณ์ | ใครส่ง | ตัวอย่างข้อความ |
   | --- | --- | --- |
   | พื้นเปียก / พื้นแห้ง | gateway | ⚠️ ...: ตรวจพบพื้นเปียก กรุณาเช็ดพื้น... |
   | ควรทำความสะอาด | AI service | 🧹 ...: ควรทำความสะอาดตอนนี้... |
   | อุปกรณ์ขาดการเชื่อมต่อ | AI service | 🔴 ...: ระบบขาดการเชื่อมต่อมา 5 นาที... |
   | กดปุ่มฉุกเฉิน | gateway | 🛑 ...: มีการกดปุ่มหยุดพัดลมฉุกเฉิน |
   แล้วเขียน 1 ประโยคเรื่องกันแจ้งเตือนรัว (เรื่องเดิมไม่ส่งซ้ำภายใน 15 นาที, gateway ส่งเฉพาะตอนสถานะเปลี่ยน) และไม่แจ้งตอนมีคนในห้อง (เช็ก `occupied` จาก ultrasonic)

### 11.3 ภาคผนวก ค (ไม่นับหน้า)
ใส่ JSON จาก `dashboard/netpie-schema.json` และโค้ด JS ของ widget จาก `dashboard/README.md`

### 11.4 ส่วนที่ทุกคนต้องเขียนเพิ่ม
- ใต้หัวข้อ 5 ของรายงาน: ปัญหาที่เจอในส่วน dashboard/แจ้งเตือน 2–3 บรรทัด (ปัญหา → แก้ยังไง)
- ใต้หัวข้อ 6 ของรายงาน: 1 บรรทัดว่าคุณใช้ AI tool ตัวไหนช่วยทำอะไร

## ข้อ 12: คำถามที่อาจารย์น่าจะถาม

1. **Dashboard ได้ข้อมูลมาจากไหน?** gateway ส่งข้อมูลเข้า shadow ของ device `gateway` บน NETPIE ทุก 10 วินาที dashboard อ่านจาก shadow ส่วนกราฟย้อนหลังอ่านจาก feed
2. **ข้อมูลดิบเก็บที่ไหน?** NETPIE feed (ตาม schema ที่ตั้ง `store` ไว้ 60 วัน) และไฟล์ CSV จาก `collect.py`
3. **Discord ส่งข้อความยังไง?** ใช้ webhook: ส่ง HTTPS POST ไปที่ webhook URL ของ channel พร้อม JSON `{"content": "ข้อความ"}` Discord ตอบ 204 เมื่อสำเร็จ ไม่ต้องสร้างบอทหรือ login URL จึงเป็นความลับที่เก็บใน `secrets.h` / `config_secrets.py` ที่ไม่ขึ้น GitHub
4. **ป้องกันแจ้งเตือนรัวยังไง?** จำเวลาที่ส่งแต่ละเรื่องไว้ เรื่องเดิมไม่ส่งซ้ำภายใน 15 นาที และฝั่ง gateway ส่งเฉพาะตอนสถานะเปลี่ยน (เพิ่งเปียก / เพิ่งแห้ง)
5. **ทำไมไม่แจ้งตอนมีคนในห้อง?** เช็ก `occupied` จาก ultrasonic ก่อนส่งแจ้งเตือนให้ทำความสะอาด จะได้ไม่เรียกแม่บ้านเข้าไปตอนมีคนใช้
