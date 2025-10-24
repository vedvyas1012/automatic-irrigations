/*
 * AUTOMATIC MULTI-SENSOR IRRIGATION SYSTEM (ADVANCED STATE MACHINE)
 *
 * Implements all your new logic:
 * 1. STATE MACHINE: The system can be in 1 of 3 states:
 * - MONITORING: Checking sensors every 'CHECK_INTERVAL_MS'.
 * - IRRIGATING: Pump is ON. Stays on for at least 'MIN_PUMP_ON_TIME_MS'.
 * - WAITING: Pump is OFF. In a "cooldown" for 'POST_IRRIGATION_WAIT_TIME_MS'.
 *
 * 2. DUAL THRESHOLDS:
 * - DRY_THRESHOLD: Turns pump ON (e.g., value > 700).
 * - WET_THRESHOLD: Turns pump OFF (e.g., value < 400).
 *
 * 3. NON-BLOCKING TIMERS: Uses millis() to manage all timers at once.
 */

// --- PIN DEFINITIONS (from your diagram) ---
const int RELAY_PIN = 8;
const int MUX_COMMON_PIN = A0;
const int MUX_S0_PIN = 2;
const int MUX_S1_PIN = 3;
const int MUX_S2_PIN = 4;

// --- !!! CRITICAL SETTINGS - YOU MUST EDIT THESE !!! ---

// 1. CALIBRATION THRESHOLDS
//    (Assuming HIGHER value = Drier soil)
const int DRY_THRESHOLD = 700; // Turn ON when a cluster is *ABOVE* this
const int WET_THRESHOLD = 400; // Turn OFF when ALL sensors are *BELOW* this

// 2. TIMING (Your "Buffer" and "Interval" ideas)
//    (Values are in milliseconds: 1000 = 1 sec; 60000 = 1 min)
const unsigned long CHECK_INTERVAL_MS = 60000;    // (1 minute) How often to check soil
                                                  // when in MONITORING state.
const unsigned long MIN_PUMP_ON_TIME_MS = 300000; // (5 minutes) How long pump *must*
                                                  // run before we check if it's wet.
const unsigned long POST_IRRIGATION_WAIT_TIME_MS = 14400000; // (4 hours) "Lockout" period
                                                            // after watering.

// 3. ALGORITHM LOGIC (from previous code)
const int MIN_DRY_SENSORS_TO_TRIGGER = 3;
const int NEIGHBOR_DISTANCE_THRESHOLD = 10;
const long NEIGHBOR_DISTANCE_THRESHOLD_SQUARED = (long)NEIGHBOR_DISTANCE_THRESHOLD * NEIGHBOR_DISTANCE_THRESHOLD;

// 4. RELAY LOGIC (HIGH = ON is most common)
const int PUMP_ON = HIGH;
const int PUMP_OFF = LOW;

// --- SYSTEM STATE VARIABLES ---
enum SystemState { MONITORING, IRRIGATING, WAITING };
SystemState currentState = MONITORING;

unsigned long lastCheckTime = 0;
unsigned long pumpStartTime = 0;
unsigned long wateringStopTime = 0;

// Sensor "Graph" Map
struct SensorNode {
  int x, y, channel;
  bool isDry;
  int moistureValue;
};
const int NUM_SENSORS = 7;
SensorNode sensorMap[NUM_SENSORS];
// --- END OF SETTINGS ---


void setup() {
  Serial.begin(9600);
  Serial.println("Advanced Irrigation State Machine Initializing...");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MUX_S0_PIN, OUTPUT);
  pinMode(MUX_S1_PIN, OUTPUT);
  pinMode(MUX_S2_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, PUMP_OFF);

  // *** IMPORTANT: DEFINE YOUR SENSOR MAP HERE ***
  initializeSensorMap();
  
  Serial.println("Sensor map initialized. Current State: MONITORING");
  lastCheckTime = millis(); // Start the first check timer
}

void loop() {
  // The main loop is now a state manager
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

/**
 * STATE 1: MONITORING
 * - Checks sensors every 'CHECK_INTERVAL_MS'.
 * - If a dry cluster is found, switches to IRRIGATING.
 */
void handleMonitoring() {
  // Check if it's time to read sensors
  if (millis() - lastCheckTime >= CHECK_INTERVAL_MS) {
    lastCheckTime = millis(); // Reset check timer
    
    Serial.println("--- (Monitoring) ---");
    readAllSensors(); // Update sensorMap with new values

    if (checkForCluster()) {
      Serial.println("Dry cluster found!");
      Serial.println("State change: MONITORING -> IRRIGATING");
      
      digitalWrite(RELAY_PIN, PUMP_ON);
      pumpStartTime = millis(); // Log when the pump started
      currentState = IRRIGATING; // Change state
    } else {
      Serial.println("Field is OK. Next check in 1 min.");
    }
  }
  // If not time, do nothing
}

/**
 * STATE 2: IRRIGATING
 * - Pump is ON.
 * - Waits for 'MIN_PUMP_ON_TIME_MS' to pass.
 * - After that, checks if ALL sensors are *BELOW* the 'WET_THRESHOLD'.
 * - If they are, switches to WAITING.
 */
void handleIrrigating() {
  // Check 1: Has the minimum pump-on time passed?
  if (millis() - pumpStartTime < MIN_PUMP_ON_TIME_MS) {
    // Not yet. Pump *must* keep running.
    // We can print a status update
    if (millis() - lastCheckTime >= 10000) { // Update every 10 sec
        lastCheckTime = millis();
        Serial.print("Irrigating... (Min run time remaining: ");
        Serial.print((MIN_PUMP_ON_TIME_MS - (millis() - pumpStartTime)) / 1000);
        Serial.println(" sec)");
    }
    return; // Exit and do nothing else
  }

  // Check 2: Min time has passed. Now we can check if it's wet enough.
  // We'll check every 10 seconds.
  if (millis() - lastCheckTime >= 10000) {
    lastCheckTime = millis();
    Serial.println("Min pump time complete. Checking if field is wet...");
    
    readAllSensors();

    // Check if ALL sensors are below the WET threshold
    if (checkIfAllSensorsWet()) {
      Serial.println("Field is now fully wet.");
      Serial.println("State change: IRRIGATING -> WAITING");
      
      digitalWrite(RELAY_PIN, PUMP_OFF);
      wateringStopTime = millis(); // Log when watering stopped
      currentState = WAITING; // Change state
    } else {
      Serial.println("Field is not wet enough yet. Continuing to water.");
    }
  }
}

/**
 * STATE 3: WAITING (The "Lockout" Period)
 * - Pump is OFF.
 * - System does nothing for 'POST_IRRIGATION_WAIT_TIME_MS'.
 * - After time is up, switches back to MONITORING.
 */
void handleWaiting() {
  // Check if the lockout time has passed
  if (millis() - wateringStopTime >= POST_IRRIGATION_WAIT_TIME_MS) {
    // Lockout is over
    Serial.println("Cooldown/Lockout complete.");
    Serial.println("State change: WAITING -> MONITORING");
    
    lastCheckTime = millis(); // Reset the monitoring timer
    currentState = MONITORING; // Go back to checking
    
  } else {
    // Still in lockout. We can print a status.
    if (millis() - lastCheckTime >= 60000) { // Update every minute
      lastCheckTime = millis();
      Serial.print("Post-irrigation cooldown... (Time remaining: ");
      Serial.print((POST_IRRIGATION_WAIT_TIME_MS - (millis() - wateringStopTime)) / 60000);
      Serial.println(" min)");
    }
  }
}


// --- HELPER FUNCTIONS ---

/**
 * Updates the 'sensorMap' array with fresh readings from all 7 sensors.
 */
void readAllSensors() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    int channel = sensorMap[i].channel;
    int value = readSensor(channel);
    sensorMap[i].moistureValue = value;

    // Update the 'isDry' status
    sensorMap[i].isDry = (value > DRY_THRESHOLD);
  }
}

/**
 * Checks if ALL sensors are wetter than the WET_THRESHOLD.
 */
bool checkIfAllSensorsWet() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    // If we find EVEN ONE sensor that is NOT wet, we return false
    if (sensorMap[i].moistureValue > WET_THRESHOLD) {
      Serial.print("...Sensor (Ch ");
      Serial.print(sensorMap[i].channel);
      Serial.print(") is still too dry (Val: ");
      Serial.print(sensorMap[i].moistureValue);
      Serial.println(")");
      return false; 
    }
  }
  // If we get through the whole loop, it means all sensors are wet
  return true;
}


/**
 * @brief Reads a specific sensor channel from the 74HC4051 multiplexer.
 * @param channel The channel to read (1-7).
 * @return The analog value from the selected sensor (0-1023).
 */
int readSensor(int channel) {
  digitalWrite(MUX_S0_PIN, bitRead(channel, 0));
  digitalWrite(MUX_S1_PIN, bitRead(channel, 1));
  digitalWrite(MUX_S2_PIN, bitRead(channel, 2));
  delay(10); // Let the signal settle
  return analogRead(MUX_COMMON_PIN);
}

/**
 * @brief Checks if any two "dry" sensors are neighbors.
 * @return true if a cluster is found, false otherwise.
 */
bool checkForCluster() {
  int dryCount = 0;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorMap[i].isDry) dryCount++;
  }

  if (dryCount < MIN_DRY_SENSORS_TO_TRIGGER) {
    return false; // Not enough dry sensors to even check
  }
  
  // Now check for clusters
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
 * @brief Sets up the X,Y coordinates for all sensors.
 * !!! YOU MUST EDIT THIS FUNCTION TO MATCH YOUR FIELD LAYOUT !!!
 */
void initializeSensorMap() {
  // (This is just an example layout for 7 sensors)
  // sensorMap[array_index] = {X-pos, Y-pos, Mux-Channel, ...};
  sensorMap[0] = {10, 10, 1, false, 0}; 
  sensorMap[1] = {10, 20, 2, false, 0}; 
  sensorMap[2] = {10, 30, 3, false, 0};
  sensorMap[3] = {20, 10, 4, false, 0};
  sensorMap[4] = {20, 20, 5, false, 0};
  sensorMap[5] = {20, 30, 6, false, 0};
  sensorMap[6] = {30, 10, 7, false, 0}; 
}