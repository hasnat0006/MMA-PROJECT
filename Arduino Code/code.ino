#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <Arduino.h>
#include "WebSocketServer.h"

MAX30105 particleSensor;
WebSocketServer wsServer(80, "/ws");

#define xpin 34  
#define ypin 35  
#define zpin 32  
#define UVOUT 33   
#define REF_3V3 36 
#define EN_PIN 25

// Define buffers for SpO2/Heart rate calculations
uint32_t irBuffer[100];  // infrared LED sensor data
uint32_t redBuffer[100]; // red LED sensor data
int32_t bufferLength;    // data length
int32_t spo2;            // SpO2 value
int8_t validSPO2;        // indicator to show if the SpO2 calculation is valid
int32_t heartRate;       // heart rate value
int8_t validHeartRate;   // indicator to show if the heart rate calculation is valid

// Step detection variables
int stepCount = 0;
float prevZ = 0;
const float stepThreshold = 3;
const unsigned long debounceDelay = 300;
unsigned long lastStepTime = 0;

void setup() {
  Serial.begin(115200);
  wsServer.begin("9S", "88888899");

  // Initialize MAX30105 sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 was not found. Please check wiring/power.");
    while (1);
  }

  // Initialize UV sensor and accelerometer
  analogReadResolution(10);
  particleSensor.setup();

  pinMode(UVOUT, INPUT);
  pinMode(REF_3V3, INPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, HIGH);  // Enable the UV sensor

  Serial.println("Setup complete.");
}

void loop() {
  // Process heart rate and SpO2
  processSpO2AndHeartRate();

  // Process UV and step data
  float uvIntensity = UV();
  gyro();

  // Transmit temperature and UV intensity via WebSocket
  float temperature = particleSensor.readTemperature();
  sendTemperatureAndUVData(temperature, uvIntensity);

  // Transmit heart rate and SpO2 via WebSocket
  sendSpO2AndHeartRate();

  // Transmit step count via WebSocket
  sendStepCount();

  delay(200);
}

void processSpO2AndHeartRate() {
  bufferLength = 100; // Buffer length of 100 stores 4 seconds of samples

  // Read the first 100 samples
  for (byte i = 0; i < bufferLength; i++) {
    while (!particleSensor.available()) {
      particleSensor.check(); // Check the sensor for new data
    }
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  // Calculate heart rate and SpO2
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
}

void gyro() {
  int x = analogRead(xpin);
  int y = analogRead(ypin);
  int z = analogRead(zpin);

  float Gz = ((float)z - 340) / 68 * 9.8;

  unsigned long currentTime = millis();
  if (abs(Gz - prevZ) > stepThreshold && (currentTime - lastStepTime > debounceDelay)) {
    stepCount++;
    lastStepTime = currentTime;
    Serial.println("Step detected!");
  }
  prevZ = Gz;
}

float UV() {
  int uvLevel = averageAnalogRead(UVOUT);
  int refLevel = averageAnalogRead(REF_3V3);
  float outputVoltage = 3.3 / refLevel * uvLevel;
  return mapfloat(outputVoltage, 0.99, 2.9, 0.0, 15.0);
}

void sendTemperatureAndUVData(float temperature, float uvIntensity) {
  char *temperatureData = (char *)malloc(60);
  if (temperatureData != NULL) {
    sprintf(temperatureData, "{\"t\":\"tmp\",\"dt\":{\"T\":%f, \"U\":%f}}", temperature, uvIntensity);
    wsServer.sendMsg(temperatureData);
    free(temperatureData);
  }
}

void sendSpO2AndHeartRate() {
  char *hrData = (char *)malloc(100);
  if (hrData != NULL) {
    sprintf(hrData, "{\"t\":\"hr\",\"dt\":{\"hr\":%d,\"spo2\":%d,\"hrValid\":%d,\"spo2Valid\":%d}}",
            heartRate, spo2, validHeartRate, validSPO2);
    wsServer.sendMsg(hrData);
    free(hrData);
  }
}

void sendStepCount() {
  char *stepData = (char *)malloc(40);
  if (stepData != NULL) {
    sprintf(stepData, "{\"t\":\"s\",\"dt\":{\"s\":%d}}", stepCount);
    wsServer.sendMsg(stepData);
    free(stepData);
  }
}

int averageAnalogRead(int pinToRead) {
  byte numberOfReadings = 8;
  unsigned int runningValue = 0;
  for (int x = 0; x < numberOfReadings; x++) {
    runningValue += analogRead(pinToRead);
  }
  return runningValue / numberOfReadings;
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
