"""เก็บข้อมูลจาก gateway ลง data/raw.csv อย่างเดียว (ใช้ช่วงเก็บข้อมูลก่อนมีโมเดล)

วิธีรัน:  python collect.py      หยุด: Ctrl+C
"""
import json
from collector import append_row, DATA_FILE
from netpie import make_client


def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload)["data"]
    except (ValueError, KeyError):
        print("ข้อความผิดรูปแบบ:", msg.payload[:100])
        return
    row = append_row(data)
    flag = " (buffered)" if row.get("buffered") else ""
    print(f"ts={row.get('ts')} odor={row.get('odor')} people={row.get('wc1_people')} "
          f"temp={row.get('temp')} leak={row.get('leak')}{flag}")


if __name__ == "__main__":
    print(f"บันทึกลง {DATA_FILE}")
    make_client(on_message).loop_forever()
