#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>

// OLED
U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 15, 14, U8X8_PIN_NONE);

// RTC
TwoWire I2C_RTC = TwoWire(1);
RTC_DS3231 rtc;

// WiFi
const char* ssid = "AirFiber-kNG33X";
const char* password = "Alchemy201@";
const char* host = "monthly-equivocal-rover.ngrok-free.dev";

// Pins
#define BUTTON_PIN 13
#define GREEN_LED 12
// #define RED_LED 2

bool lastState = HIGH;

// OLED helper
void showOLED(const char* l1, const char* l2 = "", const char* l3 = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 12, l1);
  u8g2.drawStr(0, 28, l2);
  u8g2.drawStr(0, 44, l3);
  u8g2.sendBuffer();
}

// CAMERA SETUP (YOUR FINAL OPTIMIZED)
void setupCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

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

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 8;
  config.fb_count = 1;

  esp_camera_init(&config);

  sensor_t * s = esp_camera_sensor_get();

  s->set_vflip(s, 1);
  s->set_hmirror(s, 0);

  s->set_brightness(s, -1);
  s->set_contrast(s, 3);
  s->set_saturation(s, -2);

  s->set_sharpness(s, 3);
  s->set_denoise(s, 0);

  s->set_gain_ctrl(s, 1);
  s->set_exposure_ctrl(s, 1);
  s->set_aec2(s, 0);
  s->set_ae_level(s, -1);

  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);

  // warmup
  for (int i = 0; i < 5; i++) {
    camera_fb_t* f = esp_camera_fb_get();
    if (f) esp_camera_fb_return(f);
    delay(100);
  }
}

// SETUP
void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  // pinMode(RED_LED, OUTPUT);

  u8g2.begin();
  showOLED("Booting...", "", "");

  I2C_RTC.begin(3, 1, 100000);
  rtc.begin(&I2C_RTC);

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

// LOOP
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
      // digitalWrite(RED_LED, HIGH);
      delay(500);
      // digitalWrite(RED_LED, LOW);
      return;
    }

    showOLED("Sending...", "", "");

    WiFiClientSecure client;
    client.setInsecure();

    if (!client.connect(host, 443)) {
      showOLED("Conn Failed", "", "");
      // digitalWrite(RED_LED, HIGH);
      delay(500);
      // digitalWrite(RED_LED, LOW);
      esp_camera_fb_return(fb);
      return;
    }

    client.println("POST /esp/ HTTP/1.1");
    client.print("Host: ");
    client.println(host);
    client.println("Content-Type: application/octet-stream");
    client.print("Content-Length: ");
    client.println(fb->len);
    client.println("Connection: close");
    client.println();

    uint8_t *buf = fb->buf;
    size_t len = fb->len;

    for (size_t i = 0; i < len; i += 512) {
      size_t chunk = (i + 512 < len) ? 512 : len - i;
      client.write(buf + i, chunk);
    }

    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") break;
    }

    String response = client.readString();

    showOLED("Sent!", "Check Telegram", "");

    digitalWrite(GREEN_LED, HIGH);
    delay(500);
    digitalWrite(GREEN_LED, LOW);

    client.stop();
    esp_camera_fb_return(fb);

    delay(2000);
  }

  lastState = current;
}