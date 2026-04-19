#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>

// OLED (software I2C)
U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 15, 14, U8X8_PIN_NONE);

// RTC (separate bus)
TwoWire I2C_RTC = TwoWire(1);
RTC_DS3231 rtc;

// WiFi
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASS";
const char* serverUrl = "http://YOUR_PC_IP:8000/esp";

// Pins
#define BUTTON_PIN 13
#define GREEN_LED 12
#define RED_LED 2

bool lastState = HIGH;

// ───────── OLED helper ─────────
void showOLED(const char* l1, const char* l2 = "", const char* l3 = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, l1);
  u8g2.drawStr(0, 28, l2);
  u8g2.drawStr(0, 44, l3);
  u8g2.sendBuffer();
}

// ───────── Camera setup ─────────
void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  esp_camera_init(&config);
}

// ───────── Setup ─────────
void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // OLED init
  u8g2.begin();
  showOLED("Booting...", "", "");

  // RTC init
  I2C_RTC.begin(3, 1, 100000);
  rtc.begin(&I2C_RTC);
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // WiFi
  WiFi.begin(ssid, password);
  showOLED("Connecting WiFi", "", "");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  showOLED("WiFi Connected", "", "");

  setupCamera();
  delay(1000);

  showOLED("Ready", "Press Button", "");
}

// ───────── Loop ─────────
void loop() {
  DateTime now = rtc.now();

  char timebuf[16];
  sprintf(timebuf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  showOLED("WhiteNote Ready", timebuf, "Press Button");

  bool current = digitalRead(BUTTON_PIN);

  if (lastState == HIGH && current == LOW) {

    showOLED("Capturing...", "", "");

    camera_fb_t * fb = esp_camera_fb_get();

    if (!fb) {
      showOLED("Camera Failed", "", "");
      digitalWrite(RED_LED, HIGH);
      delay(500);
      digitalWrite(RED_LED, LOW);
      return;
    }

    showOLED("Sending...", "", "");

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/octet-stream");

    int response = http.POST(fb->buf, fb->len);

    if (response > 0) {
      showOLED("Sent!", "Check Telegram", "");
      digitalWrite(GREEN_LED, HIGH);
      delay(500);
      digitalWrite(GREEN_LED, LOW);
    } else {
      showOLED("Send Failed", "", "");
      digitalWrite(RED_LED, HIGH);
      delay(500);
      digitalWrite(RED_LED, LOW);
    }

    http.end();
    esp_camera_fb_return(fb);

    delay(2000);
  }

  lastState = current;
}