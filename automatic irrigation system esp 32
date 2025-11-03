/*
 * AUTOMATIC MULTI-SENSOR IRRIGATION SYSTEM
 *
 * *** UPDATED FOR 8 SENSORS (Channels 0-7) ***
 *
 * - ESP32 3.3V logic (12-bit ADC: 0-4095)
 * - Implements 3-state machine (MONITORING, IRRIGATING, WAITING)
 * - Implements dual thresholds (DRY/WET)
 * - Implements clustering logic
 */

// --- 1. PIN DEFINITIONS (ESP32 GPIO) ---
const int RELAY_PIN = 23;      // GPIO 23
const int MUX_COMMON_PIN = 34; // GPIO 34 (ADC1_CH6)
const int MUX_S0_PIN = 19;     // GPIO 19
const int MUX_S1_PIN = 18;     // GPIO 18
const int MUX_S2_PIN = 5;      // GPIO 5

// --- 2. !!! CRITICAL SETTINGS - YOU MUST RE-CALIBRATE !!! ---
const int DRY_THRESHOLD = 3000; // Turn ON when a cluster is *ABOVE* this
const int WET_THRESHOLD = 1500; // Turn OFF when ALL sensors are *BELOW* this

// TIMING (in milliseconds: 60000 = 1 min)
const unsigned long CHECK_INTERVAL_MS = 60000;    // (1 minute)
const unsigned long MIN_PUMP_ON_TIME_MS = 300000; // (5 minutes)
const unsigned long POST_IRRIGATION_WAIT_TIME_MS = 14400000; // (4 hours)

// ALGORITHM LOGIC
const int MIN_DRY_SENSORS_TO_TRIGGER = 3;
const int NEIGHBOR_DISTANCE_THRESHOLD = 15;
const long NEIGHBOR_DISTANCE_THRESHOLD_SQUARED = (long)NEIGHBOR_DISTANCE_THRESHOLD * NEIGHBOR_DISTANCE_THRESHOLD;

// RELAY LOGIC
const int PUMP_ON = HIGH;
const int PUMP_OFF = LOW;

// --- 3. SYSTEM STATE & SENSOR MAP ---
enum SystemState { MONITORING, IRRIGATING, WAITING };
SystemState currentState = MONITORING;

unsigned long lastCheckTime = 0;
unsigned long pumpStartTime = 0;
unsigned long wateringStopTime = 0;

struct SensorNode {
  int x, y, channel;
  bool isDry;
  int moistureValue;
};
// *** UPDATED FOR 8 SENSORS ***
const int NUM_SENSORS = 8;
SensorNode sensorMap[NUM_SENSORS];
// --- END OF SETTINGS ---


void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Advanced Irrigation (8-Sensor) Initializing...");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MUX_S0_PIN, OUTPUT);
  pinMode(MUX_S1_PIN, OUTPUT);
  pinMode(MUX_S2_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, PUMP_OFF);

  // *** IMPORTANT: DEFINE/CHECK YOUR SENSOR MAP ***
  initializeSensorMap();
  
  Serial.println("Sensor map initialized. Current State: MONITORING");
  lastCheckTime = millis();
}

void loop() {
  switch (currentState) {
    case MONITORING:
      handleMonitoring();
      break;
    case IRRIGATING:
      handleIrrigating();
      break;
    case WAITING:
      handleWaiting();
      break;
  }
}

// --- STATE HANDLER FUNCTIONS ---

void handleMonitoring() {
  if (millis() - lastCheckTime >= CHECK_INTERVAL_MS) {
    lastCheckTime = millis();
    Serial.println("--- (Monitoring) ---");
    readAllSensors(); 

    if (checkForCluster()) {
      Serial.println("Dry cluster found!");
      Serial.println("State change: MONITORING -> IRRIGATING");
      digitalWrite(RELAY_PIN, PUMP_ON);
      pumpStartTime = millis();
      currentState = IRRIGATING;
    } else {
      Serial.println("Field is OK. Next check in 1 min.");
    }
  }
}

void handleIrrigating() {
  if (millis() - pumpStartTime < MIN_PUMP_ON_TIME_MS) {
    if (millis() - lastCheckTime >= 10000) {
        lastCheckTime = millis();
        Serial.print("Irrigating... (Min run time remaining: ");
        Serial.print((MIN_PUMP_ON_TIME_MS - (millis() - pumpStartTime)) / 1000);
        Serial.println(" sec)");
    }
    return;
  }

  if (millis() - lastCheckTime >= 10000) {
    lastCheckTime = millis();
    Serial.println("Min pump time complete. Checking if field is wet...");
    readAllSensors();

    if (checkIfAllSensorsWet()) {
      Serial.println("Field is now fully wet.");
      Serial.println("State change: IRRIGATING -> WAITING");
      digitalWrite(RELAY_PIN, PUMP_OFF);
      wateringStopTime = millis();
      currentState = WAITING;
    } else {
      Serial.println("Field is not wet enough yet. Continuing to water.");
    }
  }
}

void handleWaiting() {
  if (millis() - wateringStopTime >= POST_IRRIGATION_WAIT_TIME_MS) {
    Serial.println("Cooldown/Lockout complete.");
    Serial.println("State change: WAITING -> MONITORING");
    lastCheckTime = millis();
    currentState = MONITORING;
  } else {
    if (millis() - lastCheckTime >= 60000) {
      lastCheckTime = millis();
      Serial.print("Post-irrigation cooldown... (Time remaining: ");
      Serial.print((POST_IRRIGATION_WAIT_TIME_MS - (millis() - wateringStopTime)) / 60000);
      Serial.println(" min)");
    }
  }
}

// --- HELPER FUNCTIONS ---

void readAllSensors() {
  // *** UPDATED: Loop 0 through 7 ***
  for (int i = 0; i < NUM_SENSORS; i++) {
    int channel = sensorMap[i].channel;
    int value = readSensor(channel);
    sensorMap[i].moistureValue = value;
    sensorMap[i].isDry = (value > DRY_THRESHOLD);
  }
}

bool checkIfAllSensorsWet() {
  // *** UPDATED: Loop 0 through 7 ***
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorMap[i].moistureValue > WET_THRESHOLD) {
      Serial.print("...Sensor (Ch ");
      Serial.print(sensorMap[i].channel);
      Serial.print(") is still too dry (Val: ");
      Serial.print(sensorMap[i].moistureValue);
      Serial.println(")");
      return false; 
    }
  }
  return true;
}

int readSensor(int channel) {
  digitalWrite(MUX_S0_PIN, bitRead(channel, 0));
  digitalWrite(MUX_S1_PIN, bitRead(channel, 1));
  digitalWrite(MUX_S2_PIN, bitRead(channel, 2));
  delay(10);
  return analogRead(MUX_COMMON_PIN);
}

bool checkForCluster() {
  int dryCount = 0;
  // *** UPDATED: Loop 0 through 7 ***
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorMap[i].isDry) dryCount++;
  }

  if (dryCount < MIN_DRY_SENSORS_TO_TRIGGER) {
    return false;
  }
  
  // *** UPDATED: Loop 0 through 7 ***
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorMap[i].isDry) {
      for (int j = i + 1; j < NUM_SENSORS; j++) {
        if (sensorMap[j].isDry) {
          long dx = sensorMap[i].x - sensorMap[j].x;
          long dy = sensorMap[i].y - sensorMap[j].y;
          long distanceSquared = (dx * dx) + (dy * dy);
          if (distanceSquared <= NEIGHBOR_DISTANCE_THRESHOLD_SQUARED) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void initializeSensorMap() {
  // !!! EDIT THIS TO MATCH YOUR FIELD LAYOUT !!!
  // *** UPDATED: Added Channel 0 ***
  sensorMap[0] = {10, 10, 1, false, 0}; 
  sensorMap[1] = {10, 20, 2, false, 0}; 
  sensorMap[2] = {10, 30, 3, false, 0};
  sensorMap[3] = {10, 40, 4, false, 0};
  sensorMap[4] = {20, 10, 5, false, 0};
  sensorMap[5] = {20, 20, 6, false, 0};
  sensorMap[6] = {20, 30, 7, false, 0}; 
  sensorMap[7] = {20, 40, 0, false, 0}; // <-- NEW SENSOR ON CHANNEL 0 (Set X,Y)
}