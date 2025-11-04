/*
 * ESP32 ADVANCED IRRIGATION SYSTEM
 *
 * *** PATCHED VERSION (Fixes 3 Bugs) ***
 *
 * - FIX 1: Initialized simulatedValues[] in setup()
 * - FIX 2: Correctly added litersUsed to totalLitersUsed
 * - FIX 3: Corrected POST_IRRIGATION_WAIT_TIME_MS typo
 */

// --- 1. PIN DEFINITIONS (ESP32 GPIO) ---
const int RELAY_PIN = 23;
const int MUX_COMMON_PIN = 34;
const int MUX_S0_PIN = 19;
const int MUX_S1_PIN = 18;
const int MUX_S2_PIN = 5;

// --- 2. CRITICAL SETTINGS & THRESHOLDS ---
const int CALIBRATION_DRY = 3500;
const int CALIBRATION_WET = 1200;
const int DRY_THRESHOLD = 3000;
const int WET_THRESHOLD = 1500;
// Millisecond time constants for readability
const unsigned long MILLIS_PER_SECOND = 1000UL;
const unsigned long MILLIS_PER_MINUTE = 60UL * MILLIS_PER_SECOND;
const unsigned long MILLIS_PER_HOUR = 60UL * MILLIS_PER_MINUTE;
const unsigned long MILLIS_PER_DAY = 24UL * MILLIS_PER_HOUR;
const unsigned long MILLIS_PER_30_DAYS = 30UL * MILLIS_PER_DAY;

const unsigned long CHECK_INTERVAL_MS = MILLIS_PER_MINUTE;
const unsigned long MIN_PUMP_ON_TIME_MS = 300000;
// --- FIX 3: Corrected typo ---
const unsigned long POST_IRRIGATION_WAIT_TIME_MS = 14400000; // (4 hours)
const int MIN_DRY_SENSORS_TO_TRIGGER = 3;
const int NEIGHBOR_DISTANCE_THRESHOLD = 15;
const long NEIGHBOR_DISTANCE_THRESHOLD_SQUARED = 225;
const int PUMP_ON = HIGH;
const int PUMP_OFF = LOW;
const float PUMP_FLOW_RATE_LITERS_PER_MINUTE = 20.0;

// --- 3. CUSTOM TYPES (struct, enum) ---
enum SystemState { MONITORING, IRRIGATING, WAITING };

struct SensorNode {
  int x, y, channel;
  bool isDry;
  int moistureValue;
  int moisturePercentage;
};

// --- 4. GLOBAL VARIABLES ---
SystemState currentState = MONITORING;
const int NUM_SENSORS = 8;
SensorNode sensorMap[NUM_SENSORS];

// State Timers
unsigned long lastCheckTime = 0;
unsigned long pumpStartTime = 0;
unsigned long wateringStopTime = 0;

// Daily Metric Variables
unsigned long timeToWet = 0, timeToDry = 0, wateringDuration = 0, timeSinceLastIrrigation = 0;
unsigned long dailyCheckTime = 0;
int irrigationFrequencyCount = 0;
float totalWateringDurationMin = 0;
float totalLitersUsed = 0;
float totalTimeToWetMin = 0;
float totalTimeToDryHours = 0;
long totalMoistureSumPerSensor[NUM_SENSORS];
long totalMoistureReadingsPerSensor[NUM_SENSORS];
long totalFieldMoistureSum = 0;
long totalFieldMoistureReadings = 0;

// Monthly Metric Variables
unsigned long monthlyCheckTime = 0;
int monthlyIrrigationFrequency = 0;
float monthlyTotalLitersUsed = 0;
float monthlyTotalWateringMin = 0;
float monthlyAvgTimeToWet = 0;
float monthlyAvgTimeToDry = 0;
float monthlyAvgFieldMoisture = 0;
int monthlyReportCount = 0;

// Simulation Variable
int simulatedValues[NUM_SENSORS];

// --- 5. THE setup() FUNCTION ---
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Irrigation (PATCHED) Initializing...");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MUX_S0_PIN, OUTPUT);
  pinMode(MUX_S1_PIN, OUTPUT);
  pinMode(MUX_S2_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, PUMP_OFF);

  initializeSensorMap();
  resetDailyMetrics();    // Initializes daily counters (incl. per-sensor arrays)
  resetMonthlyMetrics();  // Initializes monthly counters
  
  // --- FIX 1: Initialize simulation array ---
  for (int i = 0; i < NUM_SENSORS; i++) {
    simulatedValues[i] = -1;
  }
  
  lastCheckTime = millis();
  wateringStopTime = millis();
}

// --- 6. THE loop() FUNCTION ---
void loop() {
  checkSerialCommands();

  switch (currentState) {
    case MONITORING: handleMonitoring(); break;
    case IRRIGATING: handleIrrigating(); break;
    case WAITING:    handleWaiting();    break;
  }
  
  // 24-hour automatic daily report
  if (millis() - dailyCheckTime >= MILLIS_PER_DAY) { // 24 hours
    logMoistureAndMetricsReport();
    resetDailyMetrics();
  }

  // 30-Day automatic monthly report
  if (millis() - monthlyCheckTime >= MILLIS_PER_30_DAYS) { // 30 days
    logMonthlyReport();
    resetMonthlyMetrics();
  }
}

// --- 7. ALL OTHER CUSTOM FUNCTIONS ---

/**
 * @brief Central handler for all serial commands.
 */
void checkSerialCommands() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    Serial.print("COMMAND RECEIVED: ");
    Serial.println(input);
    
    // 1. Check for Sensor Simulation (e.g., "S3:3500")
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
    // 2. Check for State Force (e.g., "STATE:MONITORING")
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
        timeToDry = millis() - wateringStopTime;
        digitalWrite(RELAY_PIN, PUMP_ON);
        Serial.println("SIM: State FORCED to IRRIGATING.");
      } else if (newState == "WAITING") {
        currentState = WAITING;
        wateringStopTime = millis();
        wateringDuration = millis() - pumpStartTime;
        timeToWet = millis() - pumpStartTime;
        digitalWrite(RELAY_PIN, PUMP_OFF);
        Serial.println("SIM: State FORCED to WAITING.");
      }
    }
    // 3. Check for Report commands
    else if (input == "REPORT") {
      Serial.println("SIM: Generating on-demand DAILY report...");
      logMoistureAndMetricsReport();
    }
    else if (input == "MONTHLY_REPORT") {
      Serial.println("SIM: Generating on-demand MONTHLY report...");
      logMonthlyReport();
    }
    // 4. Check for Timer Skip commands
    else if (input == "FORCE_CHECK") { lastCheckTime = 0; Serial.println("SIM: Forcing next MONITORING check."); }
    else if (input == "FORCE_IRRIGATE_CHECK") { pumpStartTime = 0; Serial.println("SIM: Forcing next IRRIGATING check."); }
    else if (input == "FORCE_WAIT_SKIP") { wateringStopTime = 0; Serial.println("SIM: Forcing WAITING skip."); }
  }
}

/**
 * @brief State 1: Checks sensors, looks for clusters.
 */
void handleMonitoring() {
  if (millis() - lastCheckTime >= CHECK_INTERVAL_MS) {
    lastCheckTime = millis();
    Serial.println("--- (Monitoring) ---");
    readAllSensors(); 
    if (checkForCluster()) {
      Serial.println("Dry cluster found!");
      Serial.println("State change: MONITORING -> IRRIGATING");
      timeToDry = millis() - wateringStopTime;
      float timeToDryHours = (float)timeToDry / 3600000.0;
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

/**
 * @brief State 2: Runs pump, checks if field is wet.
 */
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
      wateringDuration = millis() - pumpStartTime;
      float wateringDurationMin = (float)wateringDuration / 60000.0;
      timeToWet = wateringDuration;
      float timeToWetMin = wateringDurationMin;
      float litersUsed = wateringDurationMin * PUMP_FLOW_RATE_LITERS_PER_MINUTE;
      
      irrigationFrequencyCount++;
      totalWateringDurationMin += wateringDurationMin;
      totalTimeToWetMin += timeToWetMin;
      // --- FIX 2: Accumulate the total liters used ---
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

/**
 * @brief State 3: Cooldown period after watering.
 */
void handleWaiting() {
  // --- FIX 3: Corrected typo in constant name ---
  if (millis() - wateringStopTime >= POST_IRRIGATION_WAIT_TIME_MS) {
    Serial.println("Cooldown/Lockout complete.");
    Serial.println("State change: WAITING -> MONITORING");
    timeSinceLastIrrigation = millis() - wateringStopTime;
    lastCheckTime = millis();
    currentState = MONITORING;
  } else {
    if (millis() - lastCheckTime >= 60000) {
      lastCheckTime = millis();
      Serial.print("Post-irrigation cooldown... (Time remaining: ");
      // --- FIX 3: Corrected typo in constant name ---
      Serial.print((POST_IRRIGATION_WAIT_TIME_MS - (millis() - wateringStopTime)) / 60000);
      Serial.println(" min)");
    }
  }
}

/**
 * @brief Converts a raw 0-4095 ADC value to a 0-100% percentage.
 */
int convertToPercentage(int rawValue) {
  int percentage = map(rawValue, CALIBRATION_DRY, CALIBRATION_WET, 0, 100);
  return constrain(percentage, 0, 100);
}

/**
 * @brief Reads all sensors and updates their data in the map.
 */
void readAllSensors() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    int channel = sensorMap[i].channel;
    int value = readSensor(channel);
    sensorMap[i].moistureValue = value;
    sensorMap[i].isDry = (value > DRY_THRESHOLD);
    sensorMap[i].moisturePercentage = convertToPercentage(value);
    
    // Add to daily totals
    totalMoistureSumPerSensor[i] += sensorMap[i].moisturePercentage;
    totalMoistureReadingsPerSensor[i]++;
    totalFieldMoistureSum += sensorMap[i].moisturePercentage;
    totalFieldMoistureReadings++;
    
    // Print status
    Serial.print("  Sensor (Ch ");
    Serial.print(channel);
    Serial.print(" @ ");
    Serial.print(sensorMap[i].x);
    Serial.print(",");
    Serial.print(sensorMap[i].y);
    Serial.print("): ");
    Serial.print(value);
    Serial.print(" | ");
    Serial.print(sensorMap[i].moisturePercentage);
    Serial.print("%");
    if(sensorMap[i].isDry) Serial.println(" (DRY)");
    else Serial.println(" (WET)");
  }
}

/**
 * @brief Checks if ALL sensors are wetter than the WET_THRESHOLD.
 * (This logic is correct)
 */
bool checkIfAllSensorsWet() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    // If a sensor is "drier" (value >) than the threshold, we are not all wet
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
  return true; // All sensors are wet
}

/**
 * @brief Reads a single sensor from Mux, checking for simulation first.
 */
int readSensor(int channel) {
  
  if (channel < 0 || channel >= NUM_SENSORS) { // NUM_SENSORS is 8
    Serial.print("ERROR: Invalid channel request: ");
    Serial.println(channel);
    return CALIBRATION_DRY; // Return "dry" reading as safe fallback
  }

  if (simulatedValues[channel] != -1) {
    int simValue = simulatedValues[channel];
    simulatedValues[channel] = -1; // Use simulation value only once
    
    Serial.print("...Using simulated value ");
    Serial.print(simValue);
    Serial.print(" for Sensor ");
    Serial.println(channel);
    
    return simValue;
  }
  
  // Read from hardware
  digitalWrite(MUX_S0_PIN, bitRead(channel, 0));
  digitalWrite(MUX_S1_PIN, bitRead(channel, 1));
  digitalWrite(MUX_S2_PIN, bitRead(channel, 2));
  delay(10);
  return analogRead(MUX_COMMON_PIN);
}

/**
 * @brief Checks if a "cluster" of dry sensors exists.
 */
bool checkForCluster() {
  int dryCount = 0;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorMap[i].isDry) dryCount++;
  }
  if (dryCount < MIN_DRY_SENSORS_TO_TRIGGER) {
    return false; // Not enough dry sensors
  }
  
  // Check for neighbors
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorMap[i].isDry) {
      for (int j = i + 1; j < NUM_SENSORS; j++) {
        if (sensorMap[j].isDry) {
          long dx = sensorMap[i].x - sensorMap[j].x;
          long dy = sensorMap[i].y - sensorMap[j].y;
          long distanceSquared = (dx * dx) + (dy * dy);
          if (distanceSquared <= NEIGHBOR_DISTANCE_THRESHOLD_SQUARED) {
            return true; // Found a cluster
          }
        }
      }
    }
  }
  return false; // Found dry sensors, but they were all isolated
}

/**
 * @brief Prints a full daily report AND adds totals to the monthly counters.
 */
void logMoistureAndMetricsReport() {
  Serial.println("=========================================");
  Serial.println("--- 24-HOUR DAILY REPORT ---");
  Serial.println("=========================================");

  // 1. Per-Sensor Moisture Report
  Serial.println("--- Per-Sensor Daily Averages ---");
  for (int i = 0; i < NUM_SENSORS; i++) {
    float avgMoisture = 0.0;
    if (totalMoistureReadingsPerSensor[i] > 0) {
      avgMoisture = (float)totalMoistureSumPerSensor[i] / (float)totalMoistureReadingsPerSensor[i];
    }
    Serial.print("Sensor (Ch ");
    Serial.print(sensorMap[i].channel);
    Serial.print(" @ ");
    Serial.print(sensorMap[i].x);
    Serial.print(",");
    Serial.print(sensorMap[i].y);
    Serial.print("): ");
    Serial.print(avgMoisture);
    Serial.println("% avg");
  }

  // 2. Irrigation Metrics
  Serial.println("--- Irrigation Metrics (Totals & Avgs) ---");
  Serial.print("Irrigation Cycles Today: ");
  Serial.println(irrigationFrequencyCount);

  // Calculate field averages
  float avgFieldMoisture = 0.0;
  if(totalFieldMoistureReadings > 0) {
    avgFieldMoisture = (float)totalFieldMoistureSum / (float)totalFieldMoistureReadings;
  }
  
  float avgWateringDuration = 0.0;
  float avgTimeToWet = 0.0;
  float avgTimeToDry = 0.0;
  
  if (irrigationFrequencyCount > 0) {
    avgWateringDuration = totalWateringDurationMin / (float)irrigationFrequencyCount;
    avgTimeToWet = totalTimeToWetMin / (float)irrigationFrequencyCount;
    avgTimeToDry = totalTimeToDryHours / (float)irrigationFrequencyCount;
  }

  Serial.print("Avg. Field Moisture: ");
  Serial.print(avgFieldMoisture);
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
  
  // 3. Add daily totals to monthly totals
  monthlyIrrigationFrequency += irrigationFrequencyCount;
  monthlyTotalLitersUsed += totalLitersUsed;
  monthlyTotalWateringMin += totalWateringDurationMin;
  monthlyAvgTimeToWet += avgTimeToWet;
  monthlyAvgTimeToDry += avgTimeToDry;
  monthlyAvgFieldMoisture += avgFieldMoisture;
  monthlyReportCount++; // Increment count of daily reports
}

/**
 * @brief Prints a full 30-Day "Monthly" Report
 */
void logMonthlyReport() {
  Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  Serial.println("--- 30-DAY MONTHLY REPORT ---");
  Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

  Serial.print("Total Irrigation Cycles: ");
  Serial.println(monthlyIrrigationFrequency);
  
  Serial.print("Total Water Used (Est.): ");
  Serial.print(monthlyTotalLitersUsed);
  Serial.println(" Liters");

  Serial.print("Total Pump Run Time: ");
  Serial.print(monthlyTotalWateringMin);
  Serial.println(" minutes");

  Serial.println("--- Monthly Averages (Avg. of Dailies) ---");

  float finalAvgFieldMoisture = 0;
  float finalAvgTimeToWet = 0;
  float finalAvgTimeToDry = 0;

  if (monthlyReportCount > 0) {
    finalAvgFieldMoisture = monthlyAvgFieldMoisture / (float)monthlyReportCount;
    finalAvgTimeToWet = monthlyAvgTimeToWet / (float)monthlyReportCount;
    finalAvgTimeToDry = monthlyAvgTimeToDry / (float)monthlyReportCount;
  }

  Serial.print("Avg. Field Moisture: ");
  Serial.print(finalAvgFieldMoisture);
  Serial.println("%");

  Serial.print("Avg. Time to Wet: ");
  Serial.print(finalAvgTimeToWet);
  Serial.println(" minutes");

  Serial.print("Avg. Time to Dry: ");
  Serial.print(finalAvgTimeToDry);
  Serial.println(" hours");
  
  Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
}

/**
 * @brief Resets all daily counters.
 */
void resetDailyMetrics() {
  irrigationFrequencyCount = 0;
  totalWateringDurationMin = 0;
  totalLitersUsed = 0;
  totalTimeToWetMin = 0;
  totalTimeToDryHours = 0;
  totalFieldMoistureSum = 0;
  totalFieldMoistureReadings = 0;
  dailyCheckTime = millis(); // Reset 24h timer

  for (int i = 0; i < NUM_SENSORS; i++) {
    totalMoistureSumPerSensor[i] = 0;
    totalMoistureReadingsPerSensor[i] = 0;
  }
}

/**
 * @brief Resets all monthly counters.
 */
void resetMonthlyMetrics() {
  monthlyIrrigationFrequency = 0;
  monthlyTotalLitersUsed = 0;
  monthlyTotalWateringMin = 0;
  monthlyAvgTimeToWet = 0;
  monthlyAvgTimeToDry = 0;
  monthlyAvgFieldMoisture = 0;
  monthlyReportCount = 0;
  monthlyCheckTime = millis(); // Reset 30-day timer
}

/**
 * @brief Initializes the sensor (X,Y) map.
 */
void initializeSensorMap() {
  sensorMap[0] = {10, 10, 1, false, 0, 0}; 
  sensorMap[1] = {10, 20, 2, false, 0, 0}; 
  sensorMap[2] = {10, 30, 3, false, 0, 0};
  sensorMap[3] = {10, 40, 4, false, 0, 0};
  sensorMap[4] = {20, 10, 5, false, 0, 0};
  sensorMap[5] = {20, 20, 6, false, 0, 0};
  sensorMap[6] = {20, 30, 7, false, 0, 0}; 
  sensorMap[7] = {20, 40, 0, false, 0, 0};
}