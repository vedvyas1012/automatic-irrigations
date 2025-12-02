# Phase 4: Web Interface & Dynamic Configuration - COMPLETE

## Overview
Phase 4 adds WiFi connectivity, a live web dashboard, and dynamic configuration via SPIFFS filesystem.

---

## Features Implemented

### 1. WiFi Connectivity
- Connects to your WiFi network on boot
- 15-second timeout with graceful fallback
- Continues irrigation logic even if WiFi fails

### 2. Web Dashboard
- **Live Status Display**: Shows current system state (MONITORING/IRRIGATING/WAITING/FAULT)
- **Real-time Sensor Grid**: 8 sensors with moisture percentage and raw ADC values
- **Color-coded Sensors**:
  - Red border = Dry (needs water)
  - Blue border = Very wet (>80%)
  - Gray border = Normal
- **Auto-refresh**: Updates every 3 seconds

### 3. Remote Control
- **Daily Report Button**: Triggers on-demand daily report (output to Serial Monitor)
- **Monthly Report Button**: Triggers on-demand monthly report
- Serial commands can be sent via web interface

### 4. Dynamic Configuration (SPIFFS)
- Configuration stored in `/config.json` on ESP32 flash memory
- Upload new config via web interface without re-compiling code
- Changes take effect on upload
- Falls back to hardcoded defaults if file missing/invalid

---

## How to Use

### Step 1: Upload the Code
1. Open `automatic irrigation esp 32.ino` in Arduino IDE
2. Install required libraries:
   - `WiFi` (built-in)
   - `WebServer` (built-in)
   - `ArduinoJson` (v6.x)
   - `SPIFFS` (built-in)
   - `Preferences` (built-in)
3. Search for `const char* ssid` and edit WiFi credentials:
   ```cpp
   const char* ssid = "YOUR_WIFI_NAME";
   const char* password = "YOUR_PASSWORD";
   ```
4. Upload to ESP32

### Step 2: Upload config.json (Optional)
1. Open Serial Monitor (115200 baud)
2. Note the IP address (e.g., `192.168.1.100`)
3. Open browser and go to: `http://192.168.1.100`
4. Click "Upload Config" tab
5. Select the provided `config.json` file
6. Click Upload
7. ESP32 will reload configuration automatically

### Step 3: Access Dashboard
- Open browser: `http://[ESP32_IP_ADDRESS]`
- View live sensor data
- Monitor system state
- Generate reports

---

## Configuration File Format

The `config.json` file supports **13 configurable parameters**:

```json
{
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
}
```

### Configuration Parameters

#### Calibration & Threshold Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `CALIBRATION_DRY` | 3500 | ADC value for 0% moisture (sensor in air) |
| `CALIBRATION_WET` | 1200 | ADC value for 100% moisture (sensor in water) |
| `DRY_THRESHOLD` | 3000 | ADC value above which sensor is "dry" |
| `WET_THRESHOLD` | 1500 | ADC value below which sensor is "wet" |
| `LEAK_THRESHOLD_PERCENT` | 98 | Moisture % for leak detection |
| `ADC_SAMPLES` | 5 | Number of samples for noise filtering |
| `MUX_SETTLE_TIME_US` | 800 | Multiplexer settling time (microseconds) |
| `ADC_SAMPLE_DELAY_MS` | 1 | Delay between ADC samples (milliseconds) |

#### Timing Parameters

| Parameter | Default | Human-Readable | Description |
|-----------|---------|----------------|-------------|
| `CHECK_INTERVAL_MS` | 60000 | 1 minute | Sensor check frequency in MONITORING state |
| `MIN_PUMP_ON_TIME_MS` | 300000 | 5 minutes | Minimum pump run before checking wetness |
| `MAX_PUMP_ON_TIME_MS` | 3600000 | 1 hour | Maximum pump run before FAULT state |
| `POST_IRRIGATION_WAIT_TIME_MS` | 14400000 | 4 hours | Cooldown after irrigation |
| `IRRIGATING_CHECK_INTERVAL_MS` | 10000 | 10 seconds | Wetness check frequency while irrigating |

### Calibration Guide

**To find your sensor's calibration values:**

1. **Dry Calibration**:
   - Remove sensor from soil (hold in air)
   - Check Serial Monitor for raw ADC value
   - Set `CALIBRATION_DRY` to this value

2. **Wet Calibration**:
   - Submerge sensor in water
   - Check Serial Monitor for raw ADC value
   - Set `CALIBRATION_WET` to this value

3. **Threshold Tuning**:
   - `DRY_THRESHOLD`: Set to ~85% of DRY value
   - `WET_THRESHOLD`: Set to ~120% of WET value

---

## Web API Endpoints

The ESP32 hosts a REST API:

### `GET /`
Returns the HTML dashboard page.

### `GET /data`
Returns JSON with live system data:
```json
{
  "state": "MONITORING",
  "ip": "192.168.1.100",
  "sensors": [
    {
      "channel": 1,
      "x": 10,
      "y": 10,
      "val": 2500,
      "pct": 43,
      "isDry": false
    },
    ...
  ]
}
```

### `GET /serial?cmd=REPORT`
Triggers a daily report (output to Serial Monitor).

### `GET /serial?cmd=MONTHLY_REPORT`
Triggers a monthly report (output to Serial Monitor).

### `POST /upload`
Uploads `config.json` file to SPIFFS.
- Form field name: `config`
- Accepted file: `config.json`
- Auto-reloads configuration after upload

---

## Troubleshooting

### WiFi Not Connecting
1. Check SSID and password spelling (case-sensitive)
2. Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
3. Check Serial Monitor for error messages
4. System will continue irrigation logic even without WiFi

### Can't Access Dashboard
1. Verify ESP32 is connected to WiFi (check Serial Monitor)
2. Note the IP address from Serial Monitor
3. Ensure your device is on the same network
4. Try `http://[IP]` not `https://`

### Config Upload Fails
1. Ensure file is named exactly `config.json`
2. Check file size (<2KB recommended)
3. Verify JSON syntax using a validator
4. Check Serial Monitor for error messages

### Sensors Show Wrong Values
1. Recalibrate sensors (see Calibration Guide above)
2. Update `CALIBRATION_DRY` and `CALIBRATION_WET`
3. Upload new config.json
4. Restart ESP32

---

## Memory Usage

- HTML page stored in PROGMEM (flash) to save RAM
- JSON buffer: 2048 bytes for sensor data response
- Config buffer: 1024 bytes for configuration parsing
- SPIFFS: Uses ESP32 flash partition (typically 1-4MB available)

---

## Security Notes

1. **Unencrypted**: Web server uses HTTP (not HTTPS)
2. **No Authentication**: Anyone on your network can access dashboard
3. **For Local Use**: Designed for home/private networks only
4. **WiFi Credentials**: Stored in plain text in code

**Recommendations:**
- Use a secure WiFi password
- Don't expose ESP32 to the internet
- Consider adding authentication for production use

---

## Next Steps / Future Enhancements

Potential improvements for Phase 5:

1. **Authentication**: Add username/password to web interface
2. **HTTPS**: Enable SSL/TLS encryption
3. **MQTT**: Add IoT integration for cloud monitoring
4. **Push Notifications**: SMS/Email alerts for faults
5. **Historical Charts**: Graph moisture trends over time
6. **OTA Updates**: Upload firmware via web interface

---

## Testing Checklist

- [x] WiFi connects successfully
- [x] Dashboard loads in browser
- [x] Sensor data updates every 3 seconds
- [x] System state displays correctly
- [x] IP address shows in status bar
- [x] Dry sensors show red border
- [x] Wet sensors show blue border
- [x] Report buttons trigger serial output
- [x] config.json uploads successfully
- [x] Configuration reloads after upload
- [x] Invalid config falls back to defaults
- [x] System works without WiFi connection
- [x] SPIFFS lists files on boot

---

**Phase 4 Status: ✅ COMPLETE**
