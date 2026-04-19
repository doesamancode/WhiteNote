#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// WIFI
const char* ssid = "AirFiber-kNG33X";
const char* password = "Alchemy201@";

// ngrok host
const char* host = "monthly-equivocal-rover.ngrok-free.dev";

// CAMERA PINS (AI Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void startCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 8;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_DRAM;

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("CAM FAIL: 0x%x\n", err);
    return;
  }

  Serial.println("CAM OK");

  sensor_t * s = esp_camera_sensor_get();
  
  s->set_vflip(s, 1);
  s->set_hmirror(s, 0);
  
  // TEXT OPTIMIZATION
  s->set_brightness(s, -1);
  s->set_contrast(s, 3);
  s->set_saturation(s, -2);
  
  // EDGE SHARPNESS
  s->set_sharpness(s, 3);
  s->set_denoise(s, 0);
  
  // EXPOSURE CONTROL
  s->set_gain_ctrl(s, 1);
  s->set_exposure_ctrl(s, 1);
  s->set_aec2(s, 0);
  s->set_ae_level(s, -1);
  
  // COLOR STABILITY
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_wb_mode(s, 0);

  // Warmup frames
  for (int i = 0; i < 5; i++) {
    camera_fb_t* f = esp_camera_fb_get();
    if (f) esp_camera_fb_return(f);
    delay(100);
  }
}

// Stable capture
camera_fb_t* captureStable() {
  for (int i = 0; i < 5; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb && fb->len > 2000) return fb;

    if (fb) esp_camera_fb_return(fb);
    delay(200);
  }
  return NULL;
}

// 🔥 DOUBLE CAPTURE (FINAL VERSION)
camera_fb_t* captureBest() {
  camera_fb_t* fb1 = captureStable();
  if (!fb1) return NULL;

  delay(100); // slight settle

  camera_fb_t* fb2 = esp_camera_fb_get();

  if (fb2 && fb2->len > fb1->len) {
    esp_camera_fb_return(fb1);
    return fb2;
  }

  if (fb2) esp_camera_fb_return(fb2);
  return fb1;
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("START");

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi OK");
  WiFi.setSleep(false);

  startCamera();

  delay(1000);

  // 🔥 USE BEST CAPTURE
  camera_fb_t * fb = captureBest();

  if (!fb) {
    Serial.println("CAPTURE FAIL");
    return;
  }

  Serial.print("IMG SIZE: ");
  Serial.println(fb->len);

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(host, 443)) {
    Serial.println("Connection failed");
    esp_camera_fb_return(fb);
    return;
  }

  Serial.println("Connected to server");

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

  Serial.println("Image sent");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = client.readString();
  Serial.println("Server response:");
  Serial.println(response);

  client.stop();
  esp_camera_fb_return(fb);
}

void loop() {}