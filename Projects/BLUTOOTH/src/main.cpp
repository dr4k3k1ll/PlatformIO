#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial bt;
String line = "";  

void setup() {
  pinMode(13,OUTPUT);
  digitalWrite(13,LOW);

  Serial.begin(9600);
  delay(1000);
  Serial.println("\n BT terminal\n");

  bt.begin("ESP32_CAM_BT");
}

void loop() {
  /*if (Serial.available())
  {
    bt.write(Serial.read());
  }
  if (bt.available())
  {
    Serial.write(bt.read());
  }*/
 if (bt.available())
 {
  char ch =bt.read();
  if (ch =='\n')
  {
    Serial.print("CMD: ");
    Serial.println(line);
    if (line == "ON")
    {
    Serial.print("Hai acceso il led \n");
    digitalWrite(13,HIGH);
    line = "";
    }else if(line == "OFF"){
    Serial.print("Hai spento il led \n");
    digitalWrite(13,LOW);
    line = "";
    }   
  }else if(ch !='\r'){
    line += ch;
  }
  

 }
 
  
}
