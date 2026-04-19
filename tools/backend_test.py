from fastapi import FastAPI, File, UploadFile, Request
import io
from PIL import Image
import google.generativeai as genai
from telegram import Bot

# ─────────────────────────────
# Load config

def load_config():
    config = {}
    with open("config.txt") as f:
        for line in f:
            key, value = line.strip().split("=")
            config[key] = value
    return config

cfg = load_config()

# ─────────────────────────────

app = FastAPI()

genai.configure(api_key=cfg["GEMINI_API_KEY"])
model = genai.GenerativeModel("gemini-flash-latest")

bot = Bot(token=cfg["TELEGRAM_TOKEN"])
CHAT_ID = cfg["CHAT_ID"]

# ─────────────────────────────
# TEST ROUTE

@app.get("/")
def home():
    return {"status": "backend running"}

# ─────────────────────────────
# MAIN UPLOAD ROUTE

@app.post("/upload")
async def upload(file: UploadFile = File(...)):
    try:
        img_bytes = await file.read()
        img = Image.open(io.BytesIO(img_bytes))

        prompt = """
Analyze this whiteboard/written text image.

If it contains tasks:
- extract tasks
- create reminders

If it contains notes/study:
- give summary
- give short outline

Keep output SHORT, clean, and Telegram-friendly.
Make sure the output is a suitable summary for whatever the input image is.
"""

        response = model.generate_content([prompt, img])
        text = response.text.strip()

        bio = io.BytesIO(img_bytes)
        bio.seek(0)

        await bot.send_photo(
            chat_id=CHAT_ID,
            photo=bio,
            caption=text
        )

        return {"status": "sent"}

    except Exception as e:
        return {"error": str(e)}
    
@app.post("/esp")
async def esp_upload(request: Request):
    try:
        img_bytes = await request.body()
        img = Image.open(io.BytesIO(img_bytes))

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

        response = model.generate_content([prompt, img])
        text = response.text.strip()

        # clean + limit
        # remove markdown junk
        text = text.replace("**", "")
        text = text.replace("*", "")
        # text = text.replace("\n", " ")
        if len(text) > 800:
            text = text[:800] + "..."

        bio = io.BytesIO(img_bytes)
        bio.seek(0)

        await bot.send_photo(
            chat_id=CHAT_ID,
            photo=bio,
            caption=text
        )
        print("GEMINI OUTPUT:", text)
        return {"status": "sent"}

    except Exception as e:
        return {"error": str(e)}
