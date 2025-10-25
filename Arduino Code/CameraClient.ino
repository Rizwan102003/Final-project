#include "esp_camera.h"
#include <WiFi.h>

// ===================
// Camera model
// ===================
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"
#include "arduino_secrets.h"

// ===========================
// WiFi credentials
// ===========================
const char* ssid = WIFI_USERNAME;
const char* password = WIFI_PASSWORD;

// ===========================
// TCP Receiver ESP32 IP & Port
// ===========================
const char* receiverIP = ESP32_RECEIVER_IP;   // ← Change this to your ESP32 receiver IP
const uint16_t receiverPort = ESP32_RECEIVER_PORT;

void startCameraServer();
void setupLedFlash(int pin);

// ===========================
// Helper: Send image over TCP
// ===========================
void sendImageOverTCP(camera_fb_t * fb) {
  WiFiClient client;
  if (client.connect(receiverIP, receiverPort)) {
    Serial.println("[TCP] Connected to receiver");

    // Send the image size first (4 bytes)
    uint32_t imgSize = fb->len;
    client.write((uint8_t*)&imgSize, sizeof(imgSize));

    // Send the image data
    size_t sent = 0;
    while (sent < fb->len) {
      size_t chunkSize = client.write(fb->buf + sent, fb->len - sent);
      if (chunkSize == 0) {
        Serial.println("[TCP] Write failed");
        break;
      }
      sent += chunkSize;
    }

    client.stop();
    Serial.printf("[TCP] Image sent (%u bytes)\n", imgSize);
  } else {
    Serial.println("[TCP] Connection to receiver failed");
  }
}

// ===========================
// Capture and send task
// ===========================
void captureAndSendTask(void *pvParameters) {
  for (;;) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[CAM] Frame capture failed");
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      continue;
    }

    // Send the frame over TCP
    sendImageOverTCP(fb);

    // Return the frame buffer back to the driver
    esp_camera_fb_return(fb);

    // Adjust the delay to control send frequency
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
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
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start the existing web server (untouched)
  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  // Create a FreeRTOS task for capturing and sending images over TCP
  xTaskCreatePinnedToCore(
    captureAndSendTask,    // task function
    "CaptureAndSendTask",  // name
    8192,                  // stack size
    NULL,                  // parameter
    1,                     // priority
    NULL,                  // task handle
    1                      // run on core 1
  );
}

void loop() {
  // Do nothing. Everything else runs in background tasks
  delay(10000);
}
