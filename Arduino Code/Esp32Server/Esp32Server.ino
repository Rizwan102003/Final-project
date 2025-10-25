#include <WiFi.h>
#include <HTTPClient.h>
#include "arduino_secrets.h"

const char* ssid = WIFI_USERNAME;
const char* password = WIFI_PASSWORD;

WiFiServer server(ESP32_RECEIVER_PORT);

// Replace with your server URL
const char* processingServerURL = FASTAPI_SERVER_URL; 

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("📥 Incoming image...");
    
    // Read image size first (4 bytes)
    uint32_t imgSize = 0;
    client.readBytes((uint8_t*)&imgSize, sizeof(imgSize));
    Serial.printf("Image size: %u bytes\n", imgSize);

    // Allocate buffer (⚠️ ensure PSRAM or enough heap)
    uint8_t *imgBuffer = (uint8_t*)malloc(imgSize);
    if (!imgBuffer) {
      Serial.println("Memory allocation failed");
      client.stop();
      return;
    }

    // Read the image data
    size_t totalRead = 0;
    while (totalRead < imgSize) {
      if (client.available()) {
        totalRead += client.read(imgBuffer + totalRead, imgSize - totalRead);
      }
    }
    client.stop();
    Serial.println("✅ Image received.");

    // Forward to processing server
    HTTPClient http;
    http.begin(processingServerURL);
    http.addHeader("Content-Type", "image/jpeg");

    int httpResponseCode = http.POST(imgBuffer, imgSize);
    Serial.print("📤 Upload status: ");
    Serial.println(httpResponseCode);

    http.end();
    free(imgBuffer);
  }
}
