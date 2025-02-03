// https://github.com/me-no-dev/ESPAsyncWebServer
// https://github.com/me-no-dev/asyncTCP
#include <Arduino.h>
#include "keys.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"

AsyncWebServer server(80);

void ConnecToWiFi(){
   WiFi.begin(MY_WIFI_SSID,MY_WIFI_PASS);

   Serial.println("mi connetto... \n");
   while(WiFi.status()!=WL_CONNECTED){
    delay(500);
    Serial.print(".");
   }
   Serial.println("\n connesso! \n");
   Serial.println("IP: ");
   Serial.println(WiFi.localIP());

   delay(1000);
}

String processor(const String & par){

  //%STATO_LED
  if (par =="STATO_LED")
  {
    if (digitalRead(13))
    {
      return "ON";
    }
    else{
      return "OFF";
    }
    
  }
  
  return String();
}

void setup() {

  pinMode(13,OUTPUT);
  digitalWrite(13,LOW);

  Serial.begin(9600);
  delay(1000);
  Serial.println("\n SPiFFS WEB\n");

  if (!SPIFFS.begin(true))
  {
    Serial.println("\n SPiFFS KO\n");
    while (1);
  }
  
  ConnecToWiFi();

  server.on("/",HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(SPIFFS,"/index.html",String(),false, processor);
  });
  server.on("/index.html",HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(SPIFFS,"/index.html",String(),false, processor);
  });
  server.on("/home.html",HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(SPIFFS,"/home.html",String(),false, processor);
  });
    server.on("/on",HTTP_GET, [](AsyncWebServerRequest *req){
    digitalWrite(13,HIGH);
    req->send(SPIFFS,"/index.html",String(),false, processor);
  });
    server.on("/off",HTTP_GET, [](AsyncWebServerRequest *req){
    digitalWrite(13,LOW);
    req->send(SPIFFS,"/index.html",String(),false, processor);
  });

  server.begin();
}

void loop() {
  
}

