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
// DEFINIZIONI
#define G 9.81 //m/s^2
#define N 10
#define M 3
#define I2C_SCL 14 //SCL
#define I2C_SDA 15 //SDA
#define EEPROM_SIZE 1 // Definisce la dimensione della memoria EEPROM (1 byte)

/////VARIABILI/////
//VARIABILI PER IL FUNZIONAMENTO DEL WEBSERVER
int flag = 0;
unsigned long lastResetTime = 0;
unsigned long resetInterval = 10000; 
unsigned long currentTime = millis();
//VARIABILI UTILI PER IL FUNZIONAMENTO DELLA FOTOCAMERA E MICROSD
int pictureNumber = 0; // Variabile per tenere traccia del numero di foto scattate
unsigned long lastPictureTime; // Variabile per tenere traccia del tempo dell'ultima foto
//VARIABILI PER LA MISURAZIONE DELL'ACCELERAZIONE E DELLA VELOCITA'
const float G_INV =1/G;
float Acceleration[N][M];  //campionamento accelerazione
float Axis_Error[N][M]; // errore dovuto alla sensibilità tra gli assi (+/- 2%)
float Total_Error_g[N][M]; //errore totale espresso in g
float StandardDeviation[M] {0,0,0}; //Deviazione Standard
float UA[M] {0,0,0}; //incertezza di tipo A
float U_TOT[M] {0,0,0}; //incertezza Totale
float SensitivityScaleFactor;//LSB/g ( last sensitive bit)
float Error_LSB; //errore di offset espresso in LSB
float Error_g; //errore di offset espresso in g
float InitialCalibrationTolerance = 0.03; // Precisione iniziale calibrazione (+/- 3%)
float CrossAxisSensitivity = 0.02; //sensibilità tra gli assi (+/- 2%)
float ZEROGOutput[2][M]{
  {50e-3,50e-3,80e-3}, //incertezza dovuta alla sensibilità tra gli assi (Tipo B)
  {35e-3,35e-3,60e-3}  //incertezza dovuto alla temperatura (Tipo B)
} ;  //(mg)
float PowerSpectralDensity = 400e-6; // rumore spettrale (µg/√Hz)
float SigmaNoiseDensity = 0;// Incertezza del rumore (Tipo B)
double ris[M] {0,0,0};
double sum[M] {0,0,0};
float A_M [M] {0,0,0}; //Accellerazione media
float A_1_M [M] {0,0,0};//Accellerazione media primo campionamento
float A_2_M [M] {0,0,0};//Accellerazione media secondo campionamemto
float deltaA[M] {0,0,0};//variazione di accelerazione
float v1[M] {0,0,0}; // velocità 1
float v2[M] {0,0,0}; //velocità 2
unsigned long t1 = 0;  // tempo di campionamento A_1_M
unsigned long t2 = 0; // tempo di campionamento A_2_M
unsigned long intervallo = 0; //t2-t1
unsigned long tv1 = 0; // tempo velocità 1
unsigned long tv2 = 0; // tempo velocità 2
unsigned long Vintervallo = 0; // tv2-tv1


// Definizione ID e Password del Wi-Fi 
const char* ssid = MY_WIFI_SSID;
const char* password = MY_WIFI_PASS;

//Istanza del server
AsyncWebServer server(80);
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

////FUNZIONI SERVER////
//Configurazione DNS
void MyDns(){
  if (MDNS.begin("DASHCAM32"))
  {
     Serial.println("\n connesso! \n");
    Serial.println("http://DASHCAM32.local/");
    
  }else{
    Serial.println("ERRORE DNS! \n");
    Serial.println("\n connesso! \n");
    Serial.println("IP: 'http://");
    Serial.println(WiFi.localIP()); 
  }
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

//%STATO_CAMERA%
String processor(const String & par){//mi da indicazione sull' attività o l'inattività della fotocamera dell'esp32_cam
  
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
    SensitivityScaleFactor = 16384;
    break;
  case MPU6050_RANGE_4_G:
    Serial.println("+-4G");
    SensitivityScaleFactor = 8192;
    break;
  case MPU6050_RANGE_8_G:
    Serial.println("+-8G");
    SensitivityScaleFactor = 4096;
    break;
  case MPU6050_RANGE_16_G:
    Serial.println("+-16G");
    SensitivityScaleFactor = 2040;
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

void MeasureError(){// Errori di misura
  Error_LSB = SensitivityScaleFactor * InitialCalibrationTolerance;
  Error_g =  Error_LSB/SensitivityScaleFactor; //errore di offset espresso in g
/*
  Serial.println("\nl'errore di offset del sensore MPU6050. \n");
  Serial.print( Error_g,6);
  Serial.println(" g\n");

  Serial.println("\nl'errore dovuto alla sensibilità tra gli assi sensore MPU6050. \n");*/
 
  for (int i = 0; i < N; i++) // errore dovuto alla sensibilità tra gli assi (+/- 2%)
  {
    Axis_Error[i][0] = (Acceleration[i][0]*G_INV)* CrossAxisSensitivity;
    Axis_Error[i][1] = (Acceleration[i][1]*G_INV)* CrossAxisSensitivity;
    Axis_Error[i][2] = (Acceleration[i][2]*G_INV)* CrossAxisSensitivity;
    /*Serial.print(" X: ");
    Serial.print(Axis_Error[i][0],3);
    Serial.print(", Y: ");
    Serial.print(Axis_Error[i][1],3);
    Serial.print(", Z: ");
    Serial.print(Axis_Error[i][2],3);
    Serial.println(" g");*/
  }

 //Serial.println("\nl'errore totale espresso in g \n"); 

  for (int i = 0; i < N; i++) //errore totale espresso in g
  {
    Total_Error_g[i][0]=sqrt((Axis_Error[i][0] * Axis_Error[i][0]) + (Error_g * Error_g));
    Total_Error_g[i][1]=sqrt((Axis_Error[i][1] * Axis_Error[i][1]) + (Error_g * Error_g));
    Total_Error_g[i][2]=sqrt((Axis_Error[i][2] * Axis_Error[i][2]) + (Error_g * Error_g));
    /*Serial.print(" X: ");
    Serial.print(Total_Error_g[i][0],3);
    Serial.print(", Y: ");
    Serial.print(Total_Error_g[i][1],3);
    Serial.print(", Z: ");
    Serial.print(Total_Error_g[i][2],3);
    Serial.println(" g");*/
  }
}

void TypeAUncertainty(){ // calcolo deviazione standard e incertezza di tipo A
 //Serial.println("l'incertezza di tipo A in X  Y  Z: \n");
 for (int i = 0; i < M; i++)
  {
    sum[i] = 0;
   for (int j = 0; j < N; j++)
   {
    ris[i]=pow(Acceleration[j][i]-A_M[i],2);
    sum[i] = sum[i]+ris[i];
   }
    StandardDeviation[i]= sqrt(sum[i]/9); //deviazione standard
    UA[i] = StandardDeviation[i]/sqrt(10); // incertezza di tipo A
    //Serial.print(UA[i],6);
    //Serial.print("g ");
  }
}

void UncertaintyMeasure(){ //incertezza di misura

  TypeAUncertainty();

  //Serial.println("\nl'incertezza di tipo B inerente al rumore del MPU6050 è. \n");
  SigmaNoiseDensity = PowerSpectralDensity *sqrt(260);
  Serial.print(SigmaNoiseDensity,6);
  //Serial.println(" g\n");

  //Serial.println("\nl'incertezza di tipo B relativa alla sensibilità interasse è per gli assi : \n");
  //Serial.println(" X  Y  Z: \n");
  /* for (int i = 0; i < M; i++)
  {
    Serial.print(ZEROGOutput[0][i],6);
    Serial.print("g ");
  } 
  Serial.println("\nl'incertezza di tipo B relativa alla temperatura : \n");
  Serial.println(" X  Y  Z: \n");
   for (int i = 0; i < M; i++)
  {
    Serial.print(ZEROGOutput[1][i],6);
    Serial.print("g ");
  }*/

  //Serial.println("\nl'incertezza Totale : \n");
  //Serial.println(" X  Y  Z: \n");
  for (int i = 0; i < M; i++)
  {
    U_TOT[i] =sqrt(pow(UA[i],2)+pow(SigmaNoiseDensity,2)+pow(ZEROGOutput[0][i],2)+pow(ZEROGOutput[1][i],2));//incertezza totale espressa in g
    /*Serial.print(U_TOT[i],6);
    Serial.print("g ");
    Serial.println("\nche corrisponde a : \n");
    Serial.print((U_TOT[i]*G),6);
    Serial.print("m/s^2\n ");*/
  }
}

void goToSleepMPU() { // Funzione per mettere l'MPU6050 in modalità sleep
  Wire.beginTransmission(0x68);  // Indirizzo I2C dell'MPU6050
  Wire.write(0x6B);  // Registro PWR_MGMT_1
  Wire.write(0x40);  // Imposta il bit 6 (sleep)
  Wire.endTransmission(true);
  Serial.println("MPU6050 in modalità sleep.");
}

void wakeUpMPU() { // Funzione per risvegliare l'MPU6050 dalla modalità sleep
  Wire.beginTransmission(0x68);  // Indirizzo I2C dell'MPU6050
  Wire.write(0x6B);  // Registro PWR_MGMT_1
  Wire.write(0x00);  // Reset del bit 6 (uscire dal sleep)
  Wire.endTransmission(true);
  Serial.println("MPU6050 risvegliato.");
}

void Measure(){ // misurazione dal sensore

disableSD();
delay(50);
Wire.begin(I2C_SDA,I2C_SCL); //DEFINISCO L'SDA E SCL
I2CScanner(); //controllo se ci sono dispositivi I2C connessi
//inizializzo MPU6050
StartMPU();
SettingsMPU();

sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  for (int i = 0; i < N; i++)
  {
    Acceleration[i][0] = a.acceleration.x;
    Acceleration[i][1] = a.acceleration.y;
    Acceleration[i][2] = a.acceleration.z;
    /*Serial.print("Acceleration X: ");
    Serial.print(Acceleration[i][0], 6);
    Serial.print(", Y: ");
    Serial.print(Acceleration[i][1], 6);
    Serial.print(", Z: ");
    Serial.print(Acceleration[i][2], 6);
    Serial.println(" m/s^2");*/
  }
  Wire.end();//disattivo I2C
  delay(50);
  SPI.end();
  SPI.begin();
  enableSD();

  if (!Wire.begin()) {
    Serial.println("MPU6050 disattivato.");
  }else{
    Serial.println(" impossibile disattivare MPU6050!");
  }
  

  
  //ERRORI DI MISURA:
  MeasureError();

  //Serial.println("Accelerazione Media in X  Y  Z \n");
  for (int i = 0; i < M; i++)
  {
    for (int j = 0; j < N; j++)
    {
      A_M[i] = A_M[i] + Acceleration[j][i];
    }
    A_M[i]= A_M[i]/10;
    //Serial.print(A_M[i],6);
    //Serial.print(" ");
  }
   //Serial.println("m/s^2 ");
   
  // CALCOLI RELATIVI ALL'INCERTEZZA DI A:
  
  UncertaintyMeasure();
  
  // Visualizzazione della misurazione completa (valore medio ± incertezza totale)
    Serial.println("\nAccelerazione Media (X, Y, Z) e incertezza totale:");
    for (int i = 0; i < M; i++) {
        Serial.print("Asse ");
        Serial.print(i == 0 ? "X" : (i == 1 ? "Y" : "Z"));
        Serial.print(": ");
        Serial.print(A_M[i]);
        Serial.print(" m/s^2 ± ");
        Serial.print((U_TOT[i]*G), 2);
        Serial.println(" m/s^2");
    }

}

void measureVelocity(float V[],unsigned long &tv){ // calcola la velocità

  Measure();

  for (int i = 0; i < M; i++){
    A_1_M[i]=A_M[i];
    A_M[i]=0;
  }
    
  while((millis()-t1)<=2000){

      Measure();

      for (int i = 0; i < M; i++)
      {
        A_2_M[i]=A_M[i];
        A_M[i]=0;
      }
      t2 = millis();
      intervallo = t2-t1; 

      //Serial.print("\nLA VELOCITA' IN X Y Z E':\n ");
      for (int i = 0; i < M; i++)
      {
        deltaA[i]=A_2_M[i]-A_1_M[i];
        V[i]= deltaA[i]*(intervallo/1000);
        Serial.println(V[i],6);
        Serial.print(" ");
        deltaA[i] = 0;
      }
      Serial.print(" m/s\n");
      intervallo = 0;
      t1 = millis();
  }
  tv = millis();
}

void setup(){

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disabilita il rilevatore di brownout (bassa tensione)
  //Wire.begin(I2C_SDA,I2C_SCL); //DEFINISCO L'SDA E SCL
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  Serial.println("\nI2C Scanner");
  //I2CScanner(); //controllo se ci sono dispositivi I2C connessi
  //inizializzo MPU6050
  //StartMPU();
  //SettingsMPU();
  //goToSleepMPU();
  //inizializzo la fotocamera
  Serial.println("Configurazione Fotocamera in corso...\n");
  setupCamera();
  ControlSD(); //inizializzo la microsd
  Serial.println("Fotocamera Configurata Correttamente! \n");
   // Configura l'EEPROM
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = EEPROM.read(0) + 1; // Legge l'ultimo numero di foto dalla EEPROM e lo incrementa
  // Imposta il timer
  lastPictureTime = millis(); 
  //Connessione alla rete
  ConnecToWiFi();
  SPIFFSConnected(); //configuro SPIFFS
  // Servo le pagine web tramite SPIFFS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){//pagina principale
    req->send(SPIFFS, "/home.html", String(), false, processor);
  });
  server.on("/streaming", HTTP_GET, [](AsyncWebServerRequest *request){//pagina dello streaming
    request->send(SPIFFS, "/index.html", String(), false);
  });
  server.on("/gallery", HTTP_GET, [](AsyncWebServerRequest *request){ // pagina galleria
    request->send(SPIFFS, "/gallery.html", String(), false);
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

  startCameraStream();// Avvia la funzione di streaming
  server.begin(); // Avvia il server
  Serial.println("il Server è stato avviato correttamente!\n");
  takePicture(); // Esegui il primo scatto di prova per configurare tutto
  delay(1000);
}

void loop(){
//misurazione V1:
measureVelocity(v1, tv1);

 //ogni tot di tempo scatta una foto
  unsigned long currentTime = millis();
  if (currentTime - lastPictureTime >= 5000) {
    Serial.println("Scattando una foto...");
    takePicture();
    lastPictureTime = currentTime;
  }
  delay(10); // Ritardo per ridurre il carico sulla CPU
  
  //resetto il Buffer della fotocamera per evitare l'overflow
  currentTime = millis();
  if (currentTime - lastResetTime >= resetInterval) {
    
    Serial.println("Reset del buffer per evitare overflow");
    esp_camera_deinit();   // Deinizializza la fotocamera
    setupCamera();         // Reinizializza la fotocamera
    lastResetTime = currentTime;
  }
  deleteAllFiles();

delay(10);  

//misurazione V2:
measureVelocity(v2, tv2);
Vintervallo = tv2-tv1;

for (int i = 0; i < M; i++)
{

  deltaA[i] = 0;
  deltaA[i] = (v2[i]-v1[i])/(Vintervallo/1000);
  if (deltaA[i]>=G)
  {
    Serial.println("\n");
    Serial.print(deltaA[i]);
    Serial.println(" m/s^2\n");
    Serial.print("\n SCATTA FOTO!");
    takePicture();
  }else{
    Serial.println("\n");
    Serial.print(deltaA[i]);
    Serial.println(" m/s^2\n");
    Serial.print("\nOK!");
  }
}
delay(10); 
}