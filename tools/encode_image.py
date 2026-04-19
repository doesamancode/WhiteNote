import base64

with open("test.jpg", "rb") as f:
    print(base64.b64encode(f.read()).decode())