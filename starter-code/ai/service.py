"""AI service: รับข้อมูลจาก gateway → Chronos-2 ทำนายค่ากลิ่นอีก 30 นาที → สั่งพัดลม + แจ้งเตือน (closed loop, M4)
บันทึกข้อมูลลง data/raw.csv ไปด้วย (ไม่ต้องเปิด collect.py พร้อมกัน เพราะใช้ Client ID เดียวกัน)

วิธีรัน:  python service.py      หยุด: Ctrl+C
"""
import json
import os
import threading
import time

import pandas as pd

from collector import append_row, DATA_FILE
from features import to_minutes
from forecaster import load_pipeline, forecast_latest, CONTEXT_MIN
from netpie import make_client, TOPIC_CMD
import notifier as nt

HERE = os.path.dirname(__file__)
with open(os.path.join(HERE, "models", "config.json"), encoding="utf-8") as f:  # สร้างโดย evaluate.py
    cfg = json.load(f)
TH_MID, TH_HIGH = cfg["th_mid"], cfg["th_high"]
print(f"กำลังโหลดโมเดล {cfg['name']} ...")
pipeline = load_pipeline()
print(f"โหลดโมเดลแล้ว (MAE {cfg['mae']:.3f}, baseline {cfg['mae_baseline']:.3f}) เกณฑ์ soon>={TH_MID:.3f} now>={TH_HIGH:.3f}")

HISTORY_SEC = CONTEXT_MIN * 60   # เก็บประวัติในหน่วยความจำ 6 ชั่วโมง (ต้องมี >= 60 นาทีถึงจะทำนายได้)
PREDICT_EVERY_SEC = 60       # ทำนายทุก 1 นาที
OFFLINE_ALERT_SEC = 5 * 60   # gateway เงียบเกิน 5 นาที → แจ้งเตือน

rows = []
lock = threading.Lock()
last_msg_time = time.time()
last_predict = 0.0
offline_alerted = False
alerts = nt.Notifier()


def load_history():
    """โหลดข้อมูล 6 ชั่วโมงล่าสุดจาก CSV จะได้ทำนายได้ทันทีหลังเปิดโปรแกรม"""
    if not os.path.exists(DATA_FILE):
        return
    df = pd.read_csv(DATA_FILE)
    df = df[df["ts"] >= time.time() - HISTORY_SEC]
    rows.extend(df.to_dict("records"))
    print(f"โหลดประวัติ {len(rows)} แถวจาก {DATA_FILE}")


def decide(pred: float):
    if pred >= TH_HIGH:
        return 3, "now"
    if pred >= TH_MID:
        return 2, "soon"
    return 1, "ok"


def predict_and_command(client, latest: dict):
    m = to_minutes(pd.DataFrame(rows))
    if m.empty or (pd.Timestamp.now(tz="Asia/Bangkok") - m.index[-1]).total_seconds() > 300:
        print("[AI] ข้อมูลล่าสุดเก่าเกินไป ข้าม")
        return
    pred = forecast_latest(pipeline, m)
    if pred is None:
        print("[AI] ประวัติยังไม่ถึง 60 นาที ยังทำนายไม่ได้ (gateway ใช้ fallback อยู่)")
        return
    fan, status = decide(pred)
    client.publish(TOPIC_CMD, json.dumps({"fan": fan, "status": status, "pred": round(pred, 3)}))
    print(f"[AI] odor now={m['odor'].iloc[-1]:.2f} → อีก 30 นาที={pred:.2f}  fan={fan} status={status}")
    people_60m = int(m["people"].tail(60).sum())

    # แจ้งแม่บ้านเมื่อถึงเวลาทำความสะอาด แต่ไม่แจ้งตอนมีคนอยู่ในห้อง
    if status == "now" and int(latest.get("occupied", 0)) == 0:
        alerts.send("clean_now", nt.msg_clean_now(people_60m))
    elif status == "soon":
        alerts.send("clean_soon", nt.msg_clean_soon())


def check_devices(data: dict):
    for key in ("wc1", "wc2"):
        if f"{key}_online" in data and int(data[f"{key}_online"]) == 0:
            alerts.send(f"{key}_offline", nt.msg_node_offline(key))
        if f"{key}_odor" in data and float(data[f"{key}_odor"]) < 0:
            alerts.send(f"{key}_fault", nt.msg_sensor_fault(key))


def on_message(client, userdata, msg):
    global last_msg_time, last_predict, offline_alerted
    try:
        data = json.loads(msg.payload)["data"]
    except (ValueError, KeyError):
        return
    row = append_row(data)
    with lock:
        rows.append(row)
        cutoff = time.time() - HISTORY_SEC
        while rows and float(rows[0].get("ts", 0)) < cutoff:
            rows.pop(0)
        last_msg_time = time.time()
        if offline_alerted:
            offline_alerted = False
            alerts.send("back", nt.msg_gateway_back(), force=True)
        if data.get("buffered"):
            return  # ข้อมูลย้อนหลัง ไม่ต้องทำนาย
        check_devices(data)
        if time.time() - last_predict >= PREDICT_EVERY_SEC:
            last_predict = time.time()
            try:
                predict_and_command(client, data)
            except Exception as e:  # อย่าให้ service ล่มเพราะข้อมูลแปลกแถวเดียว
                print("[AI] error:", e)


if __name__ == "__main__":
    load_history()
    client = make_client(on_message)
    client.loop_start()
    try:
        while True:
            time.sleep(10)
            silent = time.time() - last_msg_time
            if silent > OFFLINE_ALERT_SEC and not offline_alerted:
                offline_alerted = True
                alerts.send("offline", nt.msg_gateway_offline(int(silent // 60)), force=True)
    except KeyboardInterrupt:
        client.loop_stop()
