#include <Wire.h>
#include <Adafruit_BNO08x.h>
#include <math.h>

// --- PIN DEFINITIONS (ESP32-S3 Safe Pins) ---
#define I2C_SDA 5
#define I2C_SCL 6
#define FLEX_PIN 4

// --- CALIBRATION CONSTANTS ---
int FLEX_FLAT = 1180;          
int FLEX_BENT = 800;           
const float EXPECTED_FLEXION = 95.0; 
const float STRIKE_THRESHOLD = 2.5;  

// --- SMOOTHING FILTER VARIABLES ---
const int numReadings = 10;
int readings[numReadings];      
int readIndex = 0;              
int total = 0;                  
int smoothedRaw = 0;

unsigned long lastStepTime = 0;
float cadenceSPM = 0;

Adafruit_BNO08x bno08x(-1);
sh2_SensorValue_t sensorValue;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin(I2C_SDA, I2C_SCL);
  pinMode(FLEX_PIN, INPUT);

  // Initialize smoothing array to 0
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }

  if (!bno08x.begin_I2C()) {
    Serial.println("IMU Error! Check your wiring.");
    while (1) { delay(10); } 
  }
  
  bno08x.enableReport(SH2_LINEAR_ACCELERATION);
}

void loop() {
  // ---------------------------------------------------------
  // 1. KINEMATIC PROCESSING WITH SMOOTHING FILTER
  // ---------------------------------------------------------
  total = total - readings[readIndex];
  readings[readIndex] = analogRead(FLEX_PIN);
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;
  smoothedRaw = total / numReadings; // This kills the electrical jitter!

  float actualAngle = map(smoothedRaw, FLEX_FLAT, FLEX_BENT, 0, 90);
  actualAngle = constrain(actualAngle, 0, 135); 

  // ---------------------------------------------------------
  // 2. HEEL STRIKE & CADENCE (IMU)
  // ---------------------------------------------------------
  float magnitude = 0;
  float strikeSpike = 0; 
  unsigned long currentTime = millis();

  if (bno08x.getSensorEvent(&sensorValue) && sensorValue.sensorId == SH2_LINEAR_ACCELERATION) {
    float x = sensorValue.un.linearAcceleration.x;
    float y = sensorValue.un.linearAcceleration.y;
    float z = sensorValue.un.linearAcceleration.z;
    
    magnitude = sqrt((x * x) + (y * y) + (z * z));
    
    if (magnitude > STRIKE_THRESHOLD) {
      if (currentTime - lastStepTime > 300) {
        float timeInSeconds = (currentTime - lastStepTime) / 1000.0;
        cadenceSPM = 60.0 / timeInSeconds; 
        if (cadenceSPM > 150) cadenceSPM = 150; 
        strikeSpike = 30.0; 
        lastStepTime = currentTime;
      }
    }
  }

  if (currentTime - lastStepTime > 2000) {
    cadenceSPM = 0;
  }

  // ---------------------------------------------------------
  // 3. DIVERGENCE ENGINE & CONCORDANCE
  // ---------------------------------------------------------
  float divergenceDelta = EXPECTED_FLEXION - actualAngle;
  if (divergenceDelta < 0) divergenceDelta = 0; 

  float concordanceScore = 100.0 - divergenceDelta;
  if (concordanceScore < 0) concordanceScore = 0;

  // ---------------------------------------------------------
  // 4. PLOTTER OUTPUT
  // ---------------------------------------------------------
  Serial.print("Actual_Angle:");
  Serial.print(actualAngle);
  Serial.print("\tExpected:");
  Serial.print(EXPECTED_FLEXION);
  Serial.print("\tDivergence_Delta:");
  Serial.print(divergenceDelta);
  Serial.print("\tConcordance_%:");
  Serial.print(concordanceScore);
  Serial.print("\tCadence_SPM:");
  Serial.print(cadenceSPM);
  Serial.print("\tStrike:");
  Serial.println(strikeSpike);

  delay(50); 
}