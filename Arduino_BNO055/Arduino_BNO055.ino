#include <Wire.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <Adafruit_BNO055.h>

// Servicio y características BLE
BLEService dataService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLECharacteristic jsonChar("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify, 512);
BLECharacteristic controlChar("19B10002-E8F2-537E-4F6C-D104768A1214", BLEWrite, 1); // Característica para recibir booleano
BLECharacteristic calibrationChar("19B10003-E8F2-537E-4F6C-D104768A1214", BLEWrite | BLERead | BLENotify, 1);
BLEDevice central;

bool warming = false;
bool imu_calibrated = false;
int caso = 0;
String eje;
float x = 0.0, y = 0.0, z = 0.0;
float angleX = 0.0, angleY = 0.0, angleZ = 0.0;

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);
//could add VECTOR_ACCELEROMETER, VECTOR_MAGNETOMETER,VECTOR_GRAVITY...
sensors_event_t orientationData , angVelocityData , linearAccelData, magnetometerData, accelerometerData, gravityData;

void setup() {
  Serial.begin(9600);
  /* Initialise the sensor */
  if (!bno.begin())
  {
    /* There was a problem detecting the BNO055 ... check your connections */
    Serial.print("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }

  calibrateSensor();

  if (!BLE.begin()) {
    Serial.println("No se pudo iniciar el Bluetooth");
    while (1);
  }

  // Obtener la dirección MAC del dispositivo BLE
  String macAddress = BLE.address();
  Serial.print("La dirección MAC del Arduino Nano 33 IoT es: ");
  Serial.println(macAddress);

  BLE.setLocalName("ArduinoNano33BLE");
  BLE.setAdvertisedService(dataService);

  dataService.addCharacteristic(jsonChar);
  dataService.addCharacteristic(controlChar);
  dataService.addCharacteristic(calibrationChar);
  BLE.addService(dataService);

  BLE.advertise();
  Serial.println("Esperando conexión...");
}

void loop() {
  bno.getEvent(&orientationData, Adafruit_BNO055::VECTOR_EULER); 
  angleX = orientationData.gyro.x; // Velocidad angular en X
  angleY = orientationData.gyro.y; // Velocidad angular en Y
  angleZ = orientationData.gyro.z; // Velocidad angular en Z
  // Crear objeto JSON
  StaticJsonDocument<200> doc;
  doc["X"] = angleX;
  doc["Y"] = angleY;
  doc["Z"] = angleZ;
  // Convertir JSON a cadena
  char jsonBuffer[200];
  serializeJson(doc, jsonBuffer);

  if (central && central.connected()){
    warming = true;
    if(calibrationChar.written() && imu_calibrated == false){
      bool calibration = calibrationChar.value();
      if (calibration){
        Serial.println("Calibrando... Asegúrate de que el sensor está quieto.");
        calibrateSensor(); // Calibrar el sensor para determinar el offset
        calibrationChar.writeValue("1");
        imu_calibrated = true;
      }
    }

    if(controlChar.written()) {
      bool shouldSendData = controlChar.value();
      if (shouldSendData) {
        jsonChar.writeValue(jsonBuffer); // Enviar datos
      }
    }
  }else{
    if(warming){
      Serial.println("Desconectado de " + central.address());
      warming = false;
      imu_calibrated = false;
    }
    central = BLE.central();
  }
}

// Función para calibrar el sensor en reposo
void calibrateSensor() {
  Serial.println("Calibrando el sensor... Por favor, mantén el módulo quieto.");

  while (true) {
    // Verificar el estado de calibración
    uint8_t system, gyro, accel, mag;
    bno.getCalibration(&system, &gyro, &accel, &mag);

    Serial.print("Calibración - Sistema: ");
    Serial.print(system);
    Serial.print(", Giroscopio: ");
    Serial.print(gyro);
    Serial.print(", Acelerómetro: ");
    Serial.print(accel);
    Serial.print(", Magnetómetro: ");
    Serial.println(mag);

    // Asegurarse de que el acelerómetro esté completamente calibrado
    if (gyro == 3) {
      Serial.println("Acelerómetro calibrado.");
      break; // Salir del bucle si está calibrado
    }
    delay(100); // Esperar antes de verificar nuevamente
  }
  delay(2000); // Esperar antes de verificar nuevamente
  //bno.getEvent(&angVelocityData, Adafruit_BNO055::VECTOR_GYROSCOPE);
  //bno.getEvent(&linearAccelData, Adafruit_BNO055::VECTOR_LINEARACCEL);
  //bno.getEvent(&magnetometerData, Adafruit_BNO055::VECTOR_MAGNETOMETER);
  //bno.getEvent(&accelerometerData, Adafruit_BNO055::VECTOR_ACCELEROMETER);
  bno.getEvent(&gravityData, Adafruit_BNO055::VECTOR_GRAVITY);
  
  if(gravityData.acceleration.x >= 9.0){
    caso = 1;
  }else if(gravityData.acceleration.y >= 9.0){
    caso = 2;
  }else if (gravityData.acceleration.z >= 9.0){
    caso = 3;
  }else if(gravityData.acceleration.x <= -9.0){
    caso = 4;
  }else if(gravityData.acceleration.y <= -9.0){
    caso = 5;
  }else if(gravityData.acceleration.z <= -9.0){
    caso = 6;
  }

  switch (caso){
    case 1:
      Serial.println("Xdown");
      eje = "X";
      break;    
    case 2:
      Serial.println("Ydown");
      eje = "Y";
      break;
    case 3:
      Serial.println("Zdown");
      eje = "Z";
      break;
    case 4:
      Serial.println("Xup");
      eje = "X";
      break;
    case 5:
      Serial.println("Yup");
      eje = "Y";
      break;
    case 6:
      Serial.println("Zup"); 
      eje = "Z";
      break;
  }  
}
