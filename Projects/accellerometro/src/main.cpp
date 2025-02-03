#include <Arduino.h>
#include <Adafruit_GFX.h>     // accellereometro
#include <Adafruit_MPU6050.h> // accellereometro
#include <Adafruit_Sensor.h>  // accellerometro
#include <Wire.h> //I2C

// Definizioni
#define G 9.81 //m/s^2
#define I2C_SCL 14 //SCL
#define I2C_SDA 15 //SDA

//Variabili
float Acceleration[10][3];  //campionamento accelerazione
float Axis_Error[10][3]; // errore dovuto alla sensibilità tra gli assi (+/- 2%)
float Total_Error_g[10][3]; //errore totale espresso in g
float StandardDeviation[3] {0,0,0}; //Deviazione Standard
float UA[3] {0,0,0}; //incertezza di tipo A
float U_TOT[3] {0,0,0}; //incertezza Totale
float SensitivityScaleFactor;//LSB/g ( last sensitive bit)
float Error_LSB; //errore di offset espresso in LSB
float Error_g; //errore di offset espresso in g
float InitialCalibrationTolerance = 0.03; // Precisione iniziale calibrazione (+/- 3%)
float CrossAxisSensitivity = 0.02; //sensibilità tra gli assi (+/- 2%)
float ZEROGOutput[2][3]{
  {50e-3,50e-3,80e-3}, //incertezza dovuta alla sensibilità tra gli assi (Tipo B)
  {35e-3,35e-3,60e-3}  //incertezza dovuto alla temperatura (Tipo B)
} ;  //(mg)
float PowerSpectralDensity = 400e-6; // rumore spettrale (µg/√Hz)
float SigmaNoiseDensity = 0;// Incertezza del rumore (Tipo B)
double ris[3] {0,0,0};
double sum[3] {0,0,0};
float A_M [3] {0,0,0}; //Accellerazione media
float A_1_M [3] {0,0,0};//Accellerazione media primo campionamento
float A_2_M [3] {0,0,0};//Accellerazione media secondo campionamemto
float deltaA[3] {0,0,0};//variazione di accelerazione
float v1[3] {0,0,0}; // velocità 1
float v2[3] {0,0,0}; //velocità 2
unsigned long t1 = 0;  // tempo di campionamento A_1_M
unsigned long t2 = 0; // tempo di campionamento A_2_M
unsigned long intervallo = 0; //t2-t1
unsigned long tv1 = 0; // tempo velocità 1
unsigned long tv2 = 0; // tempo velocità 2
unsigned long Vintervallo = 0; // tv2-tv1


/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/
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

//FUNZIONI DI CONFIGURAZIONE MPU6050 ( ACCELLEROMETRO)
Adafruit_MPU6050 mpu;//classe sensore

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

  Serial.println("\nl'errore di offset del sensore MPU6050. \n");
  Serial.print( Error_g,6);
  Serial.println(" g\n");
 

  for (int i = 0; i < 10; i++) // errore dovuto alla sensibilità tra gli assi (+/- 2%)
  {
    Axis_Error[i][0] = (Acceleration[i][0]/G)* CrossAxisSensitivity;
    Axis_Error[i][1] = (Acceleration[i][1]/G)* CrossAxisSensitivity;
    Axis_Error[i][2] = (Acceleration[i][2]/G)* CrossAxisSensitivity;
  }

  Serial.println("\nl'errore dovuto alla sensibilità tra gli assi sensore MPU6050. \n");

  for (int i = 0; i < 10; i++)
  {
    Serial.print(" X: ");
    Serial.print(Axis_Error[i][0],6);
    Serial.print(", Y: ");
    Serial.print(Axis_Error[i][1],6);
    Serial.print(", Z: ");
    Serial.print(Axis_Error[i][2],6);
    Serial.println(" g");
  }
  

  for (int i = 0; i < 10; i++) //errore totale espresso in g
  {
    Total_Error_g[i][0]=sqrt(pow(Axis_Error[i][0],2) + pow(Error_g,2));
    Total_Error_g[i][1]=sqrt(pow(Axis_Error[i][1],2) + pow(Error_g,2));
    Total_Error_g[i][2]=sqrt(pow(Axis_Error[i][2],2) + pow(Error_g,2));
  }

  Serial.println("\nl'errore totale espresso in g \n");

  for (int i = 0; i < 10; i++)
  {
    Serial.print(" X: ");
    Serial.print(Total_Error_g[i][0],6);
    Serial.print(", Y: ");
    Serial.print(Total_Error_g[i][1],6);
    Serial.print(", Z: ");
    Serial.print(Total_Error_g[i][2],6);
    Serial.println(" g");
  }
}

void TypeAUncertainty(){ // calcolo deviazione standard e incertezza di tipo A


 for (int i = 0; i < 3; i++)
  {
    sum[i] = 0;
   for (int j = 0; j < 10; j++)
   {
    ris[i]=pow(Acceleration[j][i]-A_M[i],2);
    sum[i] = sum[i]+ris[i];
   }
    StandardDeviation[i]= sqrt(sum[i]/9); //deviazione standard
    UA[i] = StandardDeviation[i]/sqrt(10); // incertezza di tipo A
  }

  Serial.println("l'incertezza di tipo A in X  Y  Z: \n");

}

void UncertaintyMeasure(){ //incertezza di misura

  TypeAUncertainty();

  for (int i = 0; i < 3; i++)
  {
    Serial.print(UA[i],6);
    Serial.print("g ");
  } 

  Serial.println("\nl'incertezza di tipo B inerente al rumore del MPU6050 è. \n");
 
  SigmaNoiseDensity = PowerSpectralDensity *sqrt(260);
  Serial.print(SigmaNoiseDensity,6);
  Serial.println(" g\n");

  Serial.println("\nl'incertezza di tipo B relativa alla sensibilità interasse è per gli assi : \n");
  Serial.println(" X  Y  Z: \n");

   for (int i = 0; i < 3; i++)
  {
    Serial.print(ZEROGOutput[0][i],6);
    Serial.print("g ");
  } 

  Serial.println("\nl'incertezza di tipo B relativa alla temperatura : \n");
  Serial.println(" X  Y  Z: \n");

   for (int i = 0; i < 3; i++)
  {
    Serial.print(ZEROGOutput[1][i],6);
    Serial.print("g ");
  } 

  Serial.println("\nl'incertezza Totale : \n");
  Serial.println(" X  Y  Z: \n");

  for (int i = 0; i < 3; i++)
  {
    U_TOT[i] =sqrt(pow(UA[i],2)+pow(SigmaNoiseDensity,2)+pow(ZEROGOutput[0][i],2)+pow(ZEROGOutput[1][i],2));//incertezza totale espressa in g
  }
  for (int i = 0; i < 3; i++)
  {
    Serial.print(U_TOT[i],6);
    Serial.print("g ");
  } 

  Serial.println("\nche corrisponde a : \n");

  for (int i = 0; i < 3; i++)
  {
    Serial.print((U_TOT[i]*G),6);
    Serial.print("m/s^2\n ");
  } 
}

void Measure(){ // misurazione dal sensore
sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  for (int i = 0; i < 10; i++)
  {
    Acceleration[i][0] = a.acceleration.x;
    Acceleration[i][1] = a.acceleration.y;
    Acceleration[i][2] = a.acceleration.z;
  }
  
  for (int i = 0; i < 10; i++)
  {
      Serial.print("Acceleration X: ");
      Serial.print(Acceleration[i][0], 6);
      Serial.print(", Y: ");
      Serial.print(Acceleration[i][1], 6);
      Serial.print(", Z: ");
      Serial.print(Acceleration[i][2], 6);
      Serial.println(" m/s^2");
  }

  //ERRORI DI MISURA:

  MeasureError();

  for (int i = 0; i < 3; i++)
  {
    for (int j = 0; j < 10; j++)
    {
      A_M[i] = A_M[i] + Acceleration[j][i];
    }
    A_M[i]= A_M[i]/10;
  }

   Serial.println("Accelerazione Media in X  Y  Z \n");

  for (int i = 0; i < 3; i++)
  {
    Serial.print(A_M[i],6);
    Serial.print(" ");
  } 
    Serial.println("m/s^2 ");

  // CALCOLI RELATIVI ALL'INCERTEZZA DI A:
  
  UncertaintyMeasure();
  
  // Visualizzazione della misurazione completa (valore medio ± incertezza totale)
    Serial.println("\nAccelerazione Media (X, Y, Z) e incertezza totale:");
    for (int i = 0; i < 3; i++) {
        Serial.print("Asse ");
        Serial.print(i == 0 ? "X" : (i == 1 ? "Y" : "Z"));
        Serial.print(": ");
        Serial.print(A_M[i], 6);
        Serial.print(" m/s^2 ± ");
        Serial.print((U_TOT[i]*G), 6);
        Serial.println(" m/s^2");
    }
}

void setup() {
  Wire.begin(I2C_SDA,I2C_SCL);
  Serial.begin(9600);
  Serial.println("\nI2C Scanner");
  I2CScanner(); 

  StartMPU();
  
  SettingsMPU();
  
}
 
void loop() {

//misurazione V1:
Measure();

for (int i = 0; i < 3; i++)
{
  A_1_M[i]=A_M[i];
  A_M[i]=0;
}
t1 = millis();

while((millis()-t1)<=2000){

    Measure();

    for (int i = 0; i < 3; i++)
    {
      A_2_M[i]=A_M[i];
      A_M[i]=0;
    }
    t2 = millis();
    intervallo = t2-t1; 

    Serial.print("\nLA V1 IN X Y Z E':\n ");
    for (int i = 0; i < 3; i++)
    {
      deltaA[i]=A_2_M[i]-A_1_M[i];
      v1[i]= deltaA[i]*(intervallo/1000);
      Serial.println(v1[i],6);
      Serial.print(" ");
      deltaA[i] = 0;
    }
    Serial.print(" m/s\n");
    intervallo = 0;
    tv1= millis();
}

//misurazione V2:

Measure();

for (int i = 0; i < 3; i++)
{
  A_1_M[i]=A_M[i];
  A_M[i]=0;
}
t1 = millis();

while((millis()-t1)<=2000){

    Measure();

    for (int i = 0; i < 3; i++)
    {
      A_2_M[i]=A_M[i];
      A_M[i]=0;
    }
    t2 = millis();
    intervallo = t2-t1; 

    Serial.print("\nLA V2 IN X Y Z E':\n ");
    for (int i = 0; i < 3; i++)
    {
      deltaA[i]=A_2_M[i]-A_1_M[i];
      v2[i]= deltaA[i]*(intervallo/1000);
      Serial.println(v2[i],6);
      Serial.print(" ");
      deltaA[i] = 0;
    }
    Serial.print(" m/s\n");
    intervallo = 0;
    tv2= millis();
}

Vintervallo = tv2-tv1;

for (int i = 0; i < 3; i++)
{

  deltaA[i] = 0;
  deltaA[i] = (v2[i]-v1[i])/(Vintervallo/1000);
  if (deltaA[i]>=G)
  {
    Serial.println("\n");
    Serial.print(deltaA[i]);
    Serial.println(" m/s^2\n");
    Serial.print("\n SCATTA FOTO!");
  }else{
    Serial.println("\n");
    Serial.print(deltaA[i]);
    Serial.println(" m/s^2\n");
    Serial.print("\n NON SCATTARE FOTO!");
  }
}
}

