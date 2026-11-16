"""คำนวณ metrics จาก data/raw.csv แล้วเขียนผลลง docs/metrics/results.md

วิธีรัน:  python metrics.py
         python metrics.py data/raw.csv
ความแม่นยำการนับคนอ่านจาก docs/metrics/count_test.csv (ผลทดสอบ T1 ของสิรวิชญ์)
มีคอลัมน์: round,actual,system   เช่น  1,20,19
"""
import os
import sys
import numpy as np
import pandas as pd

HERE = os.path.dirname(__file__)
raw_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "data", "raw.csv")
count_path = os.path.join(HERE, "..", "docs", "metrics", "count_test.csv")
out_path = os.path.join(HERE, "..", "docs", "metrics", "results.md")

df = pd.read_csv(raw_path).sort_values("ts").reset_index(drop=True)
df["buffered"] = df.get("buffered", 0).fillna(0)
lines = ["# ผลการวัด Metrics", "", f"ข้อมูล: `{os.path.basename(raw_path)}` ({len(df)} แถว)", ""]


def stats(s: pd.Series, unit: str, digits: int = 2) -> str:
    return (f"mean {s.mean():.{digits}f}{unit}, min {s.min():.{digits}f}{unit}, "
            f"max {s.max():.{digits}f}{unit}, SD {s.std():.{digits}f}{unit} (n={len(s)})")


# ---------- 1. Uptime ----------
start, end = df["ts"].min(), df["ts"].max()
hours = (end - start) / 3600
expected = (end - start) / 10 + 1
got = df["ts"].nunique()
gaps = df["ts"].diff()
big_gaps = gaps[gaps > 60]
lines += ["## 1. Uptime", "",
          f"- ช่วงเวลา: {pd.to_datetime(start, unit='s', utc=True).tz_convert('Asia/Bangkok'):%Y-%m-%d %H:%M} ถึง "
          f"{pd.to_datetime(end, unit='s', utc=True).tz_convert('Asia/Bangkok'):%Y-%m-%d %H:%M} ({hours:.1f} ชั่วโมง)",
          f"- ได้รับ {got} จากที่ควรได้ {expected:.0f} รอบ → **uptime {got / expected * 100:.2f}%**",
          f"- ช่วงที่ข้อมูลหายเกิน 1 นาที: {len(big_gaps)} ครั้ง รวม {big_gaps.sum() / 60:.1f} นาที", ""]

# ---------- 2. Packet delivery rate ----------
lines += ["## 2. Packet delivery rate (node → gateway)", "",
          "วิธีวัด: node ใส่เลข `seq` เพิ่มทีละ 1 ทุกข้อความ gateway นับจำนวนที่ได้รับจริง (`recv`) "
          "delivery rate = จำนวนที่ได้รับ ÷ จำนวนที่ node ส่ง (ส่วนต่างของ seq) คิดเป็นรายชั่วโมงเพื่อดูความแปรปรวน", ""]
for key in ("wc1", "wc2"):
    if f"{key}_seq" not in df:
        continue
    d = df[["ts", f"{key}_seq", f"{key}_recv"]].dropna()
    d = d[d["ts"].diff().fillna(0) >= 0]
    # แบ่งช่วงเมื่อ node หรือ gateway รีบูต (seq หรือ recv ลดลง)
    reset = (d[f"{key}_seq"].diff() < 0) | (d[f"{key}_recv"].diff() < 0)
    d = d.assign(segment=reset.cumsum(), hour=(d["ts"] // 3600))
    sent = recv = 0
    hourly = []
    for _, g in d.groupby(["segment", "hour"]):
        ds = g[f"{key}_seq"].iloc[-1] - g[f"{key}_seq"].iloc[0]
        dr = g[f"{key}_recv"].iloc[-1] - g[f"{key}_recv"].iloc[0]
        if ds >= 30:  # ใช้เฉพาะชั่วโมงที่มีข้อมูลพอ (~5 นาทีขึ้นไป)
            hourly.append(min(dr / ds, 1.0) * 100)
            sent += ds
            recv += dr
    if sent:
        lines.append(f"- **{key.upper()}**: รวม {recv:.0f}/{sent:.0f} = **{recv / sent * 100:.2f}%**, "
                     f"รายชั่วโมง {stats(pd.Series(hourly), '%')}")
lines.append("")

# ---------- 3. Latency gateway → cloud → laptop ----------
live = df[df["buffered"] == 0].copy()
if "ts_ms" in live and "recv_ts" in live:
    lat = (live["recv_ts"] - (live["ts"] + live["ts_ms"].fillna(0) / 1000)) * 1000
    lat = lat[(lat > -2000) & (lat < 10000)]
    lines += ["## 3. Latency (gateway → NETPIE → คอมที่รัน collector)", "",
              "วิธีวัด: gateway ใส่เวลาส่ง (`ts`, `ts_ms`) จากนาฬิกา NTP คอมบันทึกเวลาที่ได้รับ (`recv_ts`) "
              "ผลต่าง = เวลาเดินทางผ่าน cloud (นาฬิกาสองเครื่องต่างกันได้ประมาณ ±50 ms)", "",
              f"- {stats(lat, ' ms', 0)}", ""]

# ---------- 3b. Latency end-to-end (จากการอัดหน้าจอ) ----------
e2e_path = os.path.join(HERE, "..", "docs", "metrics", "latency_e2e.csv")
if os.path.exists(e2e_path):
    e2e = pd.read_csv(e2e_path)
    if "latency_ms" not in e2e and {"frames", "fps"} <= set(e2e.columns):
        e2e["latency_ms"] = e2e["frames"] / e2e["fps"] * 1000
    lines += ["## 3b. Latency end-to-end (sensor → dashboard)", "",
              "วิธีวัด: อัดหน้าจอที่แสดง Serial Monitor ของ node และ dashboard พร้อมกัน นับเฟรมจาก node ส่ง (บรรทัด OK) "
              "จนค่าบน dashboard เปลี่ยน หารด้วย fps (รวมเวลารอรอบส่งของ gateway 0–10 วินาที)", "",
              f"- {stats(e2e['latency_ms'], ' ms', 0)}", ""]

# ---------- 4. ความแม่นยำการนับคน ----------
if os.path.exists(count_path):
    ct = pd.read_csv(count_path)
    ct["err"] = (ct["system"] - ct["actual"]).abs() / ct["actual"] * 100
    lines += ["## 4. ความแม่นยำการนับคน (KY-032, ทดสอบ T1)", "",
              "วิธีวัด: เดินผ่าน KY-032 ของ WC-1 ช้าๆ ครั้งละ 20 ครั้ง (จำนวนจริงที่รู้แน่นอน) เทียบกับจำนวนที่ระบบนับได้ ทำซ้ำหลายรอบ", "",
              "| รอบ | จำนวนจริง | ระบบนับ | คลาดเคลื่อน |", "| --- | --- | --- | --- |"]
    for _, r in ct.iterrows():
        lines.append(f"| {int(r['round'])} | {int(r['actual'])} | {int(r['system'])} | {r['err']:.1f}% |")
    lines += ["", f"- ความคลาดเคลื่อน: {stats(ct['err'], '%', 1)}", ""]

# ---------- 5. AI ----------
cfg_path = os.path.join(HERE, "models", "config.json")  # สร้างโดย evaluate.py
if os.path.exists(cfg_path):
    import json
    b = json.load(open(cfg_path, encoding="utf-8"))
    lines += ["## 5. ความแม่นยำของโมเดล AI", "",
              f"- โมเดล {b['name']} (สำเร็จรูป ไม่ได้เทรนเพิ่ม) ทำนายค่ากลิ่นล่วงหน้า {b['horizon_min']} นาที: "
              f"MAE {b['mae']:.3f} จาก {b['test_points']} จุดทดสอบบนข้อมูลจริง {b['data_hours']} ชั่วโมง "
              f"(baseline เดาว่าไม่เปลี่ยน: {b['mae_baseline']:.3f})", ""]

os.makedirs(os.path.dirname(out_path), exist_ok=True)
open(out_path, "w", encoding="utf-8").write("\n".join(lines))
print("\n".join(lines))
print(f"\nเขียนผลลง {os.path.abspath(out_path)}")
