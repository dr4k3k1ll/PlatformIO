#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "SPIFFS.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "keyp.h"
#include <machine/_default_types.h>

// Definisci le credenziali WiFi
const char* ssid = MY_WIFI_SSID;
const char* password = MY_WIFI_PASS;

// Crea un'istanza del server sulla porta 80
AsyncWebServer server(80);

// Definisci i pin per la fotocamera (modello AI THINKER)
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// Funzione per configurare la fotocamera
void setupCamera() {
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
  config.frame_size = FRAMESIZE_QVGA; // Risoluzione 320x240
  config.jpeg_quality = 60; // Qualità JPEG (0-63), più basso è migliore
  config.fb_count = 2; // Usa più frame buffer se PSRAM è disponibile

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      Serial.println("Sto usando PSRAM");
      config.jpeg_quality = 63;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
      config.fb_location = CAMERA_FB_IN_PSRAM;// Usa PSRAM per i frame buffer
    } else {
      // Limit the frame size when PSRAM is not available
      Serial.println("Sto usando DRAM");
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM; // Usa la RAM normale se PSRAM non è disponibile
    }
  } 

  // Inizializza la fotocamera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Errore nell'inizializzazione della fotocamera");
    return;
  }

  Serial.println("Fotocamera inizializzata correttamente");
}

// Funzione per servire lo stream della fotocamera
void startCameraStream() {
  // Gestore della pagina HTML per visualizzare lo stream
  server.on("/camera", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", R"rawliteral(
      <!DOCTYPE html>
      <html lang="en">
      <head>
          <meta charset="UTF-8">
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <title>ESP32 Camera Stream</title>
      </head>
      <body>
          <h1>ESP32 CAM Streaming</h1>
          <img src="/stream" style="width: 100%;" />
      </body>
      </html>)rawliteral");
  });

  // Gestore dello stream video
  server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginChunkedResponse("multipart/x-mixed-replace; boundary=frame", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      camera_fb_t *fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Errore nel catturare frame dalla fotocamera");
        return 0;
      }

      // Preparare l'header del frame
      size_t headerLen = snprintf((char*)buffer, maxLen,
                                  "--frame\r\n"
                                  "Content-Type: image/jpeg\r\n\r\n");
      if (headerLen + fb->len > maxLen) {
        esp_camera_fb_return(fb);
        Serial.println("Frame troppo grande per il buffer");
        return 0;  // Se il frame è troppo grande per il buffer, ritorna 0
      }

      // Copia l'header e il frame JPEG nel buffer
      memcpy(buffer + headerLen, fb->buf, fb->len);
      size_t totalLen = headerLen + fb->len;

      esp_camera_fb_return(fb);

      // Log per verificare che il frame è stato inviato
      Serial.printf("Frame inviato, dimensione totale: %d byte\n", totalLen);

      delay(20); // Riduce il carico sulla CPU

      return totalLen;
    });

    request->send(response);
  });
}

void setup() {
  Serial.begin(115200);

  // Connetti al WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connessione WiFi in corso...");
  }
  Serial.println("Connesso al WiFi");
  Serial.println(WiFi.localIP());

  // Inizializza SPIFFS per servire la pagina web
  if (!SPIFFS.begin(true)) {
    Serial.println("Errore nell'inizializzazione di SPIFFS");
    return;
  }

  // Inizializza la fotocamera
  setupCamera();

  // Serve la pagina index.html tramite SPIFFS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false);
  });

  // Avvia lo streaming della fotocamera
  startCameraStream();

  // Avvia il server
  server.begin();
}


unsigned long lastResetTime = 0;
unsigned long resetInterval = 10000; // 60 secondi

void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastResetTime >= resetInterval) {
    Serial.println("Reset del buffer per evitare overflow");
    esp_camera_deinit();   // Deinizializza la fotocamera
    setupCamera();         // Reinizializza la fotocamera
    lastResetTime = currentTime;
  }
  
  // Mantieni la fotocamera attiva
  delay(1000); // Aumenta il tempo di ritardo per ridurre la frequenza dei reset del buffer
}
