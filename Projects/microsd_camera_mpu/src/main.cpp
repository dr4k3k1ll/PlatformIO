#include <Arduino.h>
//LIBRERIE PER IL FUNZIONAMENTO DEL WEBSERVER
#include <WiFi.h>
#include "SPIFFS.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
//LIBRERIE FOTOCAMERA
#define CAMERA_MODEL_AI_THINKER
#include "esp_camera.h"
#include "camera_pins.h"
//LIBRERIE MPU6050
#include <Adafruit_GFX.h>     // accellereometro
#include <Adafruit_MPU6050.h> // accellereometro
#include <Adafruit_Sensor.h>  // accellerometro
#include <Wire.h> //I2C
//LIBRERIE MICROSD
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>            // legge e scrive  flash memory
#include <SPI.h>
#include <SD.h>
//DEFINIZIONI
#define I2C_SCL 15 //SCL
#define I2C_SDA 13 //SDA
#define EEPROM_SIZE 1 // Definisce la dimensione della memoria EEPROM (1 byte)
/*#define SD_CS 13  // Chip Select (CS) per la scheda SD
#define SPI_MOSI 23  // MOSI pin
#define SPI_MISO 19  // MISO pin
#define SPI_SCLK 18  // SCLK (clock) pin*/


//VARIABILI 
int pictureNumber = 0;
sensors_event_t a, g, temp;

//FUNZIONI DI CONFIGURAZIONE MPU6050 ( ACCELLEROMETRO)
Adafruit_MPU6050 mpu;//classe sensore

//Scanner I2C
void I2CScanner(){
byte error, address;
  int nDevices;
  Serial.println("Scansione...");
  nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print(" E'stato trovato un dispositivo I2C connesso all' indirizzo: 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.println(address,HEX);
      nDevices++;
    }
    else if (error==4) {
      Serial.print("Errore sconosciuto all indirizzo 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0) {
    Serial.println(" Non è stato trovato alcun dipositivo I2C collegato.\n");
  }
  else {
    Serial.println("Connesso");
  }
  delay(5000);
}

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
  String path = "/images/picture" + String(pictureNumber) + ".jpg";

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
  pictureNumber++;
  Serial.printf("PSRAM disponibile: %d bytes\n", ESP.getFreePsram());// Stampa la memoria PSRAM libera
}

//FUNZIONI MICROSD
void enableSD() {
  // Riavvia la comunicazione con la scheda SD
  if (!SD_MMC.begin()) {
    Serial.println("Errore durante la riattivazione della scheda SD.");
  } else {
    Serial.println("Scheda SD riattivata con successo.");
  }
}

void disableSD() {
  // Termina la comunicazione con la scheda SD
  SD_MMC.end();
  Serial.println("Scheda SD disabilitata.");
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

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  // Mostra le informazioni della scheda SD
  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  // Apri la directory principale (root) e mostra il contenuto
  File root = SD_MMC.open("/");
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("DIR: ");
      Serial.println(file.name());
    } else {
      Serial.print("FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void deleteAllFiles() {// Funzione per cancellare tutti i file dalla SD
  if (pictureNumber >= 100)
  {
  pictureNumber = 0;
  File root = SD_MMC.open("/images");// Apre la directory principale della SD
  File file = root.openNextFile();// Apre il prossimo file nella directory
  while (file) {
    String fileName = file.path();
    Serial.printf("Cancellazione file: %s\n", fileName.c_str());
    SD_MMC.remove(fileName);// Rimuove il file
    file = root.openNextFile();// Passa al file successivo
  }
  Serial.println("Tutti i file sono stati cancellati");
  }
}

////CONFIGURAZIONE MPU6050////
void StartMPU(){  // avvio sensore

   Wire.begin(I2C_SDA,I2C_SCL); //DEFINISCO L'SDA E SCL
   while (!Serial)
    delay(10); 

  Serial.println(" inizializzazzione Sensore Adafruit MPU6050 ");

  // Try to initialize!
  if (!mpu.begin()) {
    Serial.println("Impossibile trovare MPU6050 ");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 ONLINE!");
}

void SettingsMPU(){  //settaggi sensore

  mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
  Serial.print("Accelerometer range set to: ");
  switch (mpu.getAccelerometerRange()) {
  case MPU6050_RANGE_2_G:
    Serial.println("+-2G");
    //SensitivityScaleFactor = 16384;
    break;
  case MPU6050_RANGE_4_G:
    Serial.println("+-4G");
    //SensitivityScaleFactor = 8192;
    break;
  case MPU6050_RANGE_8_G:
    Serial.println("+-8G");
    //SensitivityScaleFactor = 4096;
    break;
  case MPU6050_RANGE_16_G:
    Serial.println("+-16G");
    //SensitivityScaleFactor = 2040;
    break;
  }
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Gyro range set to: ");
  switch (mpu.getGyroRange()) {
  case MPU6050_RANGE_250_DEG:
    Serial.println("+- 250 deg/s");
    break;
  case MPU6050_RANGE_500_DEG:
    Serial.println("+- 500 deg/s");
    break;
  case MPU6050_RANGE_1000_DEG:
    Serial.println("+- 1000 deg/s");
    break;
  case MPU6050_RANGE_2000_DEG:
    Serial.println("+- 2000 deg/s");
    break;
  }

  mpu.setFilterBandwidth(MPU6050_BAND_260_HZ);
  Serial.print("Filter bandwidth set to: ");
  switch (mpu.getFilterBandwidth()) {
  case MPU6050_BAND_260_HZ:
    Serial.println("260 Hz");
    break;
  case MPU6050_BAND_184_HZ:
    Serial.println("184 Hz");
    break;
  case MPU6050_BAND_94_HZ:
    Serial.println("94 Hz");
    break;
  case MPU6050_BAND_44_HZ:
    Serial.println("44 Hz");
    break;
  case MPU6050_BAND_21_HZ:
    Serial.println("21 Hz");
    break;
  case MPU6050_BAND_10_HZ:
    Serial.println("10 Hz");
    break;
  case MPU6050_BAND_5_HZ:
    Serial.println("5 Hz");
    break;
  }

  Serial.println("");
  delay(100);
}

void printAvailableData(void) {

  /* Populate the sensor events with the readings*/
  mpu.getEvent(&a, &g, &temp);

  /* Print out acceleration data in a plotter-friendly format */
  Serial.print(a.acceleration.x);
  Serial.print(",");
  Serial.print(a.acceleration.y);
  Serial.print(",");
  Serial.print(a.acceleration.z);
  Serial.println("");
}

void setup(){

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disabilita il rilevatore di brownout (bassa tensione)
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  //inizializzo la fotocamera
  Serial.println("Configurazione Fotocamera in corso...\n");
  setupCamera();
  ControlSD(); //inizializzo la microsd
  Serial.println("Fotocamera Configurata Correttamente! \n");
   // Configura l'EEPROM
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0) + 1; // Legge l'ultimo numero di foto dalla EEPROM e lo incrementa
  takePicture();
 
}

void loop(){
  disableSD();
  takePicture();
  I2CScanner();
  StartMPU();
  SettingsMPU();
  printAvailableData();
  Wire.end();//disattivo I2C
  delay(50);
  ControlSD;
  

}