"""แปลงข้อมูลดิบ (ทุก 10 วินาที) เป็นข้อมูลรายนาที
ใช้ร่วมกันทั้งตอนวัดผล (evaluate.py) และตอนใช้งานจริง (service.py) เพื่อให้คำนวณเหมือนกันเป๊ะ
"""
import pandas as pd


def to_minutes(df: pd.DataFrame) -> pd.DataFrame:
    """รวมข้อมูลเป็นรายนาที: odor = ค่าสูงสุดของทุก node ที่มี MQ-137 (เฉลี่ยในนาทีนั้น), people = ผลรวมทุก node"""
    df = df.copy()
    df["time"] = pd.to_datetime(df["ts"], unit="s", utc=True).dt.tz_convert("Asia/Bangkok")
    df = df.set_index("time").sort_index()
    df = df[~df.index.duplicated(keep="last")]

    odor_cols = [c for c in df.columns if c.endswith("_odor")]
    people_cols = [c for c in df.columns if c.endswith("_people")]
    odor = df[odor_cols].apply(pd.to_numeric, errors="coerce")
    odor = odor.where(odor >= 0)  # -1 = เซ็นเซอร์หลุด → ไม่นับ
    people = df[people_cols].apply(pd.to_numeric, errors="coerce")

    base = pd.DataFrame({
        "odor": odor.max(axis=1),
        "people": people.sum(axis=1, min_count=1),
        "temp": pd.to_numeric(df.get("temp"), errors="coerce"),
        "hum": pd.to_numeric(df.get("hum"), errors="coerce"),
    }, index=df.index)

    m = pd.DataFrame({
        "odor": base["odor"].resample("1min").mean(),
        "people": base["people"].resample("1min").sum(min_count=1),  # นาทีที่ไม่มีข้อมูล = NaN ไม่ใช่ 0
        "temp": base["temp"].resample("1min").mean(),
        "hum": base["hum"].resample("1min").mean(),
    })
    return m
