#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>            // read and write from flash memory
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// define the number of bytes you want to access
#define EEPROM_SIZE 1
// FUNZIONI DI CONFIGURAZIONE DELLA FOTOCAMERA:
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
         config.frame_size = FRAMESIZE_QVGA;
        config.fb_location = CAMERA_FB_IN_DRAM; // Memorizza i frame buffer in PSRAM
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

int pictureNumber = 0;
 unsigned long lastPictureTime; // Variabile per tenere traccia del tempo dell'ultima foto

void takePicture() { //funzione per scattare la foto
   // Acquisisce un frame dalla fotocamera
  camera_fb_t *fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Errore cattura immagine");
    return;
  }

  // Controlla lo spazio disponibile sulla scheda SD
  uint64_t freeSpace = SD_MMC.totalBytes() - SD_MMC.usedBytes();
  Serial.printf("Spazio libero sulla SD: %llu bytes\n", freeSpace);
// Verifica se lo spazio disponibile è sufficiente per salvare l'immagine (almeno 50 KB)
  if (freeSpace < 50000) {  // Assumendo che ogni immagine sia più piccola di 50 KB
    Serial.println("Spazio insufficiente sulla scheda SD");
    esp_camera_fb_return(fb); // Restituisci il frame buffer se non utilizzi la foto
    return;
  }
    // Crea il percorso del file immagine con il numero corrente della foto
  String path = "/picture" + String(pictureNumber) + ".jpg";

    // Apre il file sulla SD per scrivere l'immagine
  fs::FS &fs = SD_MMC;
  Serial.printf("Nome file immagine: %s\n", path.c_str());
  // Se non riesce ad aprire il file, stampa un errore
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Impossibile aprire il file in modalità scrittura");
  } else {
    // Scrive i dati dell'immagine nel file
    file.write(fb->buf, fb->len); 
    Serial.printf("File salvato nel percorso: %s\n", path.c_str());
    // Salva il numero dell'immagine nell'EEPROM
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();
  }
  file.close();
  esp_camera_fb_return(fb); // Restituisce il frame buffer

  Serial.printf("PSRAM disponibile: %d bytes\n", ESP.getFreePsram());// Stampa la memoria PSRAM libera
}


void deleteAllFiles() {// Funzione per cancellare tutti i file dalla SD
  File root = SD_MMC.open("/");// Apre la directory principale della SD
  File file = root.openNextFile();// Apre il prossimo file nella directory
  while (file) {
    String fileName = file.name();
    Serial.printf("Cancellazione file: %s\n", fileName.c_str());
    SD_MMC.remove(fileName);// Rimuove il file
    file = root.openNextFile();// Passa al file successivo
  }
  Serial.println("Tutti i file sono stati cancellati");
}

void ControlSD(){// Controlla se è stata inserita una scheda SD
  if(!SD_MMC.begin()){ 
      Serial.println("SD Card Mount Failed");
      return;
    }
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
      Serial.println("No SD Card attached");
      return;
    }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disabilita il rilevatore di brownout (bassa tensione)

  Serial.begin(9600);
  setupCamera();
  
  ControlSD();

  // Esegui il primo scatto di prova per configurare tutto
  takePicture(); 

  // Configura l'EEPROM
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0) + 1; // Legge l'ultimo numero di foto dalla EEPROM e lo incrementa

  // Imposta il timer
  lastPictureTime = millis(); 
}


void loop() {
  unsigned long currentTime = millis();
  if (currentTime - lastPictureTime >= 5000) {
    Serial.println("Scattando una foto...");
    takePicture();
    lastPictureTime = currentTime;
  }
  delay(1000); // Ritardo per ridurre il carico sulla CPU
}