"""ทำนายค่ากลิ่นล่วงหน้าด้วย Chronos-2 (โมเดลพยากรณ์ time series สำเร็จรูปของ Amazon, ไม่ต้องเทรนเอง)
ใช้ร่วมกันโดย evaluate.py (วัดความแม่น) และ service.py (ใช้งานจริง)

ครั้งแรกที่รัน โปรแกรมจะดาวน์โหลดโมเดลจาก Hugging Face (ประมาณ 500 MB) เก็บไว้ในเครื่อง ครั้งต่อไปไม่ต้องโหลดอีก
"""
import pandas as pd

MODEL_ID = "amazon/chronos-2"
HORIZON_MIN = 30       # ทำนายล่วงหน้า 30 นาที
CONTEXT_MIN = 6 * 60   # ป้อนประวัติย้อนหลังสูงสุด 6 ชั่วโมง
MIN_CONTEXT_MIN = 60   # ต้องมีประวัติอย่างน้อย 60 นาทีถึงจะทำนาย
COVARIATES = ["people", "temp", "hum"]  # ข้อมูลประกอบที่ป้อนให้โมเดลพร้อมค่ากลิ่น


def load_pipeline():
    """โหลดโมเดล (ช้าเฉพาะครั้งแรก) รันบน CPU ได้ ไม่ต้องมีการ์ดจอ"""
    from chronos import Chronos2Pipeline
    return Chronos2Pipeline.from_pretrained(MODEL_ID, device_map="cpu")


def _clean(m: pd.DataFrame) -> pd.DataFrame:
    """เติมช่องว่างสั้นๆ ให้ข้อมูลรายนาทีต่อเนื่อง (โมเดลต้องการเวลาที่เรียงสม่ำเสมอ)"""
    m = m.copy()
    m["odor"] = m["odor"].interpolate(limit=10).ffill().bfill()
    m["people"] = m["people"].fillna(0)
    for c in ("temp", "hum"):
        m[c] = m[c].ffill().bfill().fillna(0)
    return m


def _window(m: pd.DataFrame, end_pos: int, item_id: str) -> pd.DataFrame:
    """ตัดประวัติที่จบ ณ แถว end_pos (รวมแถวนั้น) ย้อนหลังไม่เกิน CONTEXT_MIN นาที"""
    w = m.iloc[max(0, end_pos + 1 - CONTEXT_MIN): end_pos + 1]
    out = w[["odor"] + COVARIATES].copy()
    out.insert(0, "timestamp", w.index.tz_localize(None) if w.index.tz is not None else w.index)
    out.insert(0, "item_id", item_id)
    return out.reset_index(drop=True)


def forecast_at(pipeline, m: pd.DataFrame, end_positions) -> pd.Series:
    """ทำนายค่ากลิ่นอีก HORIZON_MIN นาที ณ หลายจุดเวลาในครั้งเดียว
    m = ข้อมูลรายนาทีจาก features.to_minutes()  คืน Series: ตำแหน่งแถว → ค่าที่ทำนาย"""
    m = _clean(m)
    frames = [_window(m, int(p), f"w{int(p)}") for p in end_positions]
    ctx = pd.concat(frames, ignore_index=True)
    pred = pipeline.predict_df(
        ctx, target="odor", prediction_length=HORIZON_MIN, quantile_levels=[0.5], freq="min",
    )
    last = pred.groupby("item_id", sort=False).tail(1)  # แถวสุดท้ายของแต่ละจุด = อีก 30 นาที
    return pd.Series(last["0.5"].to_numpy(), index=[int(i[1:]) for i in last["item_id"]]).sort_index()


def forecast_latest(pipeline, m: pd.DataFrame):
    """ทำนายจากข้อมูลล่าสุด คืน None ถ้าประวัติยังไม่ถึง MIN_CONTEXT_MIN นาที"""
    if len(m) < MIN_CONTEXT_MIN or m["odor"].tail(MIN_CONTEXT_MIN).notna().sum() < MIN_CONTEXT_MIN // 2:
        return None
    return float(forecast_at(pipeline, m, [len(m) - 1]).iloc[0])
