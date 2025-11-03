/*
 * ESP32 ADVANCED IRRIGATION SYSTEM
 *
 * *** UPDATED: Full Phase 2 Metrics & Logging ***
 *
 * - ADDED: All variables & logic for Phase 2 (Steps 2, 3, 4).
 * - ADDED: PUMP_FLOW_RATE_LITERS_PER_MINUTE constant.
 * - MODIFIED: State change logic to immediately log new metrics.
 * - MODIFIED: logDailyReport() to show a full summary of all metrics.
 */

// --- 1. PIN DEFINITIONS ---
const int RELAY_PIN = 23;
const int MUX_COMMON_PIN = 34;
const int MUX_S0_PIN = 19;
const int MUX_S1_PIN = 18;
const int MUX_S2_PIN = 5;

// --- 2. CRITICAL SETTINGS ---

// --- SENSOR CALIBRATION (0-4095) ---
const int CALIBRATION_DRY = 3500; // 0% moisture (in air)
const int CALIBRATION_WET = 1200; // 100% moisture (in water)

// --- THRESHOLDS ---
const int DRY_THRESHOLD = 3000; // (e.g., 14%)
const int WET_THRESHOLD = 1500; // (e.g., 87%)

// --- TIMING (in milliseconds: 60000 = 1 min) ---
const unsigned long CHECK_INTERVAL_MS = 60000;    // (1 minute)
const unsigned long MIN_PUMP_ON_TIME_MS = 300000; // (5 minutes)
const unsigned long POST_IRRIGATION_WAIT_TIME_MS = 14400000; // (4 hours)

// --- ALGORITHM LOGIC ---
const int MIN_DRY_SENSORS_TO_TRIGGER = 3;
const int NEIGHBOR_DISTANCE_THRESHOLD = 15;
const long NEIGHBOR_DISTANCE_THRESHOLD_SQUARED = 225;
const int PUMP_ON = HIGH;
const int PUMP_OFF = LOW;

// --- NEW (PHASE 2, STEP 4): PUMP FLOW RATE ---
// (Set this to your pump's flow rate in Liters/min)
const float PUMP_FLOW_RATE_LITERS_PER_MINUTE = 40.0;

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
  int moisturePercentage;
};
const int NUM_SENSORS = 8;
SensorNode sensorMap[NUM_SENSORS];

// --- 4. PHASE 2 METRIC VARIABLES ---
unsigned long timeToWet = 0;
unsigned long timeToDry = 0;
unsigned long wateringDuration = 0;
unsigned long timeSinceLastIrrigation = 0;
unsigned long dailyCheckTime = 0;

// --- NEW: Variables for Daily Report ---
int irrigationFrequencyCount = 0;
long totalMoistureReadings = 0;
long totalMoistureSum = 0;
float totalWateringDurationMin = 0;
float totalLitersUsed = 0;
float totalTimeToWetMin = 0;
float totalTimeToDryHours = 0;


// --- 5. SIMULATION ---
int simulatedValues[NUM_SENSORS];
// --- END OF SETTINGS ---


void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Irrigation (Phase 2 Metrics) Initializing...");
  Serial.println("Ready for simulation commands.");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MUX_S0_PIN, OUTPUT);
  pinMode(MUX_S1_PIN, OUTPUT);
  pinMode(MUX_S2_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, PUMP_OFF);

  initializeSensorMap();

  for (int i = 0; i < NUM_SENSORS; i++) {
    simulatedValues[i] = -1;
  }
  
  lastCheckTime = millis();
  dailyCheckTime = millis();
  wateringStopTime = millis(); // Initialize to now
}

void loop() {
  checkSerialCommands();

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
  
  if (millis() - dailyCheckTime >= 86400000) { // 24 hours
    logDailyReport();
    resetDailyMetrics();
  }
}

// --- COMMAND & STATE FUNCTIONS ---

void checkSerialCommands() {
  // ... (This function is unchanged)
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    Serial.print("COMMAND RECEIVED: ");
    Serial.println(input);
    if (input.startsWith("S")) {
      int colonIndex = input.indexOf(':');
      if (colonIndex > 0) {
        int sensorNum = input.substring(1, colonIndex).toInt();
        int sensorValue = input.substring(colonIndex + 1).toInt();
        if (sensorNum >= 0 && sensorNum < NUM_SENSORS) {
          simulatedValues[sensorNum] = sensorValue;
          Serial.print("SIM: Sensor ");
          Serial.print(sensorNum);
          Serial.print(" override value set to ");
          Serial.println(sensorValue);
        }
      }
    }
    else if (input.startsWith("STATE:")) {
      String newState = input.substring(6);
      newState.toUpperCase();
      if (newState == "MONITORING") {
        currentState = MONITORING;
        lastCheckTime = millis();
        Serial.println("SIM: State FORCED to MONITORING.");
      } else if (newState == "IRRIGATING") {
        currentState = IRRIGATING;
        pumpStartTime = millis();
        timeToDry = millis() - wateringStopTime; // Log timeToDry
        digitalWrite(RELAY_PIN, PUMP_ON);
        Serial.println("SIM: State FORCED to IRRIGATING.");
      } else if (newState == "WAITING") {
        currentState = WAITING;
        wateringStopTime = millis();
        wateringDuration = millis() - pumpStartTime; // Log duration
        timeToWet = millis() - pumpStartTime; // Log timeToWet
        digitalWrite(RELAY_PIN, PUMP_OFF);
        Serial.println("SIM: State FORCED to WAITING.");
      }
    }
    else if (input == "FORCE_CHECK") { lastCheckTime = 0; Serial.println("SIM: Forcing next MONITORING check."); }
    else if (input == "FORCE_IRRIGATE_CHECK") { pumpStartTime = 0; Serial.println("SIM: Forcing next IRRIGATING check."); }
    else if (input == "FORCE_WAIT_SKIP") { wateringStopTime = 0; Serial.println("SIM: Forcing WAITING skip."); }
  }
}

void handleMonitoring() {
  if (millis() - lastCheckTime >= CHECK_INTERVAL_MS) {
    lastCheckTime = millis();
    Serial.println("--- (Monitoring) ---");
    readAllSensors(); 
    
    if (checkForCluster()) {
      Serial.println("Dry cluster found!");
      Serial.println("State change: MONITORING -> IRRIGATING");
      
      // --- LOG METRIC (Drying Rate) ---
      timeToDry = millis() - wateringStopTime;
      float timeToDryHours = (float)timeToDry / 3600000.0; // ms to hours
      totalTimeToDryHours += timeToDryHours;
      Serial.print("METRIC: Field took ");
      Serial.print(timeToDryHours);
      Serial.println(" hours to dry.");
      
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

      // --- LOG METRICS (Wetting Rate & Duration) ---
      wateringDuration = millis() - pumpStartTime;
      float wateringDurationMin = (float)wateringDuration / 60000.0; // ms to minutes
      
      timeToWet = wateringDuration; // timeToWet = pump duration
      float timeToWetMin = wateringDurationMin;
      
      float litersUsed = wateringDurationMin * PUMP_FLOW_RATE_LITERS_PER_MINUTE;
      
      // Add to daily totals
      irrigationFrequencyCount++;
      totalWateringDurationMin += wateringDurationMin;
      totalTimeToWetMin += timeToWetMin;
      totalLitersUsed += litersUsed;
      
      Serial.print("METRIC: Pump ran for ");
      Serial.print(wateringDurationMin);
      Serial.println(" minutes.");
      Serial.print("METRIC: Field took ");
      Serial.print(timeToWetMin);
      Serial.println(" minutes to get wet.");
      Serial.print("METRIC: Estimated ");
      Serial.print(litersUsed);
      Serial.println(" Liters used.");
      
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
    timeSinceLastIrrigation = millis() - wateringStopTime;
    lastCheckTime = millis();
    currentState = MONITORING;
  } else {
    // ... (This function is unchanged)
    if (millis() - lastCheckTime >= 60000) {
      lastCheckTime = millis();
      Serial.print("Post-irrigation cooldown... (Time remaining: ");
      Serial.print((POST_IRRIGATION_WAIT_TIME_MS - (millis() - wateringStopTime)) / 60000);
      Serial.println(" min)");
    }
  }
}

// --- HELPER FUNCTIONS ---

int convertToPercentage(int rawValue) {
  int percentage = map(rawValue, CALIBRATION_DRY, CALIBRATION_WET, 0, 100);
  return constrain(percentage, 0, 100);
}

void readAllSensors() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    int channel = sensorMap[i].channel;
    int value = readSensor(channel);
    sensorMap[i].moistureValue = value;
    sensorMap[i].isDry = (value > DRY_THRESHOLD);
    sensorMap[i].moisturePercentage = convertToPercentage(value);

    // --- Add to daily average totals ---
    totalMoistureSum += sensorMap[i].moisturePercentage;
    totalMoistureReadings++;
    
    Serial.print("  Sensor (Ch ");
    Serial.print(channel);
    Serial.print("): ");
    Serial.print(value);
    Serial.print(" | ");
    Serial.print(sensorMap[i].moisturePercentage);
    Serial.print("%");
    if(sensorMap[i].isDry) Serial.println(" (DRY)");
    else Serial.println(" (WET)");
  }
}

bool checkIfAllSensorsWet() {
  // ... (This function is unchanged)
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorMap[i].moistureValue > WET_THRESHOLD) {
      Serial.print("...Sensor (Ch ");
      Serial.print(sensorMap[i].channel);
      Serial.print(") is still too dry (Val: ");
      Serial.print(sensorMap[i].moistureValue);
      Serial.print(" | ");
      Serial.print(sensorMap[i].moisturePercentage);
      Serial.println("%)");
      return false; 
    }
  }
  return true;
}

int readSensor(int channel) {
  // ... (This function is unchanged)
  if (simulatedValues[channel] != -1) {
    int simValue = simulatedValues[channel];
    simulatedValues[channel] = -1; 
    return simValue;
  }
  digitalWrite(MUX_S0_PIN, bitRead(channel, 0));
  digitalWrite(MUX_S1_PIN, bitRead(channel, 1));
  digitalWrite(MUX_S2_PIN, bitRead(channel, 2));
  delay(10);
  return analogRead(MUX_COMMON_PIN);
}

bool checkForCluster() {
  // ... (This function is unchanged)
  int dryCount = 0;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorMap[i].isDry) dryCount++;
  }
  if (dryCount < MIN_DRY_SENSORS_TO_TRIGGER) {
    return false;
  }
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

/**
 * @brief NEW: Prints a full daily report of all logged metrics.
 */
void logDailyReport() {
  Serial.println("=========================================");
  Serial.println("--- 24-HOUR DAILY REPORT ---");
  Serial.println("=========================================");
  
  Serial.print("Irrigation Cycles Today: ");
  Serial.println(irrigationFrequencyCount);

  // Calculate averages
  float avgMoisture = (float)totalMoistureSum / (float)totalMoistureReadings;
  float avgWateringDuration = totalWateringDurationMin / (float)irrigationFrequencyCount;
  float avgTimeToWet = totalTimeToWetMin / (float)irrigationFrequencyCount;
  float avgTimeToDry = totalTimeToDryHours / (float)irrigationFrequencyCount;

  // Prevent divide-by-zero if no irrigation occurred
  if (irrigationFrequencyCount == 0) {
    avgWateringDuration = 0;
    avgTimeToWet = 0;
    avgTimeToDry = 0;
  }

  Serial.print("Average Field Moisture: ");
  Serial.print(avgMoisture);
  Serial.println("%");
  
  Serial.print("Total Water Used (Est.): ");
  Serial.print(totalLitersUsed);
  Serial.println(" Liters");

  Serial.print("Avg. Watering Duration: ");
  Serial.print(avgWateringDuration);
  Serial.println(" minutes");

  Serial.print("Avg. Time to Wet (Wetting Rate): ");
  Serial.print(avgTimeToWet);
  Serial.println(" minutes");

  Serial.print("Avg. Time to Dry (Drying Rate): ");
  Serial.print(avgTimeToDry);
  Serial.println(" hours");
  
  Serial.println("=========================================");
}

/**
 * @brief NEW: Resets all daily counters.
 */
void resetDailyMetrics() {
  irrigationFrequencyCount = 0;
  totalMoistureSum = 0;
  totalMoistureReadings = 0;
  totalWateringDurationMin = 0;
  totalLitersUsed = 0;
  totalTimeToWetMin = 0;
  totalTimeToDryHours = 0;
  dailyCheckTime = millis(); // Reset 24h timer
}

void initializeSensorMap() {
  // ... (This function is unchanged)
  sensorMap[0] = {10, 10, 1, false, 0, 0}; 
  sensorMap[1] = {10, 20, 2, false, 0, 0}; 
  sensorMap[2] = {10, 30, 3, false, 0, 0};
  sensorMap[3] = {10, 40, 4, false, 0, 0};
  sensorMap[4] = {20, 10, 5, false, 0, 0};
  sensorMap[5] = {20, 20, 6, false, 0, 0};
  sensorMap[6] = {20, 30, 7, false, 0, 0}; 
  sensorMap[7] = {20, 40, 0, false, 0, 0};
}