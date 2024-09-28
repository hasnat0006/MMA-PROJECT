
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


int stepCount = 0;                        // Step counter
float prevZ = 0;                          // Previous Z-axis value for detecting peaks
bool stepDetected = false;                // Flag to avoid double-counting steps
const float stepThreshold = 3;            // G-force threshold for a step
const unsigned long debounceDelay = 300;  // 300 ms debounce delay
unsigned long lastStepTime = 0;           // Timestamp of last step detection


void setup() {
  Serial.begin(115200);
  wsServer.begin("9S", "88888899");
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
  digitalWrite(EN_PIN, HIGH);  // Enable the UV sensor

  Serial.println("MP8511 example with EN pin control on ESP32");
}

void gyro() {
  // Read analog values from the accelerometer
  int x = analogRead(xpin);  // Read XOUT
  int y = analogRead(ypin);  // Read YOUT
  int z = analogRead(zpin);  // Read ZOUT

  // Convert raw readings into G-force values
  float Gx = ((float)x - 331.5) / 65 * 9.8;
  float Gy = ((float)y - 329.5) / 68.5 * 9.8;
  float Gz = ((float)z - 340) / 68 * 9.8;

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



// float getHeartRate() {
//   float beatAvg = 0;
//   long irValue;
//   for (int i = 0; i < 100; i++) {
//     irValue = particleSensor.getIR();
//     if (1) {
//       //We sensed a beat!
//       long delta = millis() - lastBeat;
//       lastBeat = millis();

//       beatsPerMinute = 60 / (delta / 1000.0);

//       if (beatsPerMinute < 255 && beatsPerMinute > 20) {
//         rates[rateSpot++] = (byte)beatsPerMinute;  //Store this reading in the array
//         rateSpot %= RATE_SIZE;                     //Wrap variable

//         //Take average of readings
//         for (byte x = 0; x < RATE_SIZE; x++)
//           beatAvg += rates[x];
//         beatAvg /= RATE_SIZE;
//       }
//     }
//     Serial.print("IR=");
//     Serial.print(irValue);
//     Serial.print(", BPM=");
//     Serial.print(beatsPerMinute);
//     Serial.print(", Avg BPM=");
//     Serial.print(beatAvg);
//     Serial.println();
//     if (irValue < 50000) {
//       return -1;
//     }
//   }


//   return beatAvg;
// }


void loop() {
  gyro();
  float uvIntensity = UV();

  float temperature = particleSensor.readTemperature();
  char *temperatureData = (char *)malloc(60);                                           
  if (temperatureData != NULL) {                                                                       
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

    wsServer.sendMsg(msg);  // Send the message
    free(msg);              // Free the memory after sending the message
  } else {
    Serial.println("Memory allocation failed");
  }
  delay(200);


  char *msg1 = (char *)malloc(40);                                // Allocate memory for the message
  if (msg1 != NULL) {                                             // Check if memory allocation was successful
    sprintf(msg1, "{\"t\":\"s\",\"dt\":{\"s\":%d}}", stepCount);  // Format the message

    wsServer.sendMsg(msg1);  // Send the message

    free(msg1);  // Free the allocated memory after the message is sent
  } else {
    Serial.println("Memory allocation failed");
  }
  delay(200);
}


// Function to take an average of analog readings on a given pin
int averageAnalogRead(int pinToRead) {
  byte numberOfReadings = 8;
  unsigned int runningValue = 0;

  // Take multiple readings and sum them
  for (int x = 0; x < numberOfReadings; x++) {
    runningValue += analogRead(pinToRead);
  }
  runningValue /= numberOfReadings;

  return runningValue;
}

// Map function for floats
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}