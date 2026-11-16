"""จำลอง gateway: สร้างข้อมูลห้องน้ำปลอมที่สมจริง ใช้ทดสอบ dashboard และ AI โดยไม่ต้องมีบอร์ด
WC-1 (ทางเข้า) มีแค่ KY-032 จึงไม่มี wc1_odor, WC-2 (โซนโถ) มี MQ-137 + KY-032

โหมด 1 ส่งขึ้น NETPIE (ทดสอบ dashboard / service.py):
    python fake_gateway.py --live
    ต้องมีไฟล์ gateway_secrets.py (ข้อมูล device "gateway") และ **ห้ามเปิด gateway จริงพร้อมกัน**

โหมด 2 สร้างไฟล์ CSV (ทดสอบ evaluate.py / metrics.py ก่อนมีข้อมูลจริง):
    python fake_gateway.py --csv ../ai/data/fake.csv --days 3
"""
import argparse
import csv
import json
import math
import os
import random
import time


class FakeBathroom:
    def __init__(self, start_ts: float):
        self.t = start_ts
        self.odor = [1.0, 1.0]
        self.seq = [0, 0]
        self.recv = [0, 0]
        self.fan = 1
        self.leak = 0

    def people_rate(self) -> float:
        """จำนวนคนต่อนาทีตามช่วงเวลา (คนเยอะช่วงสายและบ่าย)"""
        h = time.localtime(self.t).tm_hour + time.localtime(self.t).tm_min / 60
        if h < 7 or h > 20:
            return 0.05
        return 0.3 + 1.2 * math.exp(-((h - 10.5) ** 2) / 2) + 1.0 * math.exp(-((h - 14) ** 2) / 2)

    def step(self) -> dict:
        """เดินเวลาไป 10 วินาที แล้วคืนข้อมูลเหมือนที่ gateway ส่ง"""
        self.t += 10
        people = [0, 0]
        for i in range(2):
            lam = self.people_rate() / 6
            people[i] = sum(1 for _ in range(10) if random.random() < lam / 10)
            self.odor[i] += 0.03 * people[i] * (1.3 if i == 1 else 0.8)   # โซนโถกลิ่นขึ้นเร็วกว่า
            self.odor[i] -= (self.odor[i] - 1.0) * 0.0015 * (1 + self.fan)  # พัดลมช่วยลดกลิ่น
            self.odor[i] += random.gauss(0, 0.01)
            self.seq[i] += 1
            if random.random() < 0.985:  # จำลองข้อความหาย 1.5%
                self.recv[i] += 1
        if self.odor[1] > 3.2 and random.random() < 0.01:  # แม่บ้านทำความสะอาด
            self.odor = [1.0 + random.gauss(0, 0.05) for _ in range(2)]
        m = self.odor[1]  # มี MQ-137 แค่ WC-2 (โซนโถ)
        self.fan = 3 if m >= 2.5 else 2 if m >= 1.5 else 1
        if random.random() < 0.0005:
            self.leak = 1
        elif self.leak and random.random() < 0.02:
            self.leak = 0
        h = time.localtime(self.t).tm_hour
        return {
            "ts": int(self.t), "ts_ms": 0,
            "wc1_people": people[0], "wc1_seq": self.seq[0],
            "wc1_recv": self.recv[0], "wc1_online": 1,
            "wc2_odor": round(self.odor[1], 2), "wc2_people": people[1], "wc2_seq": self.seq[1],
            "wc2_recv": self.recv[1], "wc2_online": 1,
            "temp": round(29 + 2 * math.sin((h - 9) / 24 * 2 * math.pi) + random.gauss(0, 0.2), 1),
            "hum": round(min(95, 70 + 3 * sum(people) + random.gauss(0, 1.5)), 1),
            "occupied": 1 if sum(people) else int(random.random() < 0.05),
            "dist_cm": random.randint(20, 50) if sum(people) else random.randint(150, 160),
            "leak": self.leak, "fan_level": self.fan, "clean_status": "ok", "ai_online": 0, "estop": 0,
        }


def run_csv(path: str, days: float):
    b = FakeBathroom(time.time() - days * 86400)
    n = int(days * 8640)
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", newline="") as f:
        w = None
        for _ in range(n):
            d = b.step()
            d["recv_ts"] = d["ts"] + random.uniform(0.15, 0.6)
            d["buffered"] = 0
            if w is None:
                w = csv.DictWriter(f, fieldnames=list(d.keys()))
                w.writeheader()
            w.writerow(d)
    print(f"เขียน {n} แถว ({days} วัน) ลง {path}")


def run_live(interval: float):
    import paho.mqtt.client as mqtt
    from gateway_secrets import NETPIE_CLIENT_ID, NETPIE_TOKEN, NETPIE_SECRET
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=NETPIE_CLIENT_ID)
    c.username_pw_set(NETPIE_TOKEN, NETPIE_SECRET)
    c.connect("mqtt.netpie.io", 1883, keepalive=30)
    c.loop_start()
    b = FakeBathroom(time.time())
    print("ส่งข้อมูลปลอมทุก", interval, "วินาที (Ctrl+C เพื่อหยุด)")
    while True:
        d = b.step()
        d["ts"] = int(time.time())
        payload = json.dumps({"data": d})
        c.publish("@shadow/data/update", payload)
        c.publish("@msg/onbed/raw", payload)
        print(payload)
        time.sleep(interval)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--live", action="store_true", help="ส่งขึ้น NETPIE")
    ap.add_argument("--interval", type=float, default=10)
    ap.add_argument("--csv", help="สร้างไฟล์ CSV แทน")
    ap.add_argument("--days", type=float, default=3)
    a = ap.parse_args()
    if a.csv:
        run_csv(a.csv, a.days)
    elif a.live:
        run_live(a.interval)
    else:
        ap.print_help()
