"""วัดความแม่นของ Chronos-2 บนข้อมูลจริงของเรา เทียบกับ baseline แล้วตั้งเกณฑ์ระดับพัดลม
บันทึกผลเป็น models/config.json (service.py ใช้ไฟล์นี้) และกราฟ models/prediction.png

ไม่มีการเทรน: Chronos-2 เป็นโมเดลสำเร็จรูป ใช้ได้ทันที (zero-shot)

วิธีรัน:  python evaluate.py            (ใช้ data/raw.csv)
         python evaluate.py other.csv  (ระบุไฟล์เอง)
"""
import json
import os
import sys
import time

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

from features import to_minutes
from forecaster import load_pipeline, forecast_at, HORIZON_MIN, MIN_CONTEXT_MIN, MODEL_ID

HERE = os.path.dirname(__file__)
csv_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "data", "raw.csv")
STEP_MIN = 10  # ทดสอบทุก 10 นาที

m = to_minutes(pd.read_csv(csv_path))
print(f"ข้อมูลรายนาที: {len(m)} นาที ({len(m) / 60:.1f} ชั่วโมง)")
if len(m) < MIN_CONTEXT_MIN + HORIZON_MIN + 60:
    sys.exit("ข้อมูลน้อยเกินไป (ต้องมีอย่างน้อย ~3 ชั่วโมง) เก็บข้อมูลเพิ่มก่อน")

# จุดทดสอบ: ทุก 10 นาที ที่มีประวัติย้อนหลัง >= 60 นาที และรู้ค่าจริงในอีก 30 นาที
odor = m["odor"].to_numpy()
points = [p for p in range(MIN_CONTEXT_MIN - 1, len(m) - HORIZON_MIN, STEP_MIN)
          if not np.isnan(odor[p]) and not np.isnan(odor[p + HORIZON_MIN])]
print(f"จุดทดสอบ: {len(points)} จุด")

print(f"กำลังโหลดโมเดล {MODEL_ID} (ครั้งแรกจะดาวน์โหลด ~500 MB) ...")
pipeline = load_pipeline()
t0 = time.time()
pred = forecast_at(pipeline, m, points)
sec = time.time() - t0
print(f"ทำนาย {len(points)} จุด ใช้เวลา {sec:.1f} วินาที ({sec / len(points) * 1000:.0f} ms ต่อจุด)")

actual = pd.Series(odor[[p + HORIZON_MIN for p in points]], index=points)  # ค่ากลิ่นจริงในอีก 30 นาที
now = pd.Series(odor[points], index=points)                               # ค่ากลิ่นตอนที่ทำนาย
mae_model = float((pred - actual).abs().mean())
mae_base = float((now - actual).abs().mean())  # baseline: เดาว่าอีก 30 นาทีค่ากลิ่นเท่าตอนนี้

print("\nMAE (ค่าคลาดเคลื่อนเฉลี่ย ยิ่งน้อยยิ่งดี):")
print(f"  chronos-2            {mae_model:.4f}")
print(f"  baseline_no_change   {mae_base:.4f}")
print("  →", "Chronos-2 แม่นกว่า baseline" if mae_model < mae_base else "Chronos-2 ยังไม่ชนะ baseline (รายงานตามจริง)")

# เกณฑ์แปลงค่าที่ทำนาย → ระดับพัดลม/สถานะ (จากการกระจายของค่ากลิ่นจริงที่เก็บมา)
th_mid = float(m["odor"].quantile(0.70))
th_high = float(m["odor"].quantile(0.90))
print(f"เกณฑ์: soon >= {th_mid:.3f}, now >= {th_high:.3f}")

os.makedirs(os.path.join(HERE, "models"), exist_ok=True)
with open(os.path.join(HERE, "models", "config.json"), "w", encoding="utf-8") as f:
    json.dump({
        "name": MODEL_ID, "horizon_min": HORIZON_MIN,
        "mae": mae_model, "mae_baseline": mae_base, "test_points": len(points),
        "data_hours": round(len(m) / 60, 1), "th_mid": th_mid, "th_high": th_high,
    }, f, indent=2)

# กราฟสำหรับรายงาน
t = m.index[[p + HORIZON_MIN for p in points]]
fig, ax = plt.subplots(figsize=(10, 4))
ax.plot(t, actual.values, label="actual", linewidth=1)
ax.plot(t, pred.values, label="predicted (Chronos-2)", linewidth=1)
ax.axhline(th_mid, linestyle="--", linewidth=0.8)
ax.axhline(th_high, linestyle="--", linewidth=0.8)
ax.set_ylabel("odor (R0/Rs)")
ax.set_title(f"Odor 30 min ahead — MAE {mae_model:.3f} (baseline {mae_base:.3f})")
ax.legend()
fig.tight_layout()
fig.savefig(os.path.join(HERE, "models", "prediction.png"), dpi=150)
print("\nบันทึก models/config.json และ models/prediction.png แล้ว")
