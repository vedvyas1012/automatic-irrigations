# Hybrid 9-Sensor Pin Configuration Update

## Status: ✅ COMPLETE

**Date:** 2025-12-03
**Version:** 4.2.1 - Hybrid Sensor Update

---

## Overview

Updated ESP32 irrigation system to support hybrid sensor reading:
- **Sensors 0-7:** Read via CD4051 multiplexer (GPIO 34)
- **Sensor 8:** Read directly from GPIO 35

This configuration accommodates the hardware limitation where GPIO 5 cannot be used as a multiplexer control pin.

---

## Changes Implemented

### PART 1: Pin Definitions Update ✅
**Location:** Lines 28-38

**Before:**
```cpp
const int RELAY_PIN = 23;
const int MUX_COMMON_PIN = 34;
const int MUX_S0_PIN = 19;
const int MUX_S1_PIN = 18;
const int MUX_S2_PIN = 5;
```

**After:**
```cpp
const int RELAY_PIN = 23;

// Multiplexer Control Pins (Output Capable GPIOs)
const int MUX_S0_PIN = 19;
const int MUX_S1_PIN = 18;
const int MUX_S2_PIN = 21;  // Changed from GPIO 5 to GPIO 21

// Analog Reading Pins (ADC1 - Safe for WiFi)
const int MUX_COMMON_PIN = 34;  // Reads sensors 0-7 via multiplexer
const int SENSOR_9_PIN = 35;    // Reads sensor 8 directly
```

---

### PART 2: ADC Configuration Update ✅
**Location:** Lines 180-184

**Before:**
```cpp
#ifdef ARDUINO_ARCH_ESP32
  analogSetPinAttenuation(MUX_COMMON_PIN, ADC_11db); // ~3.3V full scale
#endif
```

**After:**
```cpp
#ifdef ARDUINO_ARCH_ESP32
  analogSetPinAttenuation(MUX_COMMON_PIN, ADC_11db); // ~3.3V full scale
  analogSetPinAttenuation(SENSOR_9_PIN, ADC_11db);   // ~3.3V full scale for sensor 8
#endif
```

---

### PART 3: Hybrid readSensor() Logic ✅
**Location:** Lines 1331-1371

**Before:**
- All sensors read via multiplexer
- Parameter name: `channel`

**After:**
- Sensors 0-7: Read via multiplexer (GPIO 34)
- Sensor 8: Read directly from GPIO 35
- Parameter name: `index` (more semantic)

**New Implementation:**
```cpp
int readSensor(int index) {
  if (index < 0 || index >= NUM_SENSORS) {
    Serial.print("ERROR: Invalid sensor index: ");
    Serial.println(index);
    return CALIBRATION_DRY;
  }

  if (simulatedValues[index] != -1) {
    int simValue = simulatedValues[index];
    simulatedValues[index] = -1;
    Serial.print("SIM: Using value ");
    Serial.print(simValue);
    Serial.print(" for Sensor ");
    Serial.println(index);
    return simValue;
  }

  long acc = 0;

  if (index < 8) {
    // SENSORS 0-7: Read via Multiplexer
    digitalWrite(MUX_S0_PIN, bitRead(index, 0));
    digitalWrite(MUX_S1_PIN, bitRead(index, 1));
    digitalWrite(MUX_S2_PIN, bitRead(index, 2));
    delayMicroseconds(MUX_SETTLE_TIME_US);

    for (int i = 0; i < ADC_SAMPLES; i++) {
      acc += analogRead(MUX_COMMON_PIN);
      delay(ADC_SAMPLE_DELAY_MS);
    }
  }
  else {
    // SENSOR 8: Read Direct Pin (GPIO 35)
    for (int i = 0; i < ADC_SAMPLES; i++) {
      acc += analogRead(SENSOR_9_PIN);
      delay(ADC_SAMPLE_DELAY_MS);
    }
  }

  return (int)(acc / (long)ADC_SAMPLES);
}
```

---

## Hardware Wiring Reference

### ESP32 Pin Assignments
| GPIO | Connection | Function |
|------|------------|----------|
| 19 | MUX S0 | Multiplexer address bit 0 |
| 18 | MUX S1 | Multiplexer address bit 1 |
| 21 | MUX S2 | Multiplexer address bit 2 |
| 34 | MUX Z (Common) | Multiplexer output (sensors 0-7) |
| 35 | Sensor 8 | Direct sensor connection |
| 23 | Relay | Pump control |

### CD4051 Multiplexer Connections
| MUX Channel | Sensor Index | Sensor Position (3×3 Grid) |
|-------------|--------------|----------------------------|
| Y0 | Sensor 0 | Bottom-left (10, 10) |
| Y1 | Sensor 1 | Bottom-center (20, 10) |
| Y2 | Sensor 2 | Bottom-right (30, 10) |
| Y3 | Sensor 3 | Middle-left (10, 20) |
| Y4 | Sensor 4 | Middle-center (20, 20) |
| Y5 | Sensor 5 | Middle-right (30, 20) |
| Y6 | Sensor 6 | Top-left (10, 30) |
| Y7 | Sensor 7 | Top-center (20, 30) |
| (Direct) | Sensor 8 | Top-right (30, 30) |

---

## Technical Details

### Why GPIO 5 Cannot Be Used
- GPIO 5 has internal pull-up resistor
- Connected to onboard LED on some ESP32 boards
- Can cause unreliable HIGH/LOW output states
- **Solution:** Moved MUX_S2 to GPIO 21 (reliable output pin)

### Why GPIO 35 for Sensor 8
- GPIO 35 is ADC1_CH7 (safe for WiFi operation)
- Input-only pin (cannot interfere with multiplexer control)
- Part of ADC1 block (compatible with MUX_COMMON_PIN on GPIO 34)

### ADC Configuration
- Both GPIO 34 and 35 use ADC_11db attenuation
- Supports 0-3.3V input range
- 12-bit resolution (0-4095 raw values)
- Multiple samples averaged for noise reduction

---

## Testing Checklist

### Basic Functionality
- [ ] Compile code successfully in Arduino IDE
- [ ] Upload to ESP32 without errors
- [ ] Verify Serial Monitor shows correct boot sequence
- [ ] Confirm WiFi connection establishes
- [ ] Check web dashboard loads at ESP32 IP address

### Sensor Reading Tests
- [ ] Test sensors 0-7 via multiplexer
  - Send: `S0:3500`, `S1:3500`, etc.
  - Verify readings appear in Serial Monitor
  - Check values displayed on web dashboard

- [ ] Test sensor 8 direct connection
  - Send: `S8:3500`
  - Verify reading appears correctly
  - Check heatmap displays top-right cell

- [ ] Test all 9 sensors together
  - Verify 3×3 grid displays correctly on web dashboard
  - Check moisture heatmap updates with realistic colors
  - Confirm gradient analysis runs (top vs bottom row)

### Cluster Detection Tests
- [ ] Test 3-sensor cluster (sensors 0, 1, 3)
  - Send: `S0:3500`, `S1:3500`, `S3:3500`
  - Expected: "TOPOLOGY ALERT: Dry cluster of 3 - triggering irrigation!"

- [ ] Test isolated sensor 8
  - Send: `S8:3500`
  - Expected: "TOPOLOGY: Isolated dry sensor Ch8"

### Integration Tests
- [ ] Verify POST test shows all 9 sensors
- [ ] Test file upload still works
- [ ] Verify daily/monthly reports include all sensors
- [ ] Check leak detection works on sensor 8

---

## Serial Commands for Testing

### Individual Sensor Tests
```
S0:3500
S1:2000
S2:1500
S3:3000
S4:2500
S5:2000
S6:1200
S7:1500
S8:3500
```

### Cluster Test (Bottom-Left Corner)
```
S0:3500
S1:3500
S3:3500
```
Expected: Triggers irrigation (3 connected dry sensors)

### Gradient Test (Top Wet, Bottom Dry)
```
S6:1200
S7:1200
S8:1200
S0:3500
S1:3500
S2:3500
```
Expected: "GRADIENT WARNING: Top wet, bottom dry - possible blockage"

---

## Compatibility Notes

### Backward Compatibility
- All existing serial commands work unchanged
- Web dashboard automatically adapts to 9 sensors
- Configuration files (config.json) remain compatible
- Daily/monthly reporting includes all sensors

### Code Changes Impact
- No changes to state machine logic
- No changes to irrigation algorithms
- No changes to web dashboard HTML/CSS/JS
- No changes to configuration system

---

## Files Modified

### Code Files
- `automatic irrigation esp 32.ino` - Updated pin definitions, ADC setup, and readSensor()

### Documentation Files
- `HYBRID_SENSOR_UPDATE.md` - This file (implementation summary)

---

## Next Steps

1. **Hardware Setup:**
   - Wire multiplexer control pins to GPIO 19, 18, 21
   - Connect multiplexer common (Z) to GPIO 34
   - Connect sensor 8 directly to GPIO 35
   - Verify all ground connections are common

2. **Software Upload:**
   - Compile in Arduino IDE
   - Upload to ESP32
   - Monitor Serial output for errors

3. **Verification:**
   - Run POST test to verify all 9 sensors detected
   - Test each sensor individually
   - Verify web dashboard shows 3×3 grid
   - Test cluster detection with sensor 8

4. **Field Deployment:**
   - Install sensors in 3×3 grid layout
   - Verify physical positions match code layout
   - Test irrigation cycle
   - Monitor for 24 hours

---

## Troubleshooting

### Sensor 8 Reading 0 or 4095
- Check GPIO 35 connection
- Verify sensor power supply
- Confirm ADC attenuation is set

### Multiplexer Sensors Reading Incorrectly
- Verify GPIO 21 is used for MUX_S2 (not GPIO 5)
- Check all control pin connections
- Ensure MUX power supply is stable

### Web Dashboard Not Updating Sensor 8
- Verify sensor 8 appears in Serial Monitor
- Check JSON data endpoint (/data)
- Confirm heatmap JavaScript handles 9 sensors

---

**Implementation Complete:** ✅
**Ready for Testing:** ✅
**Status:** All changes applied successfully
