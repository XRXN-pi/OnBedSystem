"""สร้าง MQTT client ที่ต่อ NETPIE2020 ด้วยข้อมูลของ device ai-service"""
import paho.mqtt.client as mqtt
from config_secrets import NETPIE_CLIENT_ID, NETPIE_TOKEN, NETPIE_SECRET

HOST = "mqtt.netpie.io"
PORT = 1883
TOPIC_RAW = "@msg/onbed/raw"
TOPIC_CMD = "@msg/onbed/cmd"


def make_client(on_message, subscribe=(TOPIC_RAW,)):
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=NETPIE_CLIENT_ID)
    client.username_pw_set(NETPIE_TOKEN, NETPIE_SECRET)
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    def on_connect(c, userdata, flags, reason_code, properties=None):
        print(f"[MQTT] connected: {reason_code}")
        for t in subscribe:
            c.subscribe(t)

    def on_disconnect(c, userdata, flags, reason_code, properties=None):
        print(f"[MQTT] disconnected: {reason_code} (จะต่อใหม่เอง)")

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.connect(HOST, PORT, keepalive=30)
    return client
