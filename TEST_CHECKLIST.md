# Phase 4 Testing Checklist

Use this checklist to verify your Phase 4 implementation is working correctly.

## Pre-Upload Checks

### 1. Library Installation
Open Arduino IDE → Sketch → Include Library → Manage Libraries

- [ ] **ArduinoJson** v6.21.0 or higher (NOT v7.x!)
  - Search: "ArduinoJson"
  - Author: Benoit Blanchon
  - Install if missing

### 2. Board Configuration
Go to Tools menu:

- [ ] **Board**: "ESP32 Dev Module" (or your specific ESP32 board)
- [ ] **Partition Scheme**: "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
- [ ] **Upload Speed**: 921600 (or 115200 if issues)
- [ ] **Port**: Select correct COM/USB port

### 3. WiFi Credentials
Edit `automatic irrigation esp 32.ino` lines 88-89:

- [ ] SSID matches your WiFi name exactly (case-sensitive)
- [ ] Password is correct
- [ ] Using 2.4GHz network (ESP32 doesn't support 5GHz)

---

## Upload Process

### 1. Compile
- [ ] Click "Verify" (checkmark icon)
- [ ] No compilation errors
- [ ] Sketch uses less than 80% of program storage

### 2. Upload
- [ ] Click "Upload" (arrow icon)
- [ ] Upload completes successfully
- [ ] No "timed out" or "connect" errors

---

## Serial Monitor Checks

Open Serial Monitor (Ctrl+Shift+M) at **115200 baud**

### Boot Sequence
Look for these messages in order:

```
- [ ] ESP32 Irrigation (Refactored) Initializing...
- [ ] Initializing SPIFFS...
- [ ] SPIFFS mounted successfully.
- [ ] Files in SPIFFS:
- [ ] Loading config from SPIFFS...
- [ ] config.json not found. Using default settings. (expected on first boot)
- [ ] Connecting to WiFi: [YOUR_SSID]
- [ ] WiFi connected!
- [ ] IP address: 192.168.X.XXX
- [ ] --- Power-On Self-Test: Channel Map ---
- [ ] Web Server started.
- [ ] Access dashboard at: http://192.168.X.XXX
```

### What to Check:
- [ ] No "SPIFFS mount failed" errors
- [ ] WiFi connects within 15 seconds
- [ ] IP address is displayed
- [ ] All 8 channels map correctly in self-test
- [ ] No "ERROR" messages

**Write down your IP address**: ___________________

---

## Web Dashboard Checks

### 1. Access Dashboard
In a browser (on same WiFi network):

- [ ] Navigate to `http://[YOUR_IP]`
- [ ] Page loads within 3 seconds
- [ ] No "Unable to connect" errors
- [ ] Dashboard displays properly (not blank)

### 2. Live Status Section
- [ ] System State shows (MONITORING, IRRIGATING, WAITING, or SYSTEM_FAULT)
- [ ] IP address displays correctly
- [ ] Status box color changes (gray for MONITORING)

### 3. Sensor Grid
- [ ] All 8 sensors display
- [ ] Each shows:
  - [ ] Channel number (0-7)
  - [ ] Coordinates (10,10) through (20,40)
  - [ ] Moisture percentage (0-100%)
  - [ ] Raw ADC value (1200-3500 range)
- [ ] Sensors update every 3 seconds

### 4. Report Buttons
- [ ] "Get Daily Report" button exists
- [ ] "Get Monthly Report" button exists
- [ ] Clicking shows "Command executed" message
- [ ] Serial Monitor receives corresponding report

### 5. Upload Config Tab
- [ ] "Upload Config" button switches page
- [ ] File selector appears
- [ ] "Upload" button present

---

## Functional Tests

### Test 1: State Change Detection
In Serial Monitor, type: `STATE:IRRIGATING`

- [ ] Command acknowledged
- [ ] Web dashboard state changes to "IRRIGATING" within 3 seconds
- [ ] Status box turns green
- [ ] Relay clicks (pump turns on)

Type: `STATE:MONITORING`
- [ ] State returns to MONITORING
- [ ] Dashboard updates
- [ ] Pump turns off

### Test 2: Sensor Simulation
In Serial Monitor, type: `S0:3500`

- [ ] Command acknowledged
- [ ] Sensor 0 on dashboard shows ~0% moisture
- [ ] Next update shows sensor as DRY (red border)

Type: `S0:1200`
- [ ] Sensor 0 shows ~100% moisture
- [ ] Sensor shows WET (blue border)

### Test 3: Config Upload

**Step 1**: Edit `config.json` to test values:
```json
{
  "CALIBRATION_DRY": 3600,
  "CALIBRATION_WET": 1100,
  "DRY_THRESHOLD": 3100,
  "WET_THRESHOLD": 1400,
  "LEAK_THRESHOLD_PERCENT": 95,
  "ADC_SAMPLES": 10,
  "MUX_SETTLE_TIME_US": 1000,
  "ADC_SAMPLE_DELAY_MS": 2
}
```

**Step 2**: Upload via web interface:
- [ ] Click "Upload Config" tab
- [ ] Select modified `config.json`
- [ ] Click "Upload"
- [ ] See "Config uploaded successfully!" message
- [ ] Serial Monitor shows:
  ```
  Receiving file: config.json
  Upload complete: XXX bytes
  Configuration loaded successfully:
    DRY_THRESHOLD: 3100
    WET_THRESHOLD: 1400
    ...
  ```
- [ ] Values match your JSON file

**Step 3**: Verify persistence:
- [ ] Press ESP32 reset button
- [ ] After reboot, Serial Monitor shows loaded config
- [ ] New threshold values are used

### Test 4: WiFi Resilience
- [ ] Unplug router / disable WiFi
- [ ] Serial Monitor shows system continues running
- [ ] Sensor readings still print every minute
- [ ] State machine continues operating
- [ ] Dashboard becomes inaccessible (expected)
- [ ] Re-enable WiFi
- [ ] Dashboard becomes accessible again

---

## Performance Checks

### Response Times
- [ ] Dashboard loads in < 3 seconds
- [ ] Sensor data updates every 3 seconds
- [ ] Config upload completes in < 2 seconds
- [ ] No visible lag when switching tabs

### Memory Usage
In Serial Monitor, check boot messages:

- [ ] Free heap reported (should be > 100KB)
- [ ] No "low memory" warnings
- [ ] No unexpected reboots

### Concurrent Access
- [ ] Open dashboard on 2 devices simultaneously
- [ ] Both show same data
- [ ] No crashes or hangs
- [ ] Slight delay acceptable

---

## Troubleshooting Quick Fixes

### ❌ WiFi Won't Connect
```cpp
// Change these in code:
const char* ssid = "YOUR_EXACT_WIFI_NAME";      // Check case
const char* password = "YOUR_EXACT_PASSWORD";    // Check special chars
```
Re-upload code.

### ❌ SPIFFS Mount Failed
1. Tools → Erase Flash → "All Flash Contents"
2. Re-upload code
3. If still fails, change partition scheme

### ❌ ArduinoJson Errors
```
error: 'StaticJsonDocument' was not declared
```
**Fix**: Install ArduinoJson v6.x (NOT v7.x)

### ❌ Dashboard Shows "Loading..."
1. Open browser console (F12)
2. Look for JavaScript errors
3. Check ESP32 is on same WiFi
4. Try accessing `/data` directly: `http://[IP]/data`

### ❌ Config Upload Does Nothing
1. File MUST be named `config.json` exactly
2. Validate JSON at jsonlint.com
3. File must be < 1KB
4. Check Serial Monitor for parse errors

---

## Success Criteria

Your Phase 4 is **fully operational** if:

- ✅ All Serial Monitor checks pass
- ✅ Dashboard loads and displays sensors
- ✅ State changes reflect in real-time
- ✅ Config upload works and persists after reboot
- ✅ System continues running if WiFi drops
- ✅ Reports trigger via web buttons
- ✅ No error messages in Serial Monitor

---

## Next Steps After Testing

1. **Calibration**: Measure your actual sensors in air/water
2. **Threshold Tuning**: Adjust DRY/WET based on soil type
3. **Production Deployment**: Install in field
4. **Monitoring**: Watch for 24-48 hours
5. **Optimization**: Fine-tune based on real data

---

**Testing Date**: _______________
**Tester**: _______________
**ESP32 Model**: _______________
**Result**: PASS / FAIL

**Notes**:
_______________________________________
_______________________________________
_______________________________________
