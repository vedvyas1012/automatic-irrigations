# ESP32 Smart Spatial Irrigation System

An autonomous irrigation controller using **spatial logic** (analyzing relationships between multiple sensors) rather than simple thresholds to make intelligent watering decisions.

## 🌟 Features

### Phase 1-3: Core Irrigation Logic
- **Smart State Machine**: MONITORING → IRRIGATING → WAITING → SYSTEM_FAULT
- **Spatial Cluster Detection**: BFS algorithm finds 3+ neighboring dry sensors
- **Dual-Threshold System**: Separate DRY (3000) and WET (1500) thresholds
- **Advanced Diagnostics**: Pump failure, leak detection, clog detection
- **Data Analytics**: Daily/monthly reports with CSV logging
- **Soil Health Monitoring**: Infiltration and retention rate analysis

### Phase 4: Web Interface ✨ NEW
- **WiFi Connectivity**: Connect to local network
- **Live Dashboard**: Real-time sensor visualization
- **Remote Monitoring**: Check system state from any device
- **Dynamic Configuration**: Upload config.json without recompiling
- **SPIFFS Filesystem**: Persistent configuration storage
- **13 Configurable Parameters**: Including timing settings

## 🔧 Hardware

- **Controller**: ESP32 (with WiFi)
- **Sensors**: 8x Soil Moisture Sensors (capacitive or resistive)
- **Multiplexer**: CD74HC4067 (16-channel analog MUX)
- **Actuator**: Relay module + water pump
- **Power**: 5V for ESP32, 12V for pump

## 📦 Required Libraries

Install via Arduino IDE Library Manager:
- WiFi.h (built-in)
- WebServer.h (built-in)
- ArduinoJson v6.x
- SPIFFS.h (built-in)
- Preferences.h (built-in)

## 🚀 Quick Start

1. **Configure WiFi**: Search for `const char* ssid` in the .ino file and edit
2. **Enable SPIFFS**: Tools → Partition Scheme → "Default 4MB with spiffs"
3. **Upload Code**: Connect ESP32 and click Upload
4. **Find IP Address**: Check Serial Monitor (115200 baud)
5. **Access Dashboard**: Open browser to `http://[ESP32_IP]`

📖 **See [PHASE4_GUIDE.md](PHASE4_GUIDE.md) for complete setup instructions**

## 🌐 Web Interface

Once connected, access these features:
- **Live Status**: Real-time system state and sensor readings
- **Sensor Grid**: Color-coded 8-sensor visualization
- **Reports**: Daily/monthly analytics via web buttons
- **Config Upload**: Modify thresholds and timing without recompiling

## ⚙️ Configuration

Edit `config.json` to customize behavior (all 13 parameters):
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

Upload via web interface or SPIFFS Data Upload tool.

## 📊 Project Status

- ✅ Phase 1: Hardware integration & basic logic
- ✅ Phase 2: Advanced diagnostics & reporting
- ✅ Phase 3: Soil health analytics & CSV logging
- ✅ **Phase 4: WiFi & Web interface (COMPLETE)**
- ✅ **All Improvements: Bug fixes & enhancements (COMPLETE)**
- 📜 Phase 5: Push notifications & OTA updates (Future)

## 📁 Project Files

### Core Files
- `automatic irrigation esp 32.ino` - Main ESP32 code (Phase 1-4 complete)
- `automatic irrigation uno.ino` - Arduino Uno version (Phase 1-2 only)
- `config.json` - Configuration template (13 parameters)

### Setup & User Guides
| File | Description | When to Use |
|------|-------------|-------------|
| `PHASE4_GUIDE.md` | Complete setup instructions | **Start here** for new setup |
| `QUICK_REFERENCE.md` | Quick reference card | Daily operation lookup |
| `TEST_CHECKLIST.md` | Verification checklist | After setup or changes |

### Technical Documentation
| File | Description | When to Use |
|------|-------------|-------------|
| `PHASE4_README.md` | Detailed Phase 4 features | Deep dive into web features |
| `PHASE4_SUMMARY.md` | Quick implementation summary | Quick overview |
| `CHANGES_SUMMARY.md` | Technical details of all changes | Understanding code changes |

### Improvement History
| File | Description | When to Use |
|------|-------------|-------------|
| `ALL_IMPROVEMENTS_COMPLETE.md` | Complete list of all improvements | Full change history |

### Reference Materials
| File | Description |
|------|-------------|
| `software_working.rtf` | Software flow explanation |
| `hardware_flowchart.png` | Hardware wiring diagram |
| `flowchart_of_software.png` | Software logic flowchart |
| `sequence_diagram_of_software.png` | State sequence diagram |

## 🤝 Contributing

This is a personal project, but suggestions and improvements are welcome!

## 📄 License

Open source for educational and agricultural use.

---

**Last Updated**: 2025-11-29 | **Version**: 4.1 (All Improvements Complete)
