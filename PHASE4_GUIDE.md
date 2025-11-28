# Phase 4: Web Interface - Complete Setup Guide

## 🎉 What's New in Phase 4

Your ESP32 irrigation system now has a **full web interface** with:
- ✅ WiFi connectivity
- ✅ Live sensor dashboard
- ✅ Real-time state monitoring
- ✅ Remote report generation
- ✅ Dynamic configuration via `config.json` upload
- ✅ SPIFFS filesystem for persistent settings

---

## 📋 Prerequisites

### Required Libraries (Install via Arduino IDE Library Manager):
1. **WiFi.h** - Built-in with ESP32 core
2. **WebServer.h** - Built-in with ESP32 core
3. **ArduinoJson** - v6.x (Search: "ArduinoJson" by Benoit Blanchon)
4. **SPIFFS.h** - Built-in with ESP32 core
5. **Preferences.h** - Built-in with ESP32 core

### Hardware:
- ESP32 with WiFi capability
- Router/WiFi network (2.4GHz)

---

## 🚀 Quick Start

### Step 1: Board Configuration (Arduino IDE)

Go to **Tools** menu and configure:
- **Board**: ESP32 Dev Module (or your specific ESP32 board)
- **Upload Speed**: 921600 (use 115200 if upload fails)
- **Flash Frequency**: 80MHz
- **Flash Mode**: QIO
- **Flash Size**: 4MB or larger
- **Partition Scheme**: "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
- **Port**: Select your ESP32's COM/USB port

### Step 2: Configure WiFi Credentials

Open `automatic irrigation esp 32.ino` and search for `const char* ssid`:

```cpp
const char* ssid = "YOUR_WIFI_NAME";     // ← Change this
const char* password = "YOUR_PASSWORD";   // ← Change this
```

> **Note:** WiFi network must be 2.4GHz (ESP32 doesn't support 5GHz)

### Step 3: Upload the Code

1. Connect ESP32 via USB
2. Click **Upload** button
3. Wait for compilation and upload
4. Open **Serial Monitor** (115200 baud)

### Step 4: Find Your ESP32's IP Address

Look in Serial Monitor for:
```
WiFi connected!
IP address: 192.168.1.XXX
Web Server started.
Access dashboard at: http://192.168.1.XXX
```

### Step 5: Access the Web Dashboard

1. Open a browser on the same WiFi network
2. Navigate to: http://192.168.1.XXX (use your ESP32's IP)
3. You should see the **ESP32 Irrigation Control** dashboard

---

## 🖥️ Web Interface Features

### 1. Live Status Dashboard
- **System State**: Shows current state (MONITORING, IRRIGATING, WAITING, SYSTEM_FAULT)
- **Sensor Grid**: 8 sensors with:
  - Channel number and (X,Y) coordinates
  - Real-time moisture percentage
  - Raw ADC value
  - Color coding:
    - Red border = Dry sensor
    - Blue border = Super wet (>80%)
    - Gray = Normal

### 2. Report Generation
- **Get Daily Report**: Triggers 24-hour summary (output to Serial Monitor)
- **Get Monthly Report**: Triggers 30-day summary (output to Serial Monitor)

### 3. Configuration Upload
- Upload custom config.json to override default thresholds
- Changes take effect immediately (no code recompilation needed!)

---

## ⚙️ Configuration System

### config.json Format

The system supports **13 configurable parameters**:

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

### Parameter Descriptions

#### Calibration & Threshold Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| CALIBRATION_DRY | 3500 | ADC value when sensor is in air (0% moisture) |
| CALIBRATION_WET | 1200 | ADC value when sensor is in water (100% moisture) |
| DRY_THRESHOLD | 3000 | ADC value above which a sensor is considered "dry" |
| WET_THRESHOLD | 1500 | ADC value below which a sensor is considered "wet" |
| LEAK_THRESHOLD_PERCENT | 98 | Moisture % that triggers leak alert |
| ADC_SAMPLES | 5 | Number of readings averaged per sensor (noise reduction) |
| MUX_SETTLE_TIME_US | 800 | Microseconds to wait after MUX channel switch |
| ADC_SAMPLE_DELAY_MS | 1 | Milliseconds between ADC samples |

#### Timing Parameters

| Parameter | Default | Human-Readable | Description |
|-----------|---------|----------------|-------------|
| CHECK_INTERVAL_MS | 60000 | 1 minute | How often to check sensors in MONITORING state |
| MIN_PUMP_ON_TIME_MS | 300000 | 5 minutes | Minimum pump run time before checking if field is wet |
| MAX_PUMP_ON_TIME_MS | 3600000 | 1 hour | Maximum pump run time before triggering FAULT state |
| POST_IRRIGATION_WAIT_TIME_MS | 14400000 | 4 hours | Cooldown period after irrigation before next check |
| IRRIGATING_CHECK_INTERVAL_MS | 10000 | 10 seconds | How often to check wetness while pump is running |

### Timing Configuration Examples

**For faster testing (shorter cycles):**
```json
{
  "CHECK_INTERVAL_MS": 30000,
  "MIN_PUMP_ON_TIME_MS": 60000,
  "MAX_PUMP_ON_TIME_MS": 300000,
  "POST_IRRIGATION_WAIT_TIME_MS": 600000,
  "IRRIGATING_CHECK_INTERVAL_MS": 5000
}
```

**For water-sensitive plants (longer soak time):**
```json
{
  "MIN_PUMP_ON_TIME_MS": 600000,
  "POST_IRRIGATION_WAIT_TIME_MS": 21600000
}
```

---

## 🛠 Troubleshooting

### WiFi Won't Connect
**Solutions**:
1. Check SSID and password spelling (case-sensitive)
2. Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
3. Move ESP32 closer to router

**Note**: System will continue running without WiFi

### SPIFFS Mount Failed
**Solutions**:
1. Select correct partition scheme with SPIFFS
   - Tools → Partition Scheme → "Default 4MB with spiffs"
2. Erase flash and re-upload

### Dashboard Shows "Loading..." Forever
**Solutions**:
1. Open browser console (F12) and check for errors
2. Verify ESP32 is on same network
3. Try accessing /data endpoint directly: http://[IP]/data

### Config Upload Fails
**Solutions**:
1. File MUST be named exactly `config.json`
2. Validate JSON syntax at jsonlint.com
3. File must be < 2KB
4. Check Serial Monitor for parse errors

---

## 📊 Performance

- Dashboard auto-refreshes every **3 seconds**
- Sensor readings update based on **CHECK_INTERVAL_MS** (default: 1 minute)
- WebServer handles **1-2 concurrent connections**

---

## 📁 Related Documentation

- **QUICK_REFERENCE.md** - Quick reference card for daily use
- **TEST_CHECKLIST.md** - Verification checklist after setup
- **CHANGES_SUMMARY.md** - Technical details of Phase 4 changes
- **ALL_IMPROVEMENTS_COMPLETE.md** - Complete list of all improvements

---

**Congratulations!** Your ESP32 Smart Irrigation System is now fully operational with web interface. 🌱💧
