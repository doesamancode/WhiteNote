import requests

url = "http://127.0.0.1:8000/esp"

with open("test.jpg", "rb") as f:
    response = requests.post(url, data=f.read())

print(response.text)