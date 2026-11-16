"""ส่งแจ้งเตือนเข้า Discord (ผ่าน webhook) จาก Python พร้อมกันแจ้งเตือนรัว (cooldown)
เขียนโดยคนทำ dashboard ใช้โดย service.py
"""
import time
import requests
from config_secrets import DISCORD_WEBHOOK_URL

ROOM_NAME = "ห้องน้ำทดสอบ"
DEFAULT_COOLDOWN_SEC = 15 * 60


class Notifier:
    def __init__(self, cooldown_sec: int = DEFAULT_COOLDOWN_SEC):
        self.cooldown = cooldown_sec
        self.last_sent = {}  # เรื่อง → เวลาที่ส่งล่าสุด

    def send(self, key: str, text: str, force: bool = False) -> bool:
        """key = ชื่อเรื่อง เช่น "clean" เรื่องเดียวกันจะไม่ส่งซ้ำภายในเวลา cooldown"""
        now = time.time()
        if not force and now - self.last_sent.get(key, 0) < self.cooldown:
            return False
        if not DISCORD_WEBHOOK_URL:
            print(f"[ALERT ไม่ได้ส่ง ยังไม่มี webhook URL] {text}")
            return False
        try:
            r = requests.post(DISCORD_WEBHOOK_URL, json={"content": text}, timeout=10)
            ok = r.status_code in (200, 204)  # Discord ตอบ 204 เมื่อส่งสำเร็จ
            if not ok:
                print("[DISCORD] error", r.status_code, r.text[:200])
        except requests.RequestException as e:
            print("[DISCORD] error", e)
            ok = False
        if ok:
            self.last_sent[key] = now
        return ok


# ---------- ถ้อยคำแจ้งเตือน ----------
def hhmm() -> str:
    return time.strftime("%H:%M")


def msg_clean_now(people_60m: int) -> str:
    return (f"🧹 {ROOM_NAME}: ควรทำความสะอาดตอนนี้\n"
            f"มีผู้ใช้ {people_60m} ครั้งในชั่วโมงที่ผ่านมา และกลิ่นกำลังเพิ่มขึ้น ({hhmm()})")


def msg_clean_soon() -> str:
    return f"🕒 {ROOM_NAME}: น่าจะต้องทำความสะอาดภายใน 30 นาที ({hhmm()})"


def msg_gateway_offline(minutes: int) -> str:
    return f"🔴 {ROOM_NAME}: ระบบขาดการเชื่อมต่อมา {minutes} นาที กรุณาตรวจสอบอุปกรณ์"


def msg_gateway_back() -> str:
    return f"🟢 {ROOM_NAME}: ระบบกลับมาออนไลน์แล้ว ({hhmm()})"


def msg_node_offline(node: str) -> str:
    return f"🟠 {ROOM_NAME}: เซ็นเซอร์ {node.upper()} ไม่ส่งข้อมูล กรุณาตรวจสอบปลั๊กไฟ"


def msg_sensor_fault(node: str) -> str:
    return f"🟠 {ROOM_NAME}: เซ็นเซอร์กลิ่นของ {node.upper()} อ่านค่าไม่ได้ (สายอาจหลุด)"


if __name__ == "__main__":
    # ทดสอบ: python notifier.py
    n = Notifier()
    print("ส่งทดสอบ:", n.send("test", f"ทดสอบแจ้งเตือนจาก {ROOM_NAME} ({hhmm()})"))
