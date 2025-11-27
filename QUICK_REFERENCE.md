# ESP32 Irrigation System - Quick Reference Card

## 🔧 Setup (One-Time)

1. **Edit WiFi credentials** (lines 94-95):
   ```cpp
   const char* ssid = "YOUR_WIFI";
   const char* password = "YOUR_PASSWORD";
   ```

2. **Install ArduinoJson library**:
   - Arduino IDE → Sketch → Include Library → Manage Libraries
   - Search "ArduinoJson" → Install v6.x

3. **Select partition scheme**:
   - Tools → Partition Scheme → "Default 4MB with spiffs"

4. **Upload code** → Open Serial Monitor (115200 baud)

5. **Note IP address** from Serial Monitor

---

## 🌐 Web Dashboard Access

**URL**: `http://[ESP32_IP_ADDRESS]`

**Features**:
- Live sensor grid (updates every 3 seconds)
- System state indicator
- Daily/Monthly report buttons
- Config file upload

---

## 📝 Serial Commands

Type these in Serial Monitor (115200 baud):

| Command | Action |
|---------|--------|
| `REPORT` | Generate daily report |
| `MONTHLY_REPORT` | Generate monthly report |
| `STATE:MONITORING` | Force state to MONITORING |
| `STATE:IRRIGATING` | Force state to IRRIGATING |
| `STATE:WAITING` | Force state to WAITING |
| `S0:3500` | Set sensor 0 to value 3500 (simulation) |
| `FORCE_CHECK` | Force immediate monitoring check |
| `FORCE_RESET` | Clear system fault, return to MONITORING |
| `TEST_PUMP_30S` | Run pump for 30 seconds (test) |

---

## ⚙️ Key Thresholds

| Setting | Default | Description |
|---------|---------|-------------|
| DRY_THRESHOLD | 3000 | Irrigation starts when sensor > this |
| WET_THRESHOLD | 1500 | Irrigation stops when all sensors < this |
| MIN_PUMP_TIME | 5 min | Minimum irrigation duration |
| MAX_PUMP_TIME | 60 min | Maximum before fault |
| COOLDOWN | 4 hours | Waiting period after irrigation |
| CHECK_INTERVAL | 1 min | Monitoring check frequency |

Edit via `config.json` upload (no code changes needed!)

---

## 🚦 System States

| State | Color (Web) | Meaning |
|-------|-------------|---------|
| **MONITORING** | Gray | Checking sensors every 1 min |
| **IRRIGATING** | Green | Pump ON, watering field |
| **WAITING** | Gray | 4-hour cooldown after watering |
| **SYSTEM_FAULT** | Red | Error detected, pump OFF |

---

## 🔴 Diagnostic Alerts

**Leak Detection**: Sensor >98% moisture while pump OFF
**Clog Detection**: 7 sensors wet, 1 dry after irrigation
**Pump Failure**:
- Pump runs 60 min without wetting field
- Moisture not increasing after 3 checks

**Unexpectedly Dry**: Dry cluster persists >7 days
**Unexpectedly Wet**: Field stays wet >3 days

---

## 🎯 Sensor Grid Layout

```
(10,10) [Ch1]   (10,20) [Ch2]   (10,30) [Ch3]   (10,40) [Ch4]
(20,10) [Ch5]   (20,20) [Ch6]   (20,30) [Ch7]   (20,40) [Ch0]
```

**Color Coding**:
- 🔴 Red border = DRY (needs water)
- 🔵 Blue border = Very WET (>80%)
- ⚪ Gray border = Normal

---

## 📊 Reports

### Daily Report (24 hours):
- Per-sensor averages
- Irrigation cycle count
- Water usage (liters)
- Wetting/drying rates
- Soil health diagnosis

### Monthly Report (30 days):
- Total irrigation cycles
- Total water used
- Average field moisture
- Consistent soil health trends

---

## 🛠️ Common Tasks

### Calibrate Sensors:
1. **Dry calibration**: Remove sensor from soil, read value
2. **Wet calibration**: Submerge in water, read value
3. Update `config.json` with new values
4. Upload via web dashboard

### Change Watering Sensitivity:
- **More aggressive**: Lower DRY_THRESHOLD (e.g., 2800)
- **Less frequent**: Raise DRY_THRESHOLD (e.g., 3200)

### Upload New Config:
1. Edit `config.json` on computer
2. Web dashboard → "Upload Config" tab
3. Select file → Upload
4. Config reloads automatically

---

## ⚡ Quick Troubleshooting

| Problem | Solution |
|---------|----------|
| WiFi won't connect | Check credentials, use 2.4GHz network |
| Dashboard won't load | Verify IP, check same WiFi network |
| Sensors stuck at 0% | Check wiring, ADC attenuation |
| Pump won't turn off | Send `FORCE_RESET` command |
| SPIFFS mount failed | Set partition scheme, re-upload |
| Config upload fails | File must be named `config.json` |

---

## 📱 Mobile Access

1. Connect phone to **same WiFi** as ESP32
2. Open browser (Chrome/Safari)
3. Enter `http://[ESP32_IP]`
4. Bookmark for quick access

**Works on**: iPhone, Android, tablets, laptops

---

## 🔒 Security Notes

⚠️ **No authentication** - Anyone on your WiFi can access
✅ **Safe for home use** on private network
❌ **Do not expose to internet** without VPN

---

## 📞 Support Resources

- `PHASE4_GUIDE.md` - Complete setup guide
- `CHANGES_SUMMARY.md` - Technical documentation
- Serial Monitor - Real-time debugging
- Web dashboard - Visual status

---

## 🎓 Pin Reference

| Component | ESP32 Pin |
|-----------|-----------|
| Relay (Pump) | GPIO 23 |
| MUX Common | GPIO 34 (ADC) |
| MUX S0 | GPIO 19 |
| MUX S1 | GPIO 18 |
| MUX S2 | GPIO 5 |

---

**Version**: 4.0 (Phase 4 Complete)
**Last Updated**: 2025-11-27
**Total Lines**: 1422

---

**Keep this card handy for quick reference!** 📌
