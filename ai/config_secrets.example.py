# คัดลอกไฟล์นี้เป็น config_secrets.py แล้วใส่ค่าจริง (config_secrets.py ถูก .gitignore ไว้)
# ห้ามตั้งชื่อไฟล์ว่า secrets.py เพราะชนกับโมดูล secrets ของ Python

# NETPIE device "ai-service" (อยู่ Device Group เดียวกับ gateway)
NETPIE_CLIENT_ID = "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
NETPIE_TOKEN = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
NETPIE_SECRET = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

# Discord webhook URL (ได้จากคนทำ dashboard) หน้าตา https://discord.com/api/webhooks/<ตัวเลข>/<รหัสยาว>
DISCORD_WEBHOOK_URL = ""
