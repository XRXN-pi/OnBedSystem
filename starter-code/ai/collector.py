"""บันทึกข้อมูลจาก gateway ลงไฟล์ CSV (ใช้ร่วมกันโดย collect.py และ service.py)"""
import csv
import os
import time

DATA_FILE = os.path.join(os.path.dirname(__file__), "data", "raw.csv")

FIELDS = [
    "ts", "ts_ms", "recv_ts", "buffered",
    "wc1_people", "wc1_seq", "wc1_recv", "wc1_online",
    "wc2_seq", "wc2_recv", "wc2_online",
    "odor", "temp", "hum", "occupied", "dist_cm", "leak", "fan_level", "clean_status", "ai_online", "estop",
]


def append_row(data: dict) -> dict:
    """เติมเวลาที่ได้รับ แล้วต่อท้ายไฟล์ CSV คืนค่าแถวที่บันทึก"""
    row = dict(data)
    row["recv_ts"] = round(time.time(), 3)
    row.setdefault("buffered", 0)
    os.makedirs(os.path.dirname(DATA_FILE), exist_ok=True)
    new_file = not os.path.exists(DATA_FILE)
    with open(DATA_FILE, "a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS, extrasaction="ignore")
        if new_file:
            w.writeheader()
        w.writerow(row)
    return row
