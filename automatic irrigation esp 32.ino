/*
 * ==========================================================
 * ESP32 ADVANCED IRRIGATION SYSTEM
 * FINAL REFACTORED CODE (as of 2025-11-06)
 *
 * This code includes all features from Phase 1, 2, and 3,
 * and all bug fixes and robustness features from the code reviews.
 *
 * ... (Previous fixes)
 * - FIX 13 (BUG): Added missing prefs.begin()/end() calls for all
 * flash memory writes, preventing data save failures.
 * - FIX 14 (BUG): Added missing prefs.end() in setup() after loading.
 * ... (All other features/fixes included)
 */

// --- 0. INCLUDES ---
#include <Preferences.h> // For saving data to flash
#include <WiFi.h>        // For connecting to WiFi
#include <WebServer.h>   // For the web server
#include <ArduinoJson.h> // For sending data to the page
#include <SPIFFS.h>      // For reading/writing config.json

// --- 1. PIN DEFINITIONS (ESP32 GPIO) ---
const int RELAY_PIN = 23;
const int MUX_COMMON_PIN = 34;
const int MUX_S0_PIN = 19;
const int MUX_S1_PIN = 18;
const int MUX_S2_PIN = 5;

// --- Time conversion constants ---
const unsigned long MILLIS_PER_SECOND = 1000UL;
const unsigned long MILLIS_PER_MINUTE = 60000UL;
const unsigned long MILLIS_PER_HOUR   = 3600000UL;
const unsigned long MILLIS_PER_DAY    = 86400000UL;
const unsigned long MILLIS_PER_30_DAYS = 2592000000UL;

// --- 2. CRITICAL SETTINGS & THRESHOLDS ---
// These are DEFAULT values. They can be overridden by config.json from SPIFFS.
int CALIBRATION_DRY = 3500; // 0% moisture (in air)
int CALIBRATION_WET = 1200; // 100% moisture (in water)
int DRY_THRESHOLD = 3000; // Turn ON when a cluster is *ABOVE* this
int WET_THRESHOLD = 1500; // Turn OFF when ALL sensors are *BELOW* this
int LEAK_THRESHOLD_PERCENT = 98; // % for "super wet" leak check
int ADC_SAMPLES = 5; // Number of ADC samples for de-noising
int MUX_SETTLE_TIME_US = 800; // (microseconds)
int ADC_SAMPLE_DELAY_MS = 1; // (milliseconds)

// --- Compile-time safety checks (commented out since thresholds are now mutable) ---
// Note: Runtime validation will happen in loadConfigFromSPIFFS()
// static_assert(WET_THRESHOLD < DRY_THRESHOLD, "WET_THRESHOLD must be less than DRY_THRESHOLD");
// static_assert(ADC_SAMPLES > 0, "ADC_SAMPLES must be at least 1");

// --- TIMING ---
unsigned long CHECK_INTERVAL_MS = 1 * MILLIS_PER_MINUTE;
unsigned long MIN_PUMP_ON_TIME_MS = 5 * MILLIS_PER_MINUTE;
unsigned long MAX_PUMP_ON_TIME_MS = 1 * MILLIS_PER_HOUR;
unsigned long POST_IRRIGATION_WAIT_TIME_MS = 4 * MILLIS_PER_HOUR;
unsigned long IRRIGATING_CHECK_INTERVAL_MS = 10 * MILLIS_PER_SECOND;
const unsigned long FAULT_PRINT_INTERVAL_MS = 5 * MILLIS_PER_SECOND;

// --- ALERTS & REPORTING ---
const unsigned long MAX_TIME_UNEXPECTEDLY_DRY_MS = 7 * MILLIS_PER_DAY;
const unsigned long MAX_TIME_UNEXPECTEDLY_WET_MS = 3 * MILLIS_PER_DAY;
const float HEALTHY_WETTING_TIME_MIN = 10.0;
const float HEALTHY_DRYING_TIME_HOURS = 48.0;

// --- ALGORITHM LOGIC ---
const int MIN_DRY_SENSORS_TO_TRIGGER = 3; // Min cluster size
const int NEIGHBOR_DISTANCE_THRESHOLD = 15;
const long NEIGHBOR_DISTANCE_THRESHOLD_SQUARED = (long)NEIGHBOR_DISTANCE_THRESHOLD * NEIGHBOR_DISTANCE_THRESHOLD;
const int PUMP_ON = HIGH;
const int PUMP_OFF = LOW;
const float PUMP_FLOW_RATE_LITERS_PER_MINUTE = 20.0;

// --- 3. CUSTOM TYPES (struct, enum) ---
enum SystemState { MONITORING, IRRIGATING, WAITING, SYSTEM_FAULT };

struct SensorNode {
  int x, y, channel;
  bool isDry;
  int moistureValue;
  int moisturePercentage;
};

// --- 4. GLOBAL VARIABLES ---
Preferences prefs; // For saving data
SystemState currentState = MONITORING;
const int NUM_SENSORS = 8;
SensorNode sensorMap[NUM_SENSORS];
static_assert(NUM_SENSORS == 8, "NUM_SENSORS is 8, but array sizes or loops may be hardcoded. Update all arrays.");

// --- NEW: WiFi & Web Server ---
WebServer server(80); // Create the server object on port 80
const char* ssid = "Airtel_2 floor"; // Edit to match your WiFi
const char* password = "10122004";    // Edit to match your WiFi

// Per-State Timers
unsigned long monTick=0, irrTick=0, waitTick=0, faultTick=0;
unsigned long pumpStartTime = 0;
unsigned long wateringStopTime = 0;

// Phase 3 Diagnostics
int irrigationFailureCheckCount = 0;
long lastMoistureSum = 0;
bool unexpectedlyDryLatched = false;
int lastClusterSize = 0; // For logging

// Daily Metric Variables
unsigned long timeToWet = 0, timeToDry = 0, wateringDuration = 0, timeSinceLastIrrigation = 0;
unsigned long dailyCheckTime = 0;
unsigned long dailyIndex = 0; // Day counter for CSV
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

// File upload variable (global handle for chunked uploads)
File uploadFile;

// --- Forward Declarations ---
void forcePumpOff();
void setupWiFi();
void setupSPIFFS();
void loadConfigFromSPIFFS();
void createDefaultConfig();
void handleRoot();
void handleData();
void handleSerialCommand();
void handleUpload();
void handleFileUpload();

// --- 5. THE setup() FUNCTION ---
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Irrigation (Refactored) Initializing...");

  // --- NEW: Initialize SPIFFS first (before WiFi) ---
  setupSPIFFS();

  // --- NEW: Load configuration from SPIFFS ---
  loadConfigFromSPIFFS();

  // --- NEW: Connect to WiFi ---
  setupWiFi();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MUX_S0_PIN, OUTPUT);
  pinMode(MUX_S1_PIN, OUTPUT);
  pinMode(MUX_S2_PIN, OUTPUT);
  forcePumpOff(); // Ensure pump is off at boot

  // Set ADC attenuation for better 0-3.3V range
  #ifdef ARDUINO_ARCH_ESP32
    analogSetPinAttenuation(MUX_COMMON_PIN, ADC_11db); // ~3.3V full scale
  #endif

  initializeSensorMap();
  
  // Run Power-On-Self-Test for channel mapping
  runPowerOnSelfTest();

  // --- FIX 1 (Reviewer Bug 1): Added prefs.end() ---
  // Load saved monthly totals from flash memory
  prefs.begin("irrig", false); // Open "irrig" namespace
  monthlyTotalLitersUsed = prefs.getFloat("monLiters", 0.0);
  monthlyTotalWateringMin = prefs.getFloat("monMinutes", 0.0);
  monthlyIrrigationFrequency = prefs.getInt("monIrr", 0);
  monthlyAvgTimeToWet = prefs.getFloat("monAvgWet", 0.0);
  monthlyAvgTimeToDry = prefs.getFloat("monAvgDry", 0.0);
  monthlyAvgFieldMoisture = prefs.getFloat("monAvgMoist", 0.0);
  monthlyReportCount = prefs.getInt("monCnt", 0);
  dailyIndex = prefs.getULong("dailyIndex", 0); // Load day counter
  prefs.end(); // --- CLOSE the namespace ---
  Serial.print("Loaded previous monthly totals. Starting on Day ");
  Serial.println(dailyIndex);

  resetDailyMetrics();    // Initializes daily counters
  
  // Initialize simulation array
  for (int i = 0; i < NUM_SENSORS; i++) {
    simulatedValues[i] = -1;
  }
  
  // Initialize all per-state timers
  monTick = millis();
  irrTick = millis();
  waitTick = millis();
  faultTick = millis();
  wateringStopTime = millis();
  dailyCheckTime = millis();
  monthlyCheckTime = millis();

  // --- NEW: Define Server Routes (only if WiFi connected) ---
  if (WiFi.status() == WL_CONNECTED) {
    server.on("/", HTTP_GET, handleRoot);     // Send the HTML page
    server.on("/data", HTTP_GET, handleData); // Send the JSON data
    server.on("/serial", HTTP_GET, handleSerialCommand); // Handle web commands
    server.on("/upload", HTTP_POST, handleUpload, handleFileUpload); // Handle config upload

    server.begin();
    Serial.println("Web Server started.");
    Serial.print("Access dashboard at: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Web Server not started (no WiFi connection).");
  }

  // Print CSV header once on boot
  Serial.println("CSV_HEADER,dayIndex,millis,cycles,totalLiters,avgWatering,avgWet,avgDry,avgField%");
}

// --- 6. THE loop() FUNCTION ---
void loop() {
  // --- NEW: Handle web requests if WiFi is on ---
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  checkSerialCommands();

  switch (currentState) {
    case MONITORING: handleMonitoring(); break;
    case IRRIGATING: handleIrrigating(); break;
    case WAITING:    handleWaiting();    break;
    case SYSTEM_FAULT: handleSystemFault(); break;
  }
  
  // 24-hour automatic daily report
  if (millis() - dailyCheckTime >= MILLIS_PER_DAY) {
    logMoistureAndMetricsReport();
    logCsvDailySummary(); // Log CSV line
    dailyIndex++; // Increment day counter
    
    // --- FIX 2 (Reviewer Bug 2): Wrap save in begin()/end() ---
    prefs.begin("irrig", false); // Open namespace
    prefs.putULong("dailyIndex", dailyIndex); // Save incremented index
    prefs.end(); // Close namespace
    
    resetDailyMetrics();
  }

  // 30-Day automatic monthly report
  if (millis() - monthlyCheckTime >= MILLIS_PER_30_DAYS) {
    logMonthlyReport();
    resetMonthlyMetrics();
  }
}

// --- 7. ALL OTHER CUSTOM FUNCTIONS ---

// ============================================================
// PHASE 4: WiFi, SPIFFS, and Web Server Functions
// ============================================================

/**
 * @brief Connects to WiFi with timeout.
 */
void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int connectTries = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    connectTries++;
    if (connectTries > 30) { // 15-second timeout
      Serial.println("\nFailed to connect to WiFi after 15s.");
      Serial.println("Continuing WITHOUT WiFi. Irrigation logic will run.");
      return; // Exit gracefully - don't halt
    }
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/**
 * @brief Initializes the SPIFFS filesystem.
 */
void setupSPIFFS() {
  Serial.println("Initializing SPIFFS...");
  if (!SPIFFS.begin(true)) { // 'true' = format if mount fails
    Serial.println("ERROR: SPIFFS mount failed!");
    Serial.println("Continuing with default settings.");
    return;
  }
  Serial.println("SPIFFS mounted successfully.");

  // Create default config if it doesn't exist
  if (!SPIFFS.exists("/config.json")) {
    Serial.println("config.json not found. Creating default...");
    createDefaultConfig();
  }

  // List files (for debugging)
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  Serial.println("Files in SPIFFS:");
  while (file) {
    Serial.print("  - ");
    Serial.print(file.name());
    Serial.print(" (");
    Serial.print(file.size());
    Serial.println(" bytes)");
    file.close();  // Close file handle before getting next
    file = root.openNextFile();
  }
  root.close();  // Close root directory handle
}

/**
 * @brief Loads configuration from /config.json in SPIFFS.
 * If file doesn't exist or is invalid, uses default values.
 */
void loadConfigFromSPIFFS() {
  Serial.println("Loading config from SPIFFS...");

  if (!SPIFFS.exists("/config.json")) {
    Serial.println("config.json not found. Using default settings.");
    return;
  }

  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("ERROR: Failed to open config.json");
    return;
  }

  size_t size = configFile.size();
  if (size > 3072) {
    Serial.println("ERROR: config.json is too large!");
    configFile.close();
    return;
  }

  // Allocate buffer and read file
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();

  // Parse JSON
  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, buf.get());

  if (error) {
    Serial.print("ERROR: Failed to parse config.json: ");
    Serial.println(error.c_str());
    return;
  }

  // Load threshold values (with defaults if keys don't exist)
  if (doc.containsKey("CALIBRATION_DRY")) CALIBRATION_DRY = doc["CALIBRATION_DRY"];
  if (doc.containsKey("CALIBRATION_WET")) CALIBRATION_WET = doc["CALIBRATION_WET"];
  if (doc.containsKey("DRY_THRESHOLD")) DRY_THRESHOLD = doc["DRY_THRESHOLD"];
  if (doc.containsKey("WET_THRESHOLD")) WET_THRESHOLD = doc["WET_THRESHOLD"];
  if (doc.containsKey("LEAK_THRESHOLD_PERCENT")) LEAK_THRESHOLD_PERCENT = doc["LEAK_THRESHOLD_PERCENT"];
  if (doc.containsKey("ADC_SAMPLES")) ADC_SAMPLES = max(1, (int)doc["ADC_SAMPLES"]);
  if (doc.containsKey("MUX_SETTLE_TIME_US")) MUX_SETTLE_TIME_US = doc["MUX_SETTLE_TIME_US"];
  if (doc.containsKey("ADC_SAMPLE_DELAY_MS")) ADC_SAMPLE_DELAY_MS = doc["ADC_SAMPLE_DELAY_MS"];

  // Load timing parameters if present
  if (doc.containsKey("CHECK_INTERVAL_MS")) CHECK_INTERVAL_MS = doc["CHECK_INTERVAL_MS"];
  if (doc.containsKey("MIN_PUMP_ON_TIME_MS")) MIN_PUMP_ON_TIME_MS = doc["MIN_PUMP_ON_TIME_MS"];
  if (doc.containsKey("MAX_PUMP_ON_TIME_MS")) MAX_PUMP_ON_TIME_MS = doc["MAX_PUMP_ON_TIME_MS"];
  if (doc.containsKey("POST_IRRIGATION_WAIT_TIME_MS")) POST_IRRIGATION_WAIT_TIME_MS = doc["POST_IRRIGATION_WAIT_TIME_MS"];
  if (doc.containsKey("IRRIGATING_CHECK_INTERVAL_MS")) IRRIGATING_CHECK_INTERVAL_MS = doc["IRRIGATING_CHECK_INTERVAL_MS"];

  // Runtime validation for thresholds
  if (WET_THRESHOLD >= DRY_THRESHOLD) {
    Serial.println("WARNING: WET_THRESHOLD >= DRY_THRESHOLD in config! Using defaults.");
    WET_THRESHOLD = 1500;
    DRY_THRESHOLD = 3000;
  }

  // Runtime validation for timing parameters
  if (CHECK_INTERVAL_MS < 1000) {
    Serial.println("WARNING: CHECK_INTERVAL_MS < 1s! Using 60000ms.");
    CHECK_INTERVAL_MS = 60000;
  }
  if (IRRIGATING_CHECK_INTERVAL_MS < 1000) {
    Serial.println("WARNING: IRRIGATING_CHECK_INTERVAL_MS < 1s! Using 10000ms.");
    IRRIGATING_CHECK_INTERVAL_MS = 10000;
  }
  if (MIN_PUMP_ON_TIME_MS >= MAX_PUMP_ON_TIME_MS) {
    Serial.println("WARNING: MIN_PUMP >= MAX_PUMP time! Using defaults.");
    MIN_PUMP_ON_TIME_MS = 5 * MILLIS_PER_MINUTE;
    MAX_PUMP_ON_TIME_MS = 1 * MILLIS_PER_HOUR;
  }
  if (POST_IRRIGATION_WAIT_TIME_MS < MILLIS_PER_MINUTE) {
    Serial.println("WARNING: POST_IRRIGATION_WAIT < 1min! Using 4 hours.");
    POST_IRRIGATION_WAIT_TIME_MS = 4 * MILLIS_PER_HOUR;
  }

  Serial.println("Configuration loaded successfully:");
  Serial.print("  DRY_THRESHOLD: "); Serial.println(DRY_THRESHOLD);
  Serial.print("  WET_THRESHOLD: "); Serial.println(WET_THRESHOLD);
  Serial.print("  CALIBRATION_DRY: "); Serial.println(CALIBRATION_DRY);
  Serial.print("  CALIBRATION_WET: "); Serial.println(CALIBRATION_WET);
}

/**
 * @brief Creates a default config.json file if it doesn't exist
 */
void createDefaultConfig() {
  const char* defConfig = R"({
  "CALIBRATION_DRY": 3500,
  "CALIBRATION_WET": 1200,
  "DRY_THRESHOLD": 3000,
  "WET_THRESHOLD": 1500,
  "LEAK_THRESHOLD_PERCENT": 98,
  "ADC_SAMPLES": 5,
  "MUX_SETTLE_TIME_US": 800,
  "ADC_SAMPLE_DELAY_MS": 1,
  "CHECK_INTERVAL_MS": 60000,
  "MIN_PUMP_ON_TIME_MS": 300000,
  "MAX_PUMP_ON_TIME_MS": 3600000,
  "POST_IRRIGATION_WAIT_TIME_MS": 14400000,
  "IRRIGATING_CHECK_INTERVAL_MS": 10000
})";

  File file = SPIFFS.open("/config.json", "w");
  if (file) {
    file.print(defConfig);
    file.close();
    Serial.println("Default config.json created successfully.");
  } else {
    Serial.println("ERROR: Failed to create default config.json");
  }
}

/**
 * @brief Handles requests to the main web page (the root '/')
 */
void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

/**
 * @brief Handles requests for live data (at '/data')
 */
void handleData() {
  StaticJsonDocument<2048> doc;

  // Add the current system state
  switch(currentState) {
    case MONITORING: doc["state"] = "MONITORING"; break;
    case IRRIGATING: doc["state"] = "IRRIGATING"; break;
    case WAITING:    doc["state"] = "WAITING"; break;
    case SYSTEM_FAULT: doc["state"] = "SYSTEM_FAULT"; break;
  }

  // --- FIX: Add IP address ---
  doc["ip"] = WiFi.localIP().toString();

  // Create a JSON array for the sensors
  JsonArray sensors = doc.createNestedArray("sensors");

  for (int i = 0; i < NUM_SENSORS; i++) {
    JsonObject s = sensors.createNestedObject();
    s["channel"] = sensorMap[i].channel;
    s["x"] = sensorMap[i].x;
    s["y"] = sensorMap[i].y;
    s["val"] = sensorMap[i].moistureValue;
    s["pct"] = sensorMap[i].moisturePercentage;
    s["isDry"] = sensorMap[i].isDry;
  }

  // Serialize the JSON to a string
  String output;
  serializeJson(doc, output);

  server.send(200, "application/json", output);
}

/**
 * @brief Handles serial commands sent from the web page.
 */
void handleSerialCommand() {
  if (server.hasArg("cmd")) {
    String cmd = server.arg("cmd");
    Serial.print("Web command received: ");
    Serial.println(cmd);

    // Pass the command to our existing serial handler
    if (cmd == "REPORT") {
      logMoistureAndMetricsReport();
      logCsvDailySummary();
    }
    if (cmd == "MONTHLY_REPORT") logMonthlyReport();

    server.send(200, "text/plain", "Command executed. Check Serial Monitor for output.");
  } else {
    server.send(400, "text/plain", "Missing 'cmd' parameter");
  }
}

/**
 * @brief Handles the completion of a file upload.
 */
void handleUpload() {
  server.send(200, "text/plain", "Config uploaded successfully! ESP32 will reload config...");
  delay(100); // Let response send
  loadConfigFromSPIFFS(); // Reload the new config
}

/**
 * @brief Handles the actual file upload data.
 */
void handleFileUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.print("Receiving file: ");
    Serial.println(upload.filename);

    // Validate filename
    if (String(upload.filename) != "config.json") {
      Serial.println("ERROR: Rejecting upload - filename must be config.json");
      return;
    }

    // Remove old file and open new one
    SPIFFS.remove("/config.json");
    uploadFile = SPIFFS.open("/config.json", "w");
    if (!uploadFile) {
      Serial.println("ERROR: Failed to open file for writing!");
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write chunk to global file handle
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    // Close file and report completion
    if (uploadFile) {
      uploadFile.close();
      Serial.print("Upload complete: ");
      Serial.print(upload.totalSize);
      Serial.println(" bytes");
    }
  }
}

// --- HTML Page in PROGMEM (Flash Memory) ---
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Irrigation Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 0 20px; background-color: #f4f4f4; }
    h1 { color: #2c3e50; text-align: center; }
    .container { max-width: 800px; margin: 20px auto; padding: 20px; background-color: #fff; border-radius: 8px; box-shadow: 0 4px 12px rgba(0,0,0,0.05); }
    .nav { text-align: center; margin-bottom: 20px; }
    .btn { padding: 10px 20px; font-size: 1em; color: #fff; background-color: #3498db; border: none; border-radius: 5px; cursor: pointer; text-decoration: none; margin: 5px; }
    .btn-upload { background-color: #f39c12; }
    #status { padding: 20px; margin-bottom: 20px; border-radius: 8px; font-weight: bold; font-size: 1.2em; text-align: center; }
    #grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin-bottom: 20px; }
    .sensor { padding: 15px; border-radius: 8px; background: #ecf0f1; border: 2px solid #bdc3c7; }
    .sensor-title { font-weight: bold; }
    .sensor-val { font-size: 1.5em; }
    .sensor-pct { color: #7f8c8d; }
    h2 { border-bottom: 2px solid #eee; padding-bottom: 5px; }
  </style>
</head>
<body>
  <h1>ESP32 Irrigation Control</h1>

  <div class="container">
    <div class="nav">
      <button class="btn" onclick="showPage('report')">View Report</button>
      <button class="btn btn-upload" onclick="showPage('upload')">Upload Config</button>
    </div>

    <div id="page-report">
      <h2>Live Status</h2>
      <div id="status">Loading...</div>
      <div id="grid"></div>

      <h2>Serial Reports</h2>
      <button class="btn" onclick="sendCommand('REPORT')">Get Daily Report</button>
      <button class="btn" onclick="sendCommand('MONTHLY_REPORT')">Get Monthly Report</button>
      <pre id="serial-output" style="background-color: #333; color: #fff; padding: 10px; border-radius: 5px; max-height: 300px; overflow-y: auto;">Serial report output will appear here...</pre>
    </div>

    <div id="page-upload" style="display:none;">
      <h2>Upload Configuration</h2>
      <form id="upload-form" enctype="multipart/form-data" method="POST" action="/upload">
        <label for="configFile">Select config.json:</label><br>
        <input type="file" id="configFile" name="config" accept=".json"><br><br>
        <input type="submit" value="Upload" class="btn">
      </form>
      <p id="upload-status" style="margin-top: 15px;"><i>Waiting for file...</i></p>
    </div>
  </div>

  <script>
    function showPage(pageId) {
      document.getElementById('page-report').style.display = (pageId === 'report') ? 'block' : 'none';
      document.getElementById('page-upload').style.display = (pageId === 'upload') ? 'block' : 'none';
    }

    function updateData() {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          // Update Status
          const statusDiv = document.getElementById('status');
          statusDiv.innerHTML = 'System State: ' + data.state + ' | IP: ' + data.ip;
          if (data.state === 'IRRIGATING') { statusDiv.style.background = '#2ecc71'; statusDiv.style.color = '#fff'; }
          else if (data.state === 'SYSTEM_FAULT') { statusDiv.style.background = '#e74c3c'; statusDiv.style.color = '#fff'; }
          else { statusDiv.style.background = '#ecf0f1'; statusDiv.style.color = '#2c3e50'; }

          // Update Sensor Grid
          const grid = document.getElementById('grid');
          grid.innerHTML = '';
          data.sensors.forEach(s => {
            const div = document.createElement('div');
            div.className = 'sensor';
            div.innerHTML = `<span class="sensor-title">S-${s.channel} (${s.x},${s.y})</span><br>
                           <span class="sensor-val">${s.pct}%</span><br>
                           <span class="sensor-pct">(${s.val})</span>`;

            if (s.isDry) { div.style.borderColor = '#e74c3c'; div.style.background = '#fbe9e7'; }
            else if (s.pct > 80) { div.style.borderColor = '#2980b9'; div.style.background = '#eaf5fb'; }
            else { div.style.borderColor = '#bdc3c7'; div.style.background = '#ecf0f1'; }
            grid.appendChild(div);
          });
        })
        .catch(error => console.error('Error fetching data:', error));
    }

    function sendCommand(cmd) {
      fetch('/serial?cmd=' + cmd)
        .then(response => response.text())
        .then(text => {
          document.getElementById('serial-output').textContent = text;
        })
        .catch(error => {
          document.getElementById('serial-output').textContent = 'Error: ' + error;
        });
    }

    // Initial display and auto-update
    showPage('report');
    updateData();
    setInterval(updateData, 3000);

    // Handle File Upload Form Submission
    document.getElementById('upload-form').addEventListener('submit', function(e) {
        e.preventDefault();
        const fileInput = document.getElementById('configFile');
        const uploadStatus = document.getElementById('upload-status');

        if (fileInput.files.length === 0) {
            uploadStatus.textContent = 'Please select a file.';
            return;
        }

        uploadStatus.textContent = 'Uploading...';

        const file = fileInput.files[0];
        if (file.name !== "config.json") {
             uploadStatus.textContent = 'Error: File must be named config.json';
             return;
        }

        const formData = new FormData();
        formData.append('config', file);

        fetch('/upload', {
            method: 'POST',
            body: formData
        })
        .then(response => response.text())
        .then(text => {
            uploadStatus.textContent = text;
        })
        .catch(error => {
            uploadStatus.textContent = 'Upload Failed: ' + error;
        });
    });
  </script>
</body>
</html>
)rawliteral";

// ============================================================
// END OF PHASE 4 FUNCTIONS
// ============================================================

/**
 * @brief Central handler for all serial commands.
 */
void checkSerialCommands() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    Serial.print("COMMAND RECEIVED: ");
    Serial.println(input);
    
    // Check for "STATE:" first to prevent collision with "S:"
    if (input.startsWith("STATE:")) {
      String newState = input.substring(6);
      newState.trim();
      newState.toUpperCase();
      if (newState == "MONITORING") {
        currentState = MONITORING;
        monTick = millis();
        Serial.println("SIM: State FORCED to MONITORING.");
      } else if (newState == "IRRIGATING") {
        currentState = IRRIGATING;
        pumpStartTime = millis();
        timeToDry = millis() - wateringStopTime;
        digitalWrite(RELAY_PIN, PUMP_ON);
        unexpectedlyDryLatched = false;
        irrigationFailureCheckCount = 0;
        lastMoistureSum = 0;
        Serial.println("SIM: State FORCED to IRRIGATING.");
      } else if (newState == "WAITING") {
        currentState = WAITING;
        wateringStopTime = millis();
        wateringDuration = millis() - pumpStartTime;
        timeToWet = millis() - pumpStartTime;
        forcePumpOff();
        Serial.println("SIM: State FORCED to WAITING.");
      }
    }
    // Check for Sensor Simulation (e.g., "S3:3500")
    else if (input.startsWith("S")) {
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
        } else {
          Serial.println("SIM: Invalid sensor index.");
        }
      }
    }
    // Check for Report commands
    else if (input == "REPORT") {
      Serial.println("SIM: Generating on-demand DAILY report...");
      logMoistureAndMetricsReport();
      logCsvDailySummary();
    }
    else if (input == "MONTHLY_REPORT") {
      Serial.println("SIM: Generating on-demand MONTHLY report...");
      logMonthlyReport();
    }
    // Check for Timer Skip / Test commands
    else if (input == "FORCE_RESET") {
      Serial.println("SIM: Fault cleared, returning to MONITORING.");
      forcePumpOff();
      currentState = MONITORING;
      monTick = millis();
      unexpectedlyDryLatched = false;
      irrigationFailureCheckCount = 0;
      lastMoistureSum = 0;
    }
    else if (input == "TEST_PUMP_30S") {
      Serial.println("SIM: Running pump test for 30s...");
      digitalWrite(RELAY_PIN, PUMP_ON);
      unsigned long t0 = millis();
      while (millis() - t0 < 30 * MILLIS_PER_SECOND) { delay(10); }
      forcePumpOff();
      Serial.println("SIM: Pump test complete.");
    }
    else if (input == "FORCE_CHECK") { monTick = 0; Serial.println("SIM: Forcing next MONITORING check."); }
    else if (input == "FORCE_IRRIGATE_CHECK") { irrTick = 0; Serial.println("SIM: Forcing next IRRIGATING check."); }
    // NOTE: Setting wateringStopTime = 0 can intentionally trip "unexpectedly dry/wet" alerts.
    else if (input == "FORCE_WAIT_SKIP") { wateringStopTime = 0; Serial.println("SIM: Forcing WAITING skip."); }
  }
}

/**
 * @brief State 1: Checks sensors, looks for clusters, checks for alerts.
 */
void handleMonitoring() {
  if (millis() - monTick >= CHECK_INTERVAL_MS) {
    monTick = millis();
    Serial.println("--- (Monitoring) ---");
    readAllSensors(); // This now calls printSensorReadings()
    
    // --- Leak Detection Logic ---
    for (int i = 0; i < NUM_SENSORS; i++) {
      if (sensorMap[i].moisturePercentage >= LEAK_THRESHOLD_PERCENT) {
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        Serial.print("!!! WARNING: POTENTIAL LEAK DETECTED !!! ");
        Serial.print("Sensor (Ch ");
        Serial.print(sensorMap[i].channel);
        Serial.print(" @ ");
        Serial.print(sensorMap[i].x);
        Serial.print(",");
        Serial.print(sensorMap[i].y);
        Serial.print(") is ");
        Serial.print(sensorMap[i].moisturePercentage);
        Serial.println("% wet while pump is OFF.");
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
      }
    }

    bool dryCluster = checkForCluster();
    unsigned long timeSinceLastWatering = millis() - wateringStopTime;

    // --- "Unexpectedly Dry" Latch Logic ---
    if (!unexpectedlyDryLatched &&
        timeSinceLastWatering >= MAX_TIME_UNEXPECTEDLY_DRY_MS &&
        dryCluster) 
    {
      unexpectedlyDryLatched = true; // Set the latch
      Serial.println("!!! ALERT: UNEXPECTEDLY DRY !!!");
      Serial.println("Dry cluster persisted beyond max time since last irrigation.");
      Serial.println("System logic may be failing to water. Check system.");
    }
    // Clear the latch once condition resolves
    if (unexpectedlyDryLatched && !dryCluster) {
      Serial.println("INFO: 'Unexpectedly Dry' condition cleared.");
      unexpectedlyDryLatched = false;
    }

    // --- "Unexpectedly Wet" Alert Logic ---
    if (timeSinceLastWatering >= MAX_TIME_UNEXPECTEDLY_WET_MS && isFieldWet()) { 
        Serial.println("!!! ALERT: UNEXPECTEDLY WET !!!");
        Serial.println("Field has stayed wet for max time limit. Check drainage.");
    }
    // --- End of Alert Logic ---


    if (dryCluster) {
      Serial.println("Dry cluster found!");
      Serial.print("CLUSTER_SIZE: ");
      Serial.println(lastClusterSize);
      Serial.println("State change: MONITORING -> IRRIGATING");
      
      timeToDry = millis() - wateringStopTime;
      float timeToDryHours = (float)timeToDry / (float)MILLIS_PER_HOUR;
      totalTimeToDryHours += timeToDryHours;
      Serial.print("METRIC: Field took ");
      Serial.print(timeToDryHours);
      Serial.println(" hours to dry.");
      digitalWrite(RELAY_PIN, PUMP_ON);
      pumpStartTime = millis();
      currentState = IRRIGATING;
      
      unexpectedlyDryLatched = false;
      irrigationFailureCheckCount = 0;
      lastMoistureSum = 0;
    } else {
      Serial.println("Field is OK. Next check in 1 min.");
    }
  }
}

/**
 * @brief State 2: Runs pump, checks for failures, checks if field is wet.
 */
void handleIrrigating() {
  
  // --- Check 1: Max pump time (Irrigation Failure) ---
  if (millis() - pumpStartTime >= MAX_PUMP_ON_TIME_MS) {
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("!!! ALERT: IRRIGATION FAILURE (PUMP_ON_MAX) !!!");
    Serial.println("Pump has run for max time but field is not wet.");
    Serial.println("Shutting down system. Check pump/well.");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    
    forcePumpOff();
    currentState = SYSTEM_FAULT;
    return;
  }
  
  // --- Check 2: Min pump time (Wait before checking) ---
  if (millis() - pumpStartTime < MIN_PUMP_ON_TIME_MS) {
    if (millis() - irrTick >= IRRIGATING_CHECK_INTERVAL_MS) {
        irrTick = millis();
        Serial.print("Irrigating... (Min run time remaining: ");
        long remIrr = (long)MIN_PUMP_ON_TIME_MS - (long)(millis() - pumpStartTime);
        if (remIrr < 0) remIrr = 0;
        Serial.print(remIrr / (long)MILLIS_PER_SECOND);
        Serial.println(" sec)");
    }
    return;
  }
  
  // --- Check 3: Check if wet (Runs every 10s after min time) ---
  if (millis() - irrTick >= IRRIGATING_CHECK_INTERVAL_MS) {
    irrTick = millis();
    Serial.println("Min pump time complete. Checking if field is wet...");
    
    readAllSensors(); 

    long currentMoistureSum = 0;
    for (int i = 0; i < NUM_SENSORS; i++) {
        currentMoistureSum += sensorMap[i].moistureValue;
    }
    
    // --- Advanced Irrigation Failure Logic (No Moisture Change) ---
    if (lastMoistureSum != 0) {
      if (currentMoistureSum >= lastMoistureSum) { 
        irrigationFailureCheckCount++;
        Serial.print("!!! WARNING: Soil moisture is not decreasing! (Check ");
        Serial.print(irrigationFailureCheckCount);
        Serial.println(")");
        
        if (irrigationFailureCheckCount >= 3) {
          Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
          Serial.println("!!! ALERT: IRRIGATION FAILURE (NO MOISTURE CHANGE) !!!");
          Serial.println("Pump is ON but moisture is not increasing. Check pump/well.");
          Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
          
          forcePumpOff();
          currentState = SYSTEM_FAULT;
          return;
        }
      } else {
        irrigationFailureCheckCount = 0;
      }
    }
    lastMoistureSum = currentMoistureSum;
    // --- End of Advanced Failure Logic ---

    
    // --- Final Decision ---
    if (checkIfAllSensorsWet()) { 
      Serial.println("Field is now fully wet.");
      Serial.println("State change: IRRIGATING -> WAITING");
      
      // Log all metrics
      wateringDuration = millis() - pumpStartTime;
      float wateringDurationMin = (float)wateringDuration / (float)MILLIS_PER_MINUTE;
      timeToWet = wateringDuration;
      float timeToWetMin = wateringDurationMin;
      float litersUsed = wateringDurationMin * PUMP_FLOW_RATE_LITERS_PER_MINUTE;
      
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
      
      // Stop the pump and change state
      forcePumpOff();
      wateringStopTime = millis();
      currentState = WAITING;
      
      // Reset failure trackers on success
      lastMoistureSum = 0;
      irrigationFailureCheckCount = 0;
    } else {
      Serial.println("Field is not wet enough yet. Continuing to water.");
    }
  }
}

/**
 * @brief State 3: Cooldown period after watering.
 */
void handleWaiting() {
  if (millis() - wateringStopTime >= POST_IRRIGATION_WAIT_TIME_MS) {
    Serial.println("Cooldown/Lockout complete.");
    Serial.println("State change: WAITING -> MONITORING");
    timeSinceLastIrrigation = millis() - wateringStopTime;
    monTick = millis();
    currentState = MONITORING;
  } else {
    if (millis() - waitTick >= MILLIS_PER_MINUTE) {
      waitTick = millis();
      Serial.print("Post-irrigation cooldown... (Time remaining: ");
      long remWait = (long)POST_IRRIGATION_WAIT_TIME_MS - (long)(millis() - wateringStopTime);
      if (remWait < 0) remWait = 0;
      Serial.print(remWait / (long)MILLIS_PER_MINUTE);
      Serial.println(" min)");
    }
  }
}

/**
 * @brief State 4: System Failure
 */
void handleSystemFault() {
  // Defensive pump-off
  forcePumpOff();

  // Print alert every 5 seconds
  if (millis() - faultTick >= FAULT_PRINT_INTERVAL_MS) {
    faultTick = millis();
    Serial.println("!!! SYSTEM IN FAULT STATE - 'FORCE_RESET' REQUIRED !!!");
  }
}

/**
 * @brief NEW: Safe, central function to turn off the pump.
 */
void forcePumpOff() {
  digitalWrite(RELAY_PIN, PUMP_OFF);
}

/**
 * @brief Converts a raw 0-4095 ADC value to a 0-100% percentage.
 * (Note: Higher ADC value = Drier soil, so the map() inverts the range)
 */
int convertToPercentage(int rawValue) {
  // Clip input to valid calibration range first (prevents out-of-range mapping)
  long clipped = rawValue;
  if (clipped < CALIBRATION_WET) clipped = CALIBRATION_WET;
  if (clipped > CALIBRATION_DRY) clipped = CALIBRATION_DRY;

  // Map clipped value (Higher ADC = Drier, so invert)
  int percentage = map(clipped, (long)CALIBRATION_DRY, (long)CALIBRATION_WET, 0L, 100L);
  return constrain(percentage, 0, 100);
}

/**
 * @brief (Internal) Prints the sensor readings from the map.
 */
void printSensorReadings() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print("  Sensor (Ch ");
    Serial.print(sensorMap[i].channel);
    Serial.print(" @ ");
    Serial.print(sensorMap[i].x);
    Serial.print(",");
    Serial.print(sensorMap[i].y);
    Serial.print("): ");
    Serial.print(sensorMap[i].moistureValue);
    Serial.print(" | ");
    Serial.print(sensorMap[i].moisturePercentage);
    Serial.print("%");
    if(sensorMap[i].isDry) Serial.println(" (DRY)");
    else Serial.println(" (WET)");
  }
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
  }
  // Print all readings at once
  printSensorReadings();
}

/**
 * @brief Checks if ALL sensors are wetter than the WET_THRESHOLD.
 */
bool checkIfAllSensorsWet() {
  int stillDryCount = 0;
  int lastDrySensorChannel = -1;
  int lastDrySensorX = -1, lastDrySensorY = -1;

  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorMap[i].moistureValue > WET_THRESHOLD) {
      stillDryCount++;
      lastDrySensorChannel = sensorMap[i].channel;
      lastDrySensorX = sensorMap[i].x;
      lastDrySensorY = sensorMap[i].y;
    }
  }

  // Clog Detection Logic
  if (stillDryCount == 1) {
    Serial.print("!!! WARNING: POTENTIAL CLOG DETECTED !!! ");
    Serial.print("Sensor (Ch ");
    Serial.print(lastDrySensorChannel);
    Serial.print(" @ ");
    Serial.print(lastDrySensorX);
    Serial.print(",");
    Serial.print(lastDrySensorY);
    Serial.println(") is still dry, but all other sensors are wet.");
  }

  return (stillDryCount == 0);
}

/**
 * @brief Reads a single sensor from Mux, checking for simulation first.
 */
int readSensor(int channel) {
  // Bounds check
  if (channel < 0 || channel >= NUM_SENSORS) {
    Serial.print("ERROR: Invalid channel request: ");
    Serial.println(channel);
    return CALIBRATION_DRY;
  }
  
  if (simulatedValues[channel] != -1) {
    int simValue = simulatedValues[channel];
    simulatedValues[channel] = -1;
    
    Serial.print("...Using simulated value ");
    Serial.print(simValue);
    Serial.print(" for Sensor ");
    Serial.println(channel);
    
    return simValue;
  }
  
  // Select hardware channel
  digitalWrite(MUX_S0_PIN, bitRead(channel, 0));
  digitalWrite(MUX_S1_PIN, bitRead(channel, 1));
  digitalWrite(MUX_S2_PIN, bitRead(channel, 2));
  delayMicroseconds(MUX_SETTLE_TIME_US); // Settle the multiplexer
  
  // Take ADC_SAMPLES for de-noising
  long acc = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    acc += analogRead(MUX_COMMON_PIN);
    delay(ADC_SAMPLE_DELAY_MS);
  }
  return (int)(acc / (long)ADC_SAMPLES);
}

/**
 * @brief Helper function to check if the field is "wet".
 * "Wet" means "wet enough" (no 3+ dry connected nodes).
 */
bool isFieldWet() {
  return !checkForCluster();
}

/**
* @brief Advanced clustering check (BFS algorithm).
* Checks for a "connected component" of 3+ dry sensors.
*/
bool checkForCluster() {
  bool dry[NUM_SENSORS];
  for (int i = 0; i < NUM_SENSORS; i++) dry[i] = sensorMap[i].isDry;

  // Quick exit
  int dryTotal = 0;
  for (int i = 0; i < NUM_SENSORS; i++) if (dry[i]) dryTotal++;
  if (dryTotal < MIN_DRY_SENSORS_TO_TRIGGER) {
    lastClusterSize = 0; // No cluster
    return false;
  }

  bool visited[NUM_SENSORS] = {false};

  for (int start = 0; start < NUM_SENSORS; start++) {
    if (!dry[start] || visited[start]) continue;

    // BFS/DFS to count connected dry nodes
    int queue[NUM_SENSORS]; int qh = 0, qt = 0;
    queue[qt++] = start; visited[start] = true;
    int compSize = 0;

    while (qh < qt) {
      int u = queue[qh++]; compSize++;

      for (int v = 0; v < NUM_SENSORS; v++) {
        if (u == v || !dry[v] || visited[v]) continue;
        long dx = sensorMap[u].x - sensorMap[v].x;
        long dy = sensorMap[u].y - sensorMap[v].y;
        long d2 = dx*dx + dy*dy;
        if (d2 <= NEIGHBOR_DISTANCE_THRESHOLD_SQUARED) {
          visited[v] = true;
          queue[qt++] = v;
        }
      }
    }

    if (compSize >= MIN_DRY_SENSORS_TO_TRIGGER) {
      lastClusterSize = compSize; // Log cluster size
      return true;
    }
  }
  
  lastClusterSize = 0; // No clusters were large enough
  return false;
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
  
  // 3. Soil Health Report Card
  Serial.println("--- Soil Health Report Card ---");
  if (irrigationFrequencyCount > 0) {
    if (avgTimeToWet > HEALTHY_WETTING_TIME_MIN) {
      Serial.print("DIAGNOSIS: POOR INFILTRATION. Soil took ");
      Serial.print(avgTimeToWet);
      Serial.println(" mins to get wet (check for soil compaction).");
    } else {
      Serial.println("DIAGNOSIS: Good soil infiltration.");
    }
    
    if (avgTimeToDry > HEALTHY_DRYING_TIME_HOURS) {
      Serial.print("DIAGNOSIS: POOR DRAINAGE. Soil took ");
      Serial.print(avgTimeToDry);
      Serial.println(" hours to dry (check for waterlogging).");
    } else {
      Serial.println("DIAGNOSIS: Good soil drainage.");
    }
  } else {
    Serial.println("DIAGNOSIS: No irrigation cycles, cannot assess soil health.");
  }
  Serial.println("=========================================");
  
  // 4. Add daily totals to monthly totals & SAVE to flash
  monthlyIrrigationFrequency += irrigationFrequencyCount;
  monthlyTotalLitersUsed += totalLitersUsed;
  monthlyTotalWateringMin += totalWateringDurationMin;
  monthlyAvgTimeToWet += avgTimeToWet;
  monthlyAvgTimeToDry += avgTimeToDry;
  monthlyAvgFieldMoisture += avgFieldMoisture;
  monthlyReportCount++;
  
  // --- FIX 3 (Reviewer Bug 3): Wrap save in begin()/end() ---
  prefs.begin("irrig", false); // Open namespace
  prefs.putFloat("monLiters", monthlyTotalLitersUsed);
  prefs.putFloat("monMinutes", monthlyTotalWateringMin);
  prefs.putInt("monIrr", monthlyIrrigationFrequency);
  prefs.putFloat("monAvgWet", monthlyAvgTimeToWet);
  prefs.putFloat("monAvgDry", monthlyAvgTimeToDry);
  prefs.putFloat("monAvgMoist", monthlyAvgFieldMoisture);
  prefs.putInt("monCnt", monthlyReportCount);
  prefs.end(); // Close namespace
}

/**
 * @brief Prints a single CSV line for the daily report.
 */
void logCsvDailySummary() {
  float avgFieldMoisture = (totalFieldMoistureReadings>0) 
      ? (float)totalFieldMoistureSum / totalFieldMoistureReadings : 0.0;
  float avgWatering = (irrigationFrequencyCount>0) ? totalWateringDurationMin/irrigationFrequencyCount : 0.0;
  float avgWet = (irrigationFrequencyCount>0) ? totalTimeToWetMin/irrigationFrequencyCount : 0.0;
  float avgDry = (irrigationFrequencyCount>0) ? totalTimeToDryHours/irrigationFrequencyCount : 0.0;

  Serial.print("CSV,");
  Serial.print(dailyIndex); Serial.print(','); // Use the daily index
  Serial.print(millis()); Serial.print(',');
  Serial.print(irrigationFrequencyCount); Serial.print(',');
  Serial.print(totalLitersUsed); Serial.print(',');
  Serial.print(avgWatering); Serial.print(',');
  Serial.print(avgWet); Serial.print(',');
  Serial.print(avgDry); Serial.print(',');
  Serial.println(avgFieldMoisture);
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
  
  Serial.println("--- Monthly Soil Health Diagnosis ---");
  if (monthlyReportCount > 0) {
    if (finalAvgTimeToWet > HEALTHY_WETTING_TIME_MIN) {
      Serial.println("DIAGNOSIS: POOR INFILTRATION (Consistent).");
    } else {
      Serial.println("DIAGNOSIS: Good soil infiltration (Consistent).");
    }
    if (finalAvgTimeToDry > HEALTHY_DRYING_TIME_HOURS) {
      Serial.println("DIAGNOSIS: POOR DRAINAGE (Consistent).");
    } else {
      Serial.println("DIAGNOSIS: Good soil drainage (Consistent).");
    }
  } else {
    Serial.println("DIAGNOSIS: No data to assess.");
  }
  
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
  dailyCheckTime = millis();

  for (int i = 0; i < NUM_SENSORS; i++) {
    totalMoistureSumPerSensor[i] = 0;
    totalMoistureReadingsPerSensor[i] = 0;
  }
}

/**
 * @brief Resets all monthly counters and clears flash memory.
 */
void resetMonthlyMetrics() {
  monthlyIrrigationFrequency = 0;
  monthlyTotalLitersUsed = 0;
  monthlyTotalWateringMin = 0;
  monthlyAvgTimeToWet = 0;
  monthlyAvgTimeToDry = 0;
  monthlyAvgFieldMoisture = 0;
  monthlyReportCount = 0;
  dailyIndex = 0; // Reset day counter for the new month
  monthlyCheckTime = millis();
  
  // Explicitly clear keys instead of wipe
  prefs.begin("irrig", false);
  prefs.putFloat("monLiters", 0.0);
  prefs.putFloat("monMinutes", 0.0);
  prefs.putInt("monIrr", 0);
  prefs.putFloat("monAvgWet", 0.0);
  prefs.putFloat("monAvgDry", 0.0);
  prefs.putFloat("monAvgMoist", 0.0);
  prefs.putInt("monCnt", 0);
  prefs.putULong("dailyIndex", 0);
  prefs.end();
}

/**
 * @brief Runs a self-test to check channel mapping.
 */
void runPowerOnSelfTest() {
  Serial.println("--- Power-On Self-Test: Channel Map ---");
  Serial.println("Verifying Sensor Map (X,Y) against Mux Channels...");
  for (int ch = 0; ch < NUM_SENSORS; ch++) {
    Serial.print("Mux Channel "); Serial.print(ch);
    bool found = false;
    for (int i = 0; i<NUM_SENSORS; i++) {
      if (sensorMap[i].channel == ch) {
        Serial.print(" -> Mapped to (X,Y): (");
        Serial.print(sensorMap[i].x); Serial.print(",");
        Serial.print(sensorMap[i].y); Serial.println(")");
        found = true;
        break;
      }
    }
    if (!found) Serial.println(" -> ERROR: Unmapped channel!");
  }
  Serial.println("--- Self-Test Complete ---");
}

/**
 * @brief Initializes the sensor (X,Y) map.
 */
void initializeSensorMap() {
  // NOTE: Channel 0 is assigned last (sensorMap[7])
  // This is intentional and matches the 2x4 grid
  sensorMap[0] = {10, 10, 1, false, 0, 0}; 
  sensorMap[1] = {10, 20, 2, false, 0, 0}; 
  sensorMap[2] = {10, 30, 3, false, 0, 0};
  sensorMap[3] = {10, 40, 4, false, 0, 0};
  sensorMap[4] = {20, 10, 5, false, 0, 0};
  sensorMap[5] = {20, 20, 6, false, 0, 0};
  sensorMap[6] = {20, 30, 7, false, 0, 0}; 
  sensorMap[7] = {20, 40, 0, false, 0, 0};
}