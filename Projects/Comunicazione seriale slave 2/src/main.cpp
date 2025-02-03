#include <WiFi.h>
#include <WebServer.h>

// Configurazione del server e dell'Access Point
const char* ssid = "ESP32_AP";
const char* password = "123456789";

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// Struttura dati ricevuti
struct DataPacket {
  byte command;
  int16_t data;
};

// Variabili globali
DataPacket receivedPacket;
bool dataReceived = false;

// Funzione per ricevere i dati dal master
bool receiveData(HardwareSerial& serial, DataPacket& packet) {
  static bool receiving = false;
  static size_t index = 0;
  byte* dataPointer = (byte*)&packet;
  while (serial.available()) {
    byte incomingByte = serial.read();
    
    if (incomingByte == 0x02) {  // Byte di inizio (STX)
      receiving = true;
      index = 0;
    } else if (incomingByte == 0x03 && receiving) {  // Byte di fine (ETX)
      receiving = false;
      if (index == sizeof(DataPacket)) {
        // Invia l'ACK al master
        serial.write(0x06);  // 0x06 è il codice ASCII per l'ACK
        return true;
      }
    } else if (receiving) {
      if (index < sizeof(DataPacket)) {
        dataPointer[index++] = incomingByte;
      }
    }
  }
  return false;
}

// Funzione per generare la pagina web con i dati ricevuti
void handleRoot() {
  String html = "<html><body><h1>ESP32-CAM Slave - Dati Ricevuti</h1><p>";
  html += "Comando ricevuto: " + String(receivedPacket.command, HEX) + "</p>";
  html += "<p>Valore ricevuto: " + String(receivedPacket.data) + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  // Configurazione della rete Wi-Fi come Access Point
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);

  // Configura il server web
  server.on("/", handleRoot);
  server.begin();

  // Inizializza la seriale per comunicare con il master
  Serial.begin(9600);
}

void loop() {
  // Riceve i dati dal master e li memorizza
  if (receiveData(Serial, receivedPacket)) {
    dataReceived = true;
  }

  // Gestisce le richieste della pagina web
  server.handleClient();
}
