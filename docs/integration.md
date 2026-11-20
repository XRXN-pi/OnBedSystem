## Set Up
- git pull
- เปิด hotspot OnBed-Hotspot, 2.4 GHz
- fake_gateway.py และ gateway_fake ปิดหมดแล้ว (ห้ามซ้อน Client ID)

## Quick Start
1. gateway แฟลช env:gateway, Serial ขึ้น "WiFi OK channel=X" และ "[MQTT] connecting... OK"
2. จด Gateway MAC → ใส่ใน config.h ของ node → แฟลช env:wc1, env:wc2
3. เปิด node WC-1 → Serial node ขึ้น "ESP-NOW ready on channel X" (X ตรงกับ gateway) และ "OK"
4. gateway Serial ขึ้น "[NODE] new node registered: WC-1" และ "[ESP-NOW] WC-1 ..."
5. เปิด node WC-2 → gateway ขึ้น "[ESP-NOW] WC-2 ..."
6. NETPIE portal → device gateway → Shadow มี wc1_*, wc2_*, temp, hum อัปเดตทุก 10 วินาที
7. dashboard ค่าเปลี่ยนตามจริง (เอามือผ่านหน้า KY-032 ที่ประตู / เปิดขวดน้ำยาเช็ดกระจกใกล้ MQ-137 ที่ gateway / เป่าลมหายใจใส่ KY-015 / บังหน้า ultrasonic ของ WC-1 / แตะฟองน้ำเปียกบน water level ของ WC-2)
8. collect.py บันทึกลง CSV
9. ทุกคน commit + push

## Error
| error | fix |
| --- | --- |
| gateway ไม่ขึ้น MQTT OK | Client ID/Token/Secret ใน `secrets.h`, มีโปรแกรมอื่นใช้ Client ID เดียวกันอยู่ไหม |
| node ขึ้น FAILED | MAC ใน `config.h`, ชื่อ hotspot, เปิด gateway ก่อน node |
| gateway ได้ข้อมูลแต่ dashboard ไม่ขึ้น | Shadow ใน portal อัปเดตไหม → ถ้าใช่ ปัญหาอยู่ที่ datasource ของ dashboard |
| collect.py ไม่ได้ข้อมูล | device `ai-service` อยู่ group เดียวกับ `gateway` ไหม |

# log
**รูปแบบ**
```- 2026-11-20 18:30 node WC-2 ส่ง FAILED → MAC ใน config.h พิมพ์ผิด 1 หลัก → แก้แล้วแฟลชใหม่ ผ่าน```
- 2026-11-20 12:30 run code ไม่ผ่าน → แก้ environment แล้วรันผ่าน
- 2026-11-20 13:04 BUTTON_PIN กดแล้วเป็นปุ่มเพิ่มไฟ → แก้ code