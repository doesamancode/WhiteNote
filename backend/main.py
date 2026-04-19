import asyncio
from fastapi import FastAPI, Request
from fastapi.responses import PlainTextResponse
import io
from PIL import Image
import google.generativeai as genai
from telegram import Bot

# ───── CONFIG ─────
def load_config():
    config = {}
    with open("config.txt") as f:
        for line in f:
            key, value = line.strip().split("=")
            config[key] = value
    return config

cfg = load_config()

app = FastAPI()

genai.configure(api_key=cfg["GEMINI_API_KEY"])
model = genai.GenerativeModel("gemini-3.1-flash-lite-preview")

bot = Bot(token=cfg["TELEGRAM_TOKEN"])
CHAT_ID = cfg["CHAT_ID"]

# ───── ROOT ─────
@app.get("/")
def home():
    return {"status": "backend running"}


# ───── BACKGROUND PROCESSOR ─────
async def process_image(img_bytes):
    try:
        print("PROCESSING STARTED")

        img = Image.open(io.BytesIO(img_bytes)).convert("RGB")

        prompt = """
You are generating a short Telegram message from a whiteboard image.

Rules:
- MAX 8 - 10 lines total
- Keep it VERY SHORT and simple
- No long paragraphs
- No explanations
- No formatting like ** or markdown
- Use plain text only

If it contains tasks:
Return:
TASKS:
- task 1
- task 2

REMINDERS:
- reminder 1
- reminder 2

If it contains study/notes:
Return:

SUMMARY:
1 - 2 lines max

POINTS:
- key point 1
- key point 2
- key point 3

Keep everything concise and easy to read on phone.
"""

        try:
            response = await asyncio.to_thread(
                model.generate_content,
                [prompt, img]
            )
            text = response.text.strip() if response else "No response"

        except Exception as e:
            print("GEMINI ERROR:", e)
            text = "AI unavailable"

        # cleanup
        text = text.replace("*", "")
        if len(text) > 800:
            text = text[:800] + "..."

        try:
            bio = io.BytesIO(img_bytes)
            bio.seek(0)

            await bot.send_photo(
                chat_id=CHAT_ID,
                photo=bio,
                caption=text
            )

            print("TELEGRAM SENT")

        except Exception as e:
            print("TELEGRAM ERROR:", e)

    except Exception as e:
        print("PROCESS ERROR:", e)


# ───── ESP ENDPOINT ─────
@app.post("/esp/")
async def esp_upload(request: Request):
    try:
        print("REQUEST HIT")

        img_bytes = await request.body()
        print("Received bytes:", len(img_bytes))

        if len(img_bytes) < 500:
            return PlainTextResponse("too small", status_code=400)

        # 🔥 KEY CHANGE: run processing in background
        asyncio.create_task(process_image(img_bytes))

        # 🔥 RETURN IMMEDIATELY (no waiting for Gemini)
        return PlainTextResponse("OK", status_code=200)

    except Exception as e:
        print("FATAL ERROR:", e)
        return PlainTextResponse("FAIL", status_code=500)