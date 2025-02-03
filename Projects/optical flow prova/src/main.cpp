#include <Arduino.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#define CAMERA_MODEL_AI_THINKER
#include "esp_camera.h"
#include "camera_pins.h"
#include <WiFi.h>
#include <HTTPClient.h>



// Risoluzione ridotta per non sovraccaricare l'ESP32-CAM
#define CAMERA_RESOLUTION FRAMESIZE_QQVGA

// Variabili per l'elaborazione delle immagini
camera_fb_t *previousFrame = NULL;
camera_fb_t *currentFrame = NULL;
unsigned long previousMillis = 0;
const long interval = 500; // Intervallo tra acquisizioni (in millisecondi)

// Funzione per inizializzare la fotocamera
void setupCamera() { // Funzione per configurare la fotocamera
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

// Funzione per confrontare due fotogrammi e calcolare la variazione
int compareFrames(camera_fb_t *frame1, camera_fb_t *frame2) {
  int changes = 0;
  for (size_t i = 0; i < frame1->len; i++) {
    if (abs(frame1->buf[i] - frame2->buf[i]) > 15) { // Soglia di differenza pixel
      changes++;
    }
  }
  return changes;
}

// Funzione per salvare il fotogramma sulla microSD
void saveFrameToSD(camera_fb_t *frame, const char *path) {
  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Errore nell'apertura del file sulla SD");
    return;
  }
  file.write(frame->buf, frame->len);
  file.close();
}

void setup() {
  Serial.begin(9600);

  // Inizializza la fotocamera
  setupCamera();

  // Inizializza la microSD
  if (!SD_MMC.begin()) {
    Serial.println("Impossibile montare la microSD");
    return;
  }

  Serial.println("Fotocamera e microSD pronte.");
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Acquisisci un nuovo fotogramma
    currentFrame = esp_camera_fb_get();
    if (!currentFrame) {
      Serial.println("Errore durante l'acquisizione del fotogramma");
      return;
    }

    // Se esiste un fotogramma precedente, confrontalo con il nuovo
    if (previousFrame != NULL) {
      int differences = compareFrames(previousFrame, currentFrame);
      Serial.printf("Differenze rilevate: %d\n", differences);

      // Salva il nuovo fotogramma sulla microSD
      saveFrameToSD(currentFrame, "/images/currentFrame.jpg");

      // Rilascia il vecchio fotogramma
      esp_camera_fb_return(previousFrame);
    }

    // Il fotogramma attuale diventa il fotogramma precedente
    previousFrame = currentFrame;
  }
}

