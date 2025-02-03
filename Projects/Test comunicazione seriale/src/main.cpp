#include <Arduino.h>
/*
struct mydata{
  int AccelerazioneX;
  int AccelerazioneY;
  int AccelerazioneZ;
  int PBatteria;
};

int i = 0;
byte buff[100];
mydata dato;


void setup() {
  Serial.begin(9600); //Inizializziamo la comunicazione seriale
}

void loop() {
   while(Serial.available()){
    byte n = Serial.read();
    if ((char)n =='\n'){
      memcpy(&dato,buff,i);
      i = 0;
    }else{
      buff[i] = n;
      i++;
    }
    
   }
}*/
#include <WiFi.h>              // Libreria per la connessione Wi-Fi
#include <ESPAsyncWebServer.h>  // Libreria per il server web asincrono
#include <AsyncTCP.h>           // Gestione asincrona delle connessioni TCP

// Credenziali per l'Access Point
const char* ssidAP = "ESP32_AP";
const char* passwordAP = "123456789";

// IP statico per l'Access Point
IPAddress local_IP(192, 168, 4, 1);  
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// Struttura per i dati ricevuti via seriale
struct mydata {
  int32_t AccelerazioneX;
  int32_t AccelerazioneY;
  int32_t AccelerazioneZ;
  int32_t PBatteria;
  int32_t Comando;
};

mydata dato;  // Variabile globale per memorizzare i dati ricevuti
int i = 0;
byte buff[100]; // Buffer per memorizzare i dati seriali

// Creazione del server web sulla porta 80
AsyncWebServer server(80);

// Funzione per configurare l'ESP32 come Access Point con IP statico
void setupWiFiAsAP() {
    WiFi.softAP(ssidAP, passwordAP);  
    if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
        // Non possiamo usare Serial.print() quindi saltiamo il debug
    }
}

// Pagina web per visualizzare i dati seriali
String getSerialData() {
  String debugInfo = "";
  
  // Aggiungiamo il contenuto del buffer (in formato esadecimale) per capire cosa stiamo ricevendo
  debugInfo += "Buffer Raw Data: ";
  for (int j = 0; j < i; j++) {
    debugInfo += String(buff[j], HEX);  // Stampa ogni byte in esadecimale
    debugInfo += " ";
  }
  debugInfo += "\n";

  // Visualizza i valori letti nella struttura mydata
  debugInfo += "comando: " + String(dato.Comando) + "\n";
  debugInfo += "PBatteria: " + String(dato.PBatteria) + "\n";
  debugInfo += "AccelerazioneX: " + String(dato.AccelerazioneX) + "\n";
  debugInfo += "AccelerazioneY: " + String(dato.AccelerazioneY) + "\n";
  debugInfo += "AccelerazioneZ: " + String(dato.AccelerazioneZ) + "\n";
  
  return debugInfo;
}

int invertiEndianness(int num) {
  return ((num >> 24) & 0xFF) |      // Byte 1
         ((num >> 8) & 0xFF00) |     // Byte 2
         ((num << 8) & 0xFF0000) |   // Byte 3
         ((num << 24) & 0xFF000000); // Byte 4
}

void setup() {
   Serial.begin(9600); //Inizializziamo la comunicazione seriale
  // Configurazione WiFi come Access Point
  setupWiFiAsAP();

  // Configurazione della pagina principale
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", getSerialData());  // Invia i dati seriali come risposta
  });

  // Avvia il server web
  server.begin();
}

void loop() {
  // Lettura dei dati dalla seriale
  while (Serial.available()) {
    byte n = Serial.read();
    
    // Aggiungi il dato al buffer
    buff[i] = n;
    i++;
   // if (n == '\n') {  
    // Se abbiamo ricevuto tutti i 16 byte necessari (4 int da 4 byte ciascuno)
    if (i == sizeof(mydata)) {  // Verifica che i dati siano esattamente 16 byte
      memcpy(&dato, buff, sizeof(mydata));  // Copia il buffer nella struttura

    }
      i = 0;  // Resetta l'indice per il buffer
    //}
  }
}
