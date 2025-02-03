#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "SPIFFS.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "keyp.h"
//#include <machine/_default_types.h>
#include <ESPmDNS.h>
//#include "io_pin_remap.h"
//#include <esp_https_server.h>

//Variabili
int flag = 0;
unsigned long lastResetTime = 0;
unsigned long resetInterval = 10000; 
unsigned long currentTime = millis();

// Definizione ID e Password del Wi-Fi 
const char* ssid = MY_WIFI_SSID;
const char* password = MY_WIFI_PASS;

//Istanza del server
AsyncWebServer server(80);

//Configurazione DNS
void MyDns(){
  if (MDNS.begin("DASHCAM32"))
  {
     Serial.println("\n connesso! \n");
    Serial.println("https://DASHCAM32.local/");
    
  }else{
    Serial.println("ERRORE DNS! \n");
    Serial.println("\n connesso! \n");
    Serial.println("IP: 'http://");
    Serial.println(WiFi.localIP()); 
  }
}

// Definisco i pin per la fotocamera per il modello AI THINKER esp32CAM
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

//%STATO_CAMERA%
String processor(const String & par){
  if (par =="STATO_CAMERA")
  {
    if (flag != 1)
    {
      return "ONLINE";
    }
    else{
      return "OFFLINE";
    }
    
  }
  
  return String();
}

// FUNZIONI DI CONFIGURAZIONE DELLA FOTOCAMERA:
void setupCamera() {     // Funzione per configurare la fotocamera
camera_config_t config;
config.ledc_channel = LEDC_CHANNEL_0; // Min: LEDC_CHANNEL_0, Max: LEDC_CHANNEL_15 // Canale LED per il controllo della luminosità dell'illuminazione LED, se usato
config.ledc_timer = LEDC_TIMER_0; // Min: LEDC_TIMER_0, Max: LEDC_TIMER_3 // Timer LED per il controllo della luminosità
config.pin_d0 = Y2_GPIO_NUM;// Pin di connessione dei dati della fotocamera
config.pin_d1 = Y3_GPIO_NUM;
config.pin_d2 = Y4_GPIO_NUM;
config.pin_d3 = Y5_GPIO_NUM;
config.pin_d4 = Y6_GPIO_NUM;
config.pin_d5 = Y7_GPIO_NUM;
config.pin_d6 = Y8_GPIO_NUM;
config.pin_d7 = Y9_GPIO_NUM; 
config.pin_xclk = XCLK_GPIO_NUM;  // Pin di clock esterno per la fotocamera
config.pin_pclk = PCLK_GPIO_NUM; // Pin per il clock di pixel e il segnale VSYNC
config.pin_vsync = VSYNC_GPIO_NUM;
config.pin_href = HREF_GPIO_NUM;  // Pin per il segnale HREF e il bus I2C per SCCB
config.pin_sccb_sda = SIOD_GPIO_NUM; 
config.pin_sccb_scl = SIOC_GPIO_NUM; 
config.pin_pwdn = PWDN_GPIO_NUM; // Pin per alimentazione e reset della fotocamera
config.pin_reset = RESET_GPIO_NUM; 
config.xclk_freq_hz = 20000000; // Min: 10000000, Max: 30000000 (tipicamente 20 MHz per OV2640) // Frequenza del clock esterno in Hz
config.frame_size = FRAMESIZE_QVGA; // Min: FRAMESIZE_96X96, Max: FRAMESIZE_UXGA (per OV2640, le risoluzioni tipiche sono QVGA e VGA) // Risoluzione del frame
config.pixel_format = PIXFORMAT_JPEG; // Min: PIXFORMAT_GRAYSCALE, Max: PIXFORMAT_JPEG (JPEG è comune per streaming) // Formato dei pixel
config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Min: CAMERA_GRAB_LATEST, Max: CAMERA_GRAB_WHEN_EMPTY // Modalità di acquisizione del frame
config.jpeg_quality = 60; // Min: 0 (migliore qualità), Max: 63 (più alta compressione) // Qualità della compressione JPEG
config.fb_count = 2; // Min: 1, Max: 2 (con PSRAM); fino a 3 con PSRAM e risoluzioni più basse // Numero di frame buffer utilizzati

 if (config.pixel_format == PIXFORMAT_JPEG) { // Verifica se il formato dei pixel è JPEG
    if (psramFound()) { // Controlla se PSRAM è presente
        Serial.println("Sto usando PSRAM"); 
        config.jpeg_quality = 63; 
        config.fb_count = 2; 
        config.grab_mode = CAMERA_GRAB_LATEST; // Acquisisce l'ultimo frame disponibile
        config.fb_location = CAMERA_FB_IN_PSRAM; // Memorizza i frame buffer in PSRAM
    } else { // PSRAM non trovato
        Serial.println("Sto usando DRAM"); 
        config.frame_size = FRAMESIZE_96X96; // Riduce la risoluzione a 96X96 (800x600) per risparmiare memoria
        config.fb_location = CAMERA_FB_IN_DRAM; // Memorizza i frame buffer nella DRAM normale
    }
 }

  // Inizializza la fotocamera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Errore nell'inizializzazione della fotocamera");
    return;
  }

  Serial.println("Fotocamera inizializzata correttamente");
}

void startCameraStream() { // Funzione per servire lo stream della fotocamera
 
  server.on("/Camera", HTTP_GET, [](AsyncWebServerRequest *request) {  // Gestore della pagina HTML per visualizzare lo stream
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
        flag = 1;
      if (!fb) {
        Serial.println("Errore nel catturare frame dalla fotocamera");
        return 0;
        flag = 0;
      }

      // Preparare l'header del frame
      size_t headerLen = snprintf((char*)buffer, maxLen,
                                  "--frame\r\n"
                                  "Content-Type: image/jpeg\r\n\r\n");
      if (headerLen + fb->len > maxLen) {
        esp_camera_fb_return(fb);
        Serial.println("Frame troppo grande per il buffer");
        return 0;  // Se il frame è troppo grande per il buffer, 
         flag = 2;
      }

      // Copia l'header e il frame JPEG nel buffer
      memcpy(buffer + headerLen, fb->buf, fb->len);
      size_t totalLen = headerLen + fb->len;

      esp_camera_fb_return(fb);

      // Log per verificare che il frame è stato inviato
      Serial.printf("Frame inviato, dimensione totale: %d byte\n", totalLen);

      delay(20); 

      return totalLen;
    });

    request->send(response);
  });
}

//Setup alla Connessione  Wi-fi
void ConnecToWiFi(){
   WiFi.begin(ssid, password);
   WiFi.setSleep(false); // Funzione che Assicura che il WiFi non vada in modalità sleep

   Serial.println("mi connetto... \n");
   while(WiFi.status()!=WL_CONNECTED){
    delay(500);
    Serial.print(".");
   }
  MyDns();
  Serial.println("\n connesso! \n");
  Serial.print("IP: http://");
  Serial.println(WiFi.localIP()); 

   delay(1000);
}

// Configurazione SPIFFS
void SPIFFSConnected(){
   if (!SPIFFS.begin(true))
  {
    Serial.println("\n SPiFFS KO\n");
    while (1);
  }
}

void setup(){
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  Serial.println("Configurazione Fotocamera in corso...\n");

  setupCamera();
  Serial.println("Fotocamera Configurata Correttamente! \n");

  ConnecToWiFi();
  SPIFFSConnected();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(SPIFFS, "/home.html", String(), false, processor);
  });
  // Serve la pagina index.html tramite SPIFFS
  server.on("/streaming", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false);
  });

  startCameraStream();// Avvia la funzione di streaming

  server.begin(); // Avvia il server

  Serial.println("il Server è stato avviato correttamente!\n");
  
}

void loop(){

  currentTime = millis();

  if (currentTime - lastResetTime >= resetInterval) {
    
    Serial.println("Reset del buffer per evitare overflow");
    esp_camera_deinit();   // Deinizializza la fotocamera
    setupCamera();         // Reinizializza la fotocamera
    lastResetTime = currentTime;
  }
  
  // Mantieni la fotocamera attiva
  delay(1000); 
}