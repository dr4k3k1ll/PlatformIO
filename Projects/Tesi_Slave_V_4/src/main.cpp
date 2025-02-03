#include <Arduino.h>
//LIBRERIE PER IL FUNZIONAMENTO DEL WEBSERVER
#include <WiFi.h>
#include "SPIFFS.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "keyprova.h"
#include <ESPmDNS.h>
//LIBRERIE FOTOCAMERA
#define CAMERA_MODEL_AI_THINKER
#include "esp_camera.h"
#include "camera_pins.h"
//LIBRERIE MICROSD
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>            // legge e scrive  flash memory


// DEFINIZIONI
#define EEPROM_SIZE 1 // Definisce la dimensione della memoria EEPROM (1 byte)

/////VARIABILI/////
//VARIABILI PER IL FUNZIONAMENTO DEL WEBSERVER
int flag = 0;
String currentPage = "";
String destinationPath = "";
int accelX = 0; 
int accelY = 0;
int accelZ = 0;
char SaccelX[10];
char SaccelY[10];
char SaccelZ[10];
unsigned long lastResetTime = 0;
unsigned long resetInterval = 10000; 
unsigned long currentTime = millis();
//VARIABILI UTILI PER IL FUNZIONAMENTO DELLA FOTOCAMERA E MICROSD
int pictureNumber = 0; // Variabile per tenere traccia del numero di foto scattate
unsigned long lastPictureTime; // Variabile per tenere traccia del tempo dell'ultima foto

// Definizione ID e Password del Wi-Fi 
const char* ssidAP = MY_WIFI_SSID_3;
const char* passwordAP = MY_WIFI_PASS_3;

//Istanza del server
AsyncWebServer server(80);
// Impostazione di IP Statico
IPAddress local_IP(192, 168, 4, 1);  // Cambia questo indirizzo a seconda della tua rete (192.168.4.1 è standard in modalità AP)
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
    

//Funzione per ricevere i comandi da arduino nano
void MasterCommandSlave(){

    if (Serial.available() > 0) {
        int command = Serial.read();  // Leggi il codice comando inviato dal master

        switch (command) {
            case 1:
                delay(10);
                Serial.write(100);  // Risposta ACK con codice TAKE_PICTURE
                break;
            case 2:
                delay(10);
                Serial.write(101);  // Risposta ACK per STOP
                Serial.flush();
                // Dopo aver inviato l'ACK, leggi i dati di accelerazione
                if (Serial.available() >= 3) {  // Verifica che ci siano almeno 3 byte disponibili per i dati di accelerazione
                     accelX = Serial.read(); // Salvo i valori presenti sul terminale di mediaX, Y, Z
                     accelY = Serial.read();
                     accelZ = Serial.read();
                    Serial.flush();
                }
                break;
             case 4:
                delay(10);
                Serial.write(3);
                Serial.flush();  //comando per far partire il DNS sul nano
                break;
            
            default:
                delay(10);
                Serial.write(255);  // Risposta di errore
                break;
        }
    }
    delay(10);  // Breve pausa per evitare loop troppo veloci
}

////FUNZIONI SERVER////
//Setup alla Connessione  Wi-fi
void setupWiFiAsAP() {
    // Impostazione dell'ESP32 come Access Point
    WiFi.softAP(ssidAP, passwordAP);  // Crea una rete Wi-Fi chiamata "DashCam_AP" con password "password123"
    
    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
        Serial.println("Configurazione IP statica fallita");
    }

    IPAddress IP = WiFi.softAPIP();
    Serial.print("Access Point IP address: ");
    Serial.print("IP: http://");
    Serial.println(IP);
}
// Configurazione SPIFFS
void SPIFFSConnected(){
   if (!SPIFFS.begin(true))
  {
    while (1);
  }
}

//%STATO_CAMERA%
String processor(const String & par){//mi da indicazione sull' attività o l'inattività della fotocamera dell'esp32_cam
  
  if (currentPage == "home") {
        if (par == "STATO_CAMERA") {
            return (flag != 1) ? "ONLINE" : "OFFLINE";
        }
    } else if (currentPage == "accelerometer") {
        if (par == "AccX") {
            return (flag != 1) ? String(accelX) : "Errore";
        } else if (par == "AccY") {
            return (flag != 1) ? String(accelY) : "Errore";
        } else if (par == "AccZ") {
            return (flag != 1) ? String(accelZ) : "Errore";
        }
    }
  
  
  return String();
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
        config.jpeg_quality = 63; 
        config.fb_count = 2; 
        config.grab_mode = CAMERA_GRAB_LATEST; // Acquisisce l'ultimo frame disponibile
        config.fb_location = CAMERA_FB_IN_PSRAM; // Memorizza i frame buffer in PSRAM
    } else { // PSRAM non trovato, usa DRAM
        config.frame_size = FRAMESIZE_96X96; // Riduce la risoluzione a 96X96 (800x600) per risparmiare memoria
        config.fb_location = CAMERA_FB_IN_DRAM; // Memorizza i frame buffer nella DRAM normale
    }
 }

  // Inizializza la fotocamera
  if (esp_camera_init(&config) != ESP_OK) {
    return;
  }
}

void takePicture(String destinationPath) { //funzione per scattare la foto
   // Acquisisce un frame dalla fotocamera
  camera_fb_t *fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Errore cattura immagine");
    Serial.flush();
    return;
  }

  // Controlla lo spazio disponibile sulla scheda SD
  uint64_t freeSpace = SD_MMC.totalBytes() - SD_MMC.usedBytes();
  Serial.printf("Spazio libero sulla SD: %llu bytes\n", freeSpace);
  Serial.flush();
// Verifica se lo spazio disponibile è sufficiente per salvare l'immagine (almeno 50 KB)
  if (freeSpace < 50000) {  // Assumendo che ogni immagine sia più piccola di 50 KB
    Serial.println("Spazio insufficiente sulla scheda SD");
    Serial.flush();
    esp_camera_fb_return(fb); // Restituisci il frame buffer se non utilizzi la foto
    return;
  }
    // Crea il percorso del file immagine con il numero corrente della foto
  String path = destinationPath;

    // Apre il file sulla SD per scrivere l'immagine
  fs::FS &fs = SD_MMC;
  Serial.printf("Nome file immagine: %s\n", path.c_str());
  Serial.flush();
  // Se non riesce ad aprire il file, stampa un errore
  File file = fs.open(path.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Impossibile aprire il file in modalità scrittura");
    Serial.flush();
  } else {
    // Scrive i dati dell'immagine nel file
    file.write(fb->buf, fb->len); 
    Serial.printf("File salvato nel percorso: %s\n", path.c_str());
    Serial.flush();
    // Salva il numero dell'immagine nell'EEPROM
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();
  }
  file.close();
  esp_camera_fb_return(fb); // Restituisce il frame buffer
  pictureNumber++;
  Serial.printf("PSRAM disponibile: %d bytes\n", ESP.getFreePsram());// Stampa la memoria PSRAM libera
  Serial.flush();
}

//FUNZIONI MICROSD
void ControlSD(){// Controlla se è stata inserita una scheda SD
  if(!SD_MMC.begin()){ 
      Serial.println("SD Card Mount Failed");
      Serial.flush();
      return;
    }
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
      Serial.println("No SD Card attached");
      Serial.flush();
      return;
    }

  Serial.print("SD Card Type: ");
  Serial.flush();
  if(cardType == CARD_MMC){
    Serial.println("MMC");
    Serial.flush();
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
    Serial.flush();
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
    Serial.flush();
  } else {
    Serial.println("UNKNOWN");
    Serial.flush();
  }
  // Mostra le informazioni della scheda SD
  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  Serial.flush();

  // Apri la directory principale (root) e mostra il contenuto
  File root = SD_MMC.open("/");
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("DIR: ");
      Serial.flush();
      Serial.println(file.name());
      Serial.flush();
    } else {
      Serial.print("FILE: ");
      Serial.flush();
      Serial.print(file.name());
      Serial.flush();
      Serial.print("\tSIZE: ");
      Serial.flush();
      Serial.println(file.size());
      Serial.flush();
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
    Serial.flush();
    SD_MMC.remove(fileName);// Rimuove il file
    file = root.openNextFile();// Passa al file successivo
  }
  Serial.println("Tutti i file sono stati cancellati");
  Serial.flush();
  }
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disabilita il rilevatore di brownout (bassa tensione)
    Serial.begin(9600);  // Comunicazione seriale con Arduino Nano
    delay(1000); // Breve ritardo per stabilizzare la connessione
    setupCamera();
    ControlSD(); //inizializzo la microsd
    // Configura l'EEPROM
    EEPROM.begin(EEPROM_SIZE);
    pictureNumber = EEPROM.read(0) + 1; // Legge l'ultimo numero di foto dalla EEPROM e lo incrementa
    // Imposta il timer
    lastPictureTime = millis(); 
    //Connessione alla rete
    setupWiFiAsAP();
    SPIFFSConnected(); //configuro SPIFFS
    // Servo le pagine web tramite SPIFFS
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){//pagina principale
        currentPage = "home";
        req->send(SPIFFS, "/home.html", String(), false, processor);
    });
    server.on("/accelerometer", HTTP_GET, [](AsyncWebServerRequest *req){//pagina che visualizza le accelerazioni
        currentPage = "accelerometer";
        req->send(SPIFFS, "/accelerometer.html", String(), false, processor);
    });
    server.on("/gallery", HTTP_GET, [](AsyncWebServerRequest *request){ // pagina galleria
    request->send(SPIFFS, "/gallery.html", String(), false);
    });
    server.on("/pictures", HTTP_GET, [](AsyncWebServerRequest *request){ // pagina galleria
    destinationPath = "/images/picture" + String(pictureNumber) + ".jpg";
    request->send(SPIFFS, "/home.html", String(), false);
    takePicture(destinationPath);
    });
    server.on("/list-images", HTTP_GET, [](AsyncWebServerRequest *request) {// Serve la lista delle immagini in un vettore json
        String json = "[";
        File root = SD_MMC.open("/images");
        File file = root.openNextFile();
        bool firstItem = true;
        while (file) {
        if (!file.isDirectory() && String(file.name()).endsWith(".jpg")) {
            if (!firstItem) {
            json += ",";
            }
            json += "\"" + String(file.name()) + "\"";
            firstItem = false;
        }
        file = root.openNextFile();
        }
        json += "]";
        request->send(200, "application/json", json);
    });
    server.on("/images", HTTP_GET, [](AsyncWebServerRequest *request) { // Serve una specifica immagine in base al nome
        String fileName = request->getParam("name")->value();
        String path = "/images/" + fileName;
        request->send(SD_MMC, path, "image/jpeg");
    });
    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) { // scarica una specifica immagine
        String fileName = request->getParam("name")->value();
        String path = "/images/" + fileName;
        request->send(SD_MMC, path, "application/octet-stream", true);
    });
    server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request) { // elimina una specifica immagine
        String fileName = request->getParam("name")->value();
        String path = "/images/" + fileName;
        SD_MMC.remove(path);
        pictureNumber--;
        request->send(200, "application/json", "{\"response\": \"ok\"}" );
    });
    server.begin(); // Avvia il server
    destinationPath = "/images/picture" + String(pictureNumber) + ".jpg";
    takePicture(destinationPath); // Esegui il primo scatto di prova per configurare tutto
    delay(1000);

}


void loop() {
    MasterCommandSlave();//riceve i comandi dal master
     //ogni tot di tempo scatta una foto
  unsigned long currentTime = millis();
  if (currentTime - lastPictureTime >= 20000) {
    Serial.println("Scattando una foto...");
    Serial.flush();
    destinationPath = "/images/picture" + String(pictureNumber) + ".jpg";
    takePicture(destinationPath);
    lastPictureTime = currentTime;
  }
  delay(10); // Ritardo per ridurre il carico sulla CPU
  
  //resetto il Buffer della fotocamera per evitare l'overflow
  currentTime = millis();
  if (currentTime - lastResetTime >= resetInterval) {
    
    Serial.println("Reset del buffer per evitare overflow");
    Serial.flush();
    esp_camera_deinit();   // Deinizializza la fotocamera
    setupCamera();         // Reinizializza la fotocamera
    lastResetTime = currentTime;
  }
  deleteAllFiles();

delay(10);
}