# Wiring

| บอร์ด | อุปกรณ์ | ขาอุปกรณ์ | ต่อกับ | หมายเหตุ |
| --- | --- | --- | --- | --- |
| Node WC-2 | MQ-137 | VCC | ESP32 VIN (5V) | WC-1 ไม่มี MQ-137 |
| Node WC-2 | MQ-137 | GND | ESP32 GND | |
| Node WC-2 | MQ-137 | AO | ตัวต้านทาน 10k → GPIO34, และ GPIO34 → ตัวต้านทาน 20k → GND | voltage divider ลด 5 V เหลือ 3.3 V |
| Node WC-1, WC-2 | KY-032 | + / GND / OUT | 3.3V / GND / GPIO27 | ขา EN ไม่ต่อ (เสียบ jumper EN ไว้) |
| Gateway | KY-015 | S / ขากลาง (+) / − | GPIO4 / 3.3V / GND | |
| Gateway | Ultrasonic | VCC / GND / Trig | VIN (5V) / GND / GPIO26 | |
| Gateway | Ultrasonic | Echo | ตัวต้านทาน 10k → GPIO33, และ GPIO33 → ตัวต้านทาน 20k → GND | Echo ออก 5 V ต้องลดเหลือ 3.3 V |
| Gateway | Water level module | + / − / S | 3.3V / GND / GPIO35 | |
| Gateway | ปุ่มหยุดฉุกเฉิน | ขา 1 / ขา 2 | GPIO14 / GND | |
| Gateway | MOSFET module | PWM(TRIG) / GND | GPIO25 / GND ของ ESP32 | GND ต้องต่อร่วมกับ adapter |
| Gateway | MOSFET module | VIN+ / VIN− | Adapter 12V + / − | |
| Gateway | MOSFET module | OUT+ / OUT− | พัดลมสายแดง / สายดำ | ไดโอดคร่อมพัดลม ขีดขาวไปทางสายแดง |
| Gateway | Buck converter (MP1584EN) | IN+ / IN− | Adapter 12V + / − | |
| Gateway | Buck converter (MP1584EN) | OUT+ / OUT− | ESP32 VIN / GND | **ตั้ง 5.0 V ด้วยมัลติมิเตอร์ก่อนต่อ** |