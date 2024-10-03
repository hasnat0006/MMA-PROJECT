
#include <Wire.h>
#include "MAX30105.h"
#include <Arduino.h>
#include "WebSocketServer.h"
WebSocketServer wsServer(80, "/ws");

MAX30105 particleSensor;

//ADXL sensor pin setup
#define xpin 34  
#define ypin 35  
#define zpin 32  


// UV sensor pin setup
#define UVOUT 33   //amr eikhane G23
#define REF_3V3 36  //amr eikhane SP
#define EN_PIN 25

/*
MAX30105 sensor pin setup
SDA: 21
SCL: 22
Vin: 3v3
GND: GND
*/

// Heart rate things
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;


int stepCount = 0;                       
float prevZ = 0;                         
bool stepDetected = false;               
const float stepThreshold = 3;           
const unsigned long debounceDelay = 300; 
unsigned long lastStepTime = 0;      


void setup() {
  Serial.begin(115200);
  wsServer.begin("ESP", "00000000");
  Serial.println("MAX30105 Basic Readings Example");
  if (particleSensor.begin() == false) {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1)
      ;
  }
  analogReadResolution(10);
  particleSensor.setup();

  pinMode(UVOUT, INPUT);
  pinMode(REF_3V3, INPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);  

  Serial.println("MP8511 example with EN pin control on ESP32");
}

void gyro() {
  // Read analog values from the accelerometer
  int x = analogRead(xpin);  // Read XOUT
  int y = analogRead(ypin);  // Read YOUT
  int z = analogRead(zpin);  // Read ZOUT

  // Convert raw readings into G-force values
  // float G = (axis_value - calibration_value) / scale_factor * 9.8
  float Gx = ((float)x - 331.5) / 65 * 9.8;
  float Gy = ((float)y - 329.5) / 68.5 * 9.8;
  float Gz = ((float)z - 340) / 68 * 9.8;
  
  // from data sheet
  //   Sensitivity: 330 mV/g
  // ADC reference voltage: 3.3V
  // ADC resolution: 10 bits (1024 levels)

  // Current timestamp
  unsigned long currentTime = millis();

  // Step detection based on Z-axis G-force change
  if (abs(Gz - prevZ) > stepThreshold && (currentTime - lastStepTime > debounceDelay)) {
    // A significant change in Z-axis acceleration indicates a step
    stepCount++;
    lastStepTime = currentTime;  // Update the timestamp of the last step
    stepDetected = true;
    Serial.println("Step detected!");
  } else {
    stepDetected = false;
  }

  // Store the current Z-axis value for the next loop iteration
  prevZ = Gz;

  // Output the current acceleration and step count to the serial monitor
  Serial.print("Gx: ");
  Serial.print(Gx);
  Serial.print("\tGy: ");
  Serial.print(Gy);
  Serial.print("\tGz: ");
  Serial.print(Gz);
  Serial.print("\tSteps: ");
  Serial.println(stepCount);
}


float UV() {
  // Read UV sensor data
  int uvLevel = averageAnalogRead(UVOUT);
  int refLevel = averageAnalogRead(REF_3V3);
  float outputVoltage = 3.3 / refLevel * uvLevel;
  // Convert the voltage to UV intensity (mW/cm^2)
  // The formula for UV intensity is given in the datasheet
  // The output voltage is in the range of 0.99V to 2.9V
  // The UV intensity is in the range of 0.0 mW/cm^2 to 15.0 mW/cm^2
  float uvIntensity = mapfloat(outputVoltage, 0.99, 2.9, 0.0, 15.0);

  // Output data to Serial Monitor
  Serial.print("MP8511 output: ");
  Serial.print(uvLevel);
  Serial.print(" MP8511 voltage: ");
  Serial.print(outputVoltage);
  Serial.print(" UV Intensity (mW/cm^2): ");
  Serial.print(uvIntensity);
  Serial.println();
  return uvIntensity;
}


void loop() {
  gyro();
  float uvIntensity = UV();

  float temperature = particleSensor.readTemperature();
  char *temperatureData = (char *)malloc(60);                                           
  if (temperatureData != NULL) 
  
  // {
  //   type : {
  //     "tmp" : {
  //       "data" : {
  //         "T" : 0.0,
  //         "U" : 0.0
  //       }
  //     }
  //   }
  // }
  
  {                                                                       
    sprintf(temperatureData, "{\"t\":\"tmp\",\"dt\":{\"T\":%f, \"U\":%f}}", temperature, uvIntensity); 
    wsServer.sendMsg(temperatureData);  // Send the message

    free(temperatureData);  // Free the allocated memory after the message is sent
  } else {
    Serial.println("Memory allocation failed");
  }
  delay(200);


  char *msg = (char *)malloc(100); 
  if (msg != NULL) {
    sprintf(msg, "{\"t\":\"rig\",\"dt\":{\"r\":%d,\"ir\":%d,\"g\":%d}}",
            particleSensor.getRed(),
            particleSensor.getIR(),
            particleSensor.getGreen());

    wsServer.sendMsg(msg);  
    free(msg);             
  } else {
    Serial.println("Memory allocation failed");
  }
  delay(200);


  char *msg1 = (char *)malloc(40);                              
  if (msg1 != NULL) {                                           
    sprintf(msg1, "{\"t\":\"s\",\"dt\":{\"s\":%d}}", stepCount); 
    wsServer.sendMsg(msg1); 
    free(msg1);  
  } 
  else {
    Serial.println("Memory allocation failed");
  }
  delay(200);
}

int averageAnalogRead(int pinToRead) {
  byte numberOfReadings = 8;
  unsigned int runningValue = 0;

  for (int x = 0; x < numberOfReadings; x++) {
    runningValue += analogRead(pinToRead);
  }
  runningValue /= numberOfReadings;

  return runningValue;
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}