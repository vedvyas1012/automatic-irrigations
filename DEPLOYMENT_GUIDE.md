# ESP32 Smart Irrigation System - Complete Deployment Guide

## Version 4.2.3 - Zero to Production

**Last Updated:** 2025-12-03

---

## Table of Contents

1. [Hardware Requirements](#1-hardware-requirements)
2. [Software Requirements](#2-software-requirements)
3. [Wiring Schematic](#3-wiring-schematic)
4. [Arduino IDE Setup](#4-arduino-ide-setup)
5. [Code Configuration](#5-code-configuration)
6. [Code Upload Process](#6-code-upload-process)
7. [Initial Testing](#7-initial-testing)
8. [Field Installation](#8-field-installation)
9. [System Calibration](#9-system-calibration)
10. [Production Deployment](#10-production-deployment)
11. [Troubleshooting](#11-troubleshooting)

---

## 1. Hardware Requirements

### Electronics Components

| Component | Quantity | Specifications | Purpose |
|-----------|----------|----------------|---------|
| **ESP32 Dev Board** | 1 | ESP32-WROOM-32 (30-pin) | Main microcontroller |
| **CD4051 Multiplexer** | 1 | 8-channel analog MUX | Routes sensors 0-7 |
| **Capacitive Soil Sensors** | 9 | 3.3V-5V compatible | Moisture measurement |
| **Relay Module** | 1 | 5V, 10A rated | Controls water pump |
| **Water Pump** | 1 | 12V DC or AC (depends on relay) | Irrigation |
| **Power Supply (ESP32)** | 1 | 5V 2A USB or Vin input | Powers ESP32 + sensors |
| **Power Supply (Pump)** | 1 | 12V 2A (or per pump specs) | Powers pump |
| **Breadboard/PCB** | 1 | Large breadboard or custom PCB | Connections |
| **Jumper Wires** | 30+ | Male-to-male, male-to-female | Connections |
| **Resistors** | 3 | 10kΩ pull-down (optional) | MUX control stability |

### Tools Required

- Soldering iron + solder (if making permanent connections)
- Wire strippers
- Multimeter (for testing)
- Screwdriver set
- USB cable (micro-USB or USB-C depending on ESP32)
- Computer with Arduino IDE

---

## 2. Software Requirements

### Install Arduino IDE

1. **Download Arduino IDE:**
   - Go to: https://www.arduino.cc/en/software
   - Download version 2.x or 1.8.19+ for your OS
   - Install following system prompts

2. **Install ESP32 Board Support:**
   - Open Arduino IDE
   - Go to: **File → Preferences**
   - In "Additional Board Manager URLs" add:
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Click **OK**
   - Go to: **Tools → Board → Boards Manager**
   - Search for "esp32"
   - Install "esp32 by Espressif Systems" (version 2.0.11 or later)

3. **Install Required Libraries:**
   - Go to: **Sketch → Include Library → Manage Libraries**
   - Install the following:
     - **WiFi** (included with ESP32 core)
     - **WebServer** (included with ESP32 core)
     - **ArduinoJson** (by Benoit Blanchon) - version 6.x
     - **Preferences** (included with ESP32 core)
     - **SPIFFS** (included with ESP32 core)

### Install USB Driver (if needed)

- **Windows:** Install CP210x or CH340 driver (depends on your ESP32 board)
- **Mac:** Usually works without drivers
- **Linux:** May need to add user to dialout group:
  ```bash
  sudo usermod -a -G dialout $USER
  ```

---

## 3. Wiring Schematic

### Complete Wiring Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         ESP32 PINOUT                                      │
├─────────────────────────────────────────────────────────────────────────┤
│  3.3V ──┬─── CD4051 VCC                                                  │
│         ├─── All Sensors VCC (if 3.3V compatible)                        │
│         └─── Pull-up resistors (optional)                                │
│                                                                           │
│  GND  ──┬─── CD4051 GND                                                  │
│         ├─── CD4051 VEE (substrate)                                      │
│         ├─── All Sensors GND                                             │
│         ├─── Relay GND                                                   │
│         └─── Common ground                                               │
│                                                                           │
│  GPIO 19 ─── CD4051 S0 (Address bit 0)                                  │
│  GPIO 18 ─── CD4051 S1 (Address bit 1)                                  │
│  GPIO 21 ─── CD4051 S2 (Address bit 2)                                  │
│  GPIO 34 ─── CD4051 Z (Common output)                                   │
│  GPIO 35 ─── Sensor 8 (Direct connection)                               │
│  GPIO 23 ─── Relay IN (signal)                                          │
│                                                                           │
│  5V/VIN ─── Relay VCC (if 5V relay)                                     │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                      CD4051 MULTIPLEXER                                   │
├─────────────────────────────────────────────────────────────────────────┤
│  Pin 16 (VCC) ─── ESP32 3.3V                                            │
│  Pin 8 (GND)  ─── ESP32 GND                                             │
│  Pin 7 (VEE)  ─── ESP32 GND (for single supply operation)              │
│  Pin 6 (INH)  ─── ESP32 GND (enable chip)                              │
│                                                                           │
│  Pin 11 (S0)  ─── ESP32 GPIO 19                                         │
│  Pin 10 (S1)  ─── ESP32 GPIO 18                                         │
│  Pin 9  (S2)  ─── ESP32 GPIO 21                                         │
│                                                                           │
│  Pin 3  (Z)   ─── ESP32 GPIO 34 (ADC1_CH6)                             │
│                                                                           │
│  Pin 13 (Y0)  ─── Sensor 0 Signal                                       │
│  Pin 14 (Y1)  ─── Sensor 1 Signal                                       │
│  Pin 15 (Y2)  ─── Sensor 2 Signal                                       │
│  Pin 12 (Y3)  ─── Sensor 3 Signal                                       │
│  Pin 1  (Y4)  ─── Sensor 4 Signal                                       │
│  Pin 5  (Y5)  ─── Sensor 5 Signal                                       │
│  Pin 2  (Y6)  ─── Sensor 6 Signal                                       │
│  Pin 4  (Y7)  ─── Sensor 7 Signal                                       │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                    SENSOR CONNECTIONS                                     │
├─────────────────────────────────────────────────────────────────────────┤
│  Each Sensor (0-7):                                                      │
│    VCC    ─── ESP32 3.3V or 5V (check sensor specs)                    │
│    GND    ─── ESP32 GND                                                 │
│    SIGNAL ─── CD4051 Y# pin (see above)                                │
│                                                                           │
│  Sensor 8 (Direct):                                                      │
│    VCC    ─── ESP32 3.3V or 5V                                          │
│    GND    ─── ESP32 GND                                                 │
│    SIGNAL ─── ESP32 GPIO 35 (ADC1_CH7)                                 │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                      RELAY MODULE                                         │
├─────────────────────────────────────────────────────────────────────────┤
│  VCC ─── ESP32 5V or separate 5V supply                                 │
│  GND ─── ESP32 GND                                                       │
│  IN  ─── ESP32 GPIO 23                                                   │
│                                                                           │
│  COM ─── Pump Power Supply (+)                                          │
│  NO  ─── Pump Motor (+)                                                 │
│  NC  ─── (not connected)                                                │
│                                                                           │
│  Pump (-) ─── Pump Power Supply (-)                                     │
└─────────────────────────────────────────────────────────────────────────┘
```

### Pin Summary Table

| ESP32 Pin | Connection | Function |
|-----------|------------|----------|
| **3.3V** | CD4051 VCC, Sensors VCC | Power supply |
| **GND** | All components GND | Common ground |
| **GPIO 19** | CD4051 S0 | MUX address bit 0 |
| **GPIO 18** | CD4051 S1 | MUX address bit 1 |
| **GPIO 21** | CD4051 S2 | MUX address bit 2 |
| **GPIO 34** | CD4051 Z (Common) | Analog input from MUX |
| **GPIO 35** | Sensor 8 signal | Direct analog input |
| **GPIO 23** | Relay IN | Pump control signal |
| **5V/VIN** | Relay VCC (optional) | Relay power |

### CD4051 Pin Mapping

| CD4051 Pin | Pin Name | Connection |
|------------|----------|------------|
| 1 | Y4 | Sensor 4 |
| 2 | Y6 | Sensor 6 |
| 3 | Z (Common) | ESP32 GPIO 34 |
| 4 | Y7 | Sensor 7 |
| 5 | Y5 | Sensor 5 |
| 6 | INH (Enable) | GND (always enabled) |
| 7 | VEE (Substrate) | GND |
| 8 | GND | ESP32 GND |
| 9 | S2 | ESP32 GPIO 21 |
| 10 | S1 | ESP32 GPIO 18 |
| 11 | S0 | ESP32 GPIO 19 |
| 12 | Y3 | Sensor 3 |
| 13 | Y0 | Sensor 0 |
| 14 | Y1 | Sensor 1 |
| 15 | Y2 | Sensor 2 |
| 16 | VCC | ESP32 3.3V |

### Field Sensor Layout (3×3 Grid)

```
          ← Water source direction

┌─────────────────────────────────────┐
│  [6]      [7]      [8]              │  Row 3 (Y=30) - Top
│   │        │        │               │
│   │        │        │               │
│                                     │
│  [3]      [4]      [5]              │  Row 2 (Y=20) - Middle
│   │        │        │               │
│   │        │        │               │
│                                     │
│  [0]      [1]      [2]              │  Row 1 (Y=10) - Bottom
│   │        │        │               │
│   └────────┴────────┘               │
│    X=10   X=20   X=30               │
│                                     │
│         Drain/Slope →               │
└─────────────────────────────────────┘

Sensor Spacing: ~10 units (meters or feet)
```

---

## 4. Arduino IDE Setup

### Step-by-Step Configuration

1. **Open Arduino IDE**

2. **Load the Code:**
   - File → Open
   - Navigate to: [path where you saved the sketch files]
   - Open: `automatic_irrigation_esp32.ino`

3. **Select Board:**
   - Tools → Board → ESP32 Arduino → **ESP32 Dev Module**
   - (Or select your specific ESP32 board if different)

4. **Configure Board Settings:**
   ```
   Upload Speed: 115200
   CPU Frequency: 240MHz (WiFi/BT)
   Flash Frequency: 80MHz
   Flash Mode: QIO
   Flash Size: 4MB (32Mb)
   Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)
   Core Debug Level: None (or "Info" for debugging)
   PSRAM: Disabled
   ```

5. **Select COM Port:**
   - Tools → Port → Select the port showing "CP210x" or "CH340"
   - **Windows:** COM3, COM4, etc.
   - **Mac:** /dev/cu.usbserial-XXXX
   - **Linux:** /dev/ttyUSB0 or /dev/ttyACM0

---

## 5. Code Configuration

### STEP 1: Update WiFi Credentials

Find lines **94-95** in the code:

```cpp
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";
```

Replace with your actual WiFi credentials:

```cpp
const char* ssid = "MyHomeWiFi";
const char* password = "MySecurePassword123";
```

⚠️ **Important:** WiFi must be 2.4GHz (ESP32 doesn't support 5GHz)

### STEP 2: Verify Sensor Count

Line **88** should show:

```cpp
const int NUM_SENSORS = 9;
```

✅ This is correct for 9 sensors (3×3 grid)

### STEP 3: Check Pin Definitions

Lines **29-38** should show:

```cpp
const int RELAY_PIN = 23;

// Multiplexer Control Pins
const int MUX_S0_PIN = 19;
const int MUX_S1_PIN = 18;
const int MUX_S2_PIN = 21;

// Analog Reading Pins
const int MUX_COMMON_PIN = 34;
const int SENSOR_9_PIN = 35;
```

✅ These match the wiring schematic

### STEP 4: Initial Calibration Values (Optional - can calibrate later)

Lines **44-48**:

```cpp
int CALIBRATION_DRY = 3500; // Sensor in air
int CALIBRATION_WET = 1200; // Sensor in water
int DRY_THRESHOLD = 3000;   // Trigger irrigation above this
int WET_THRESHOLD = 1500;   // Stop irrigation below this
```

You can use these defaults initially and fine-tune after testing.

---

## 6. Code Upload Process

### STEP 1: Connect ESP32

1. Connect ESP32 to computer via USB cable
2. Wait for drivers to install (Windows may take 1-2 minutes)
3. Check Device Manager (Windows) or `ls /dev/tty*` (Mac/Linux) to confirm port

### STEP 2: Compile Code

1. Click **Verify** button (✓ checkmark icon) or press `Ctrl+R`
2. Wait for compilation (may take 1-2 minutes first time)
3. Check for errors in console output

**Expected Output:**
```
Sketch uses XXXXX bytes (XX%) of program storage space.
Global variables use XXXXX bytes (XX%) of dynamic memory.
```

### STEP 3: Upload Code

1. Click **Upload** button (→ arrow icon) or press `Ctrl+U`
2. Wait for upload progress:
   ```
   Connecting........_____.....
   Chip is ESP32-D0WDQ6 (revision 1)
   Writing at 0x00001000... (X %)
   ```
3. If connection fails:
   - Hold **BOOT** button on ESP32
   - Click Upload
   - Release BOOT when "Connecting..." appears

**Expected Output:**
```
Leaving...
Hard resetting via RTS pin...
```

### STEP 4: Open Serial Monitor

1. Tools → Serial Monitor (or `Ctrl+Shift+M`)
2. Set baud rate to **115200** at bottom-right
3. Set line ending to **Both NL & CR**

---

## 7. Initial Testing

### STEP 1: Verify Boot Sequence

After upload, Serial Monitor should show:

```
ESP32 Irrigation (Refactored) Initializing...
SPIFFS mounted successfully.
Configuration loaded successfully.
WiFi connecting to: YourWiFiSSID
WiFi connected! IP: 192.168.1.XXX
Web Server started.
Access dashboard at: http://192.168.1.XXX

===========================================
--- Power-On Self-Test: Sensor Config ---
===========================================
Pin Configuration:
  Relay: GPIO 23
  MUX S0: GPIO 19
  MUX S1: GPIO 18
  MUX S2: GPIO 21
  MUX Common: GPIO 34
  Direct ADC: GPIO 35

Sensor Mapping:
  MUX Ch0 -> S0 (10,10)
  MUX Ch1 -> S1 (20,10)
  MUX Ch2 -> S2 (30,10)
  MUX Ch3 -> S3 (10,20)
  MUX Ch4 -> S4 (20,20)
  MUX Ch5 -> S5 (30,20)
  MUX Ch6 -> S6 (10,30)
  MUX Ch7 -> S7 (20,30)
  Direct GPIO 35 -> S8 (30,30)
===========================================
```

✅ **Verify:**
- WiFi connected successfully
- IP address displayed
- All 9 sensors mapped correctly
- Sensor 8 shows "Direct GPIO 35"

### STEP 2: Test Web Dashboard

1. Note the IP address from Serial Monitor
2. Open web browser on same WiFi network
3. Navigate to: `http://192.168.1.XXX` (use your IP)
4. Verify dashboard loads with 3 pages:
   - 📊 Live Status
   - 🗺️ Moisture Map
   - ⚙️ Upload Config

### STEP 3: Test Sensor Readings (Without Wiring Yet)

In Serial Monitor, type commands:

```
S0:3500
```

Expected response:
```
SIM: Using value 3500 for Sensor 0
S0: 3500 (0%) - DRY
```

Test all sensors:
```
S1:3000
S2:2500
S3:2000
S4:1500
S5:1200
S6:2800
S7:2200
S8:3100
```

Verify web dashboard updates in real-time.

### STEP 4: Test Relay (Pump Control)

⚠️ **DO NOT connect actual pump yet!**

In Serial Monitor:
```
PUMP_ON
```

Expected response:
```
Manual PUMP_ON command received.
```

Check relay LED turns ON (or hear relay click).

Then:
```
PUMP_OFF
```

Expected response:
```
Manual PUMP_OFF command received.
```

---

## 8. Field Installation

### STEP 1: Power Down System

1. Disconnect USB cable
2. Disconnect any temporary power sources

### STEP 2: Assemble Hardware

#### A. CD4051 Multiplexer Setup

1. Place CD4051 on breadboard
2. Connect power:
   - Pin 16 (VCC) → ESP32 3.3V
   - Pin 8 (GND) → ESP32 GND
   - Pin 7 (VEE) → ESP32 GND
   - Pin 6 (INH) → ESP32 GND

3. Connect address lines:
   - Pin 11 (S0) → ESP32 GPIO 19
   - Pin 10 (S1) → ESP32 GPIO 18
   - Pin 9 (S2) → ESP32 GPIO 21

4. Connect common output:
   - Pin 3 (Z) → ESP32 GPIO 34

#### B. Sensor Connections

**Sensors 0-7 (via Multiplexer):**

For each sensor:
1. Connect sensor VCC to ESP32 3.3V rail
2. Connect sensor GND to ESP32 GND rail
3. Connect sensor SIGNAL to CD4051 Y# pin:

| Sensor | CD4051 Pin | Pin Number |
|--------|------------|------------|
| Sensor 0 | Y0 | Pin 13 |
| Sensor 1 | Y1 | Pin 14 |
| Sensor 2 | Y2 | Pin 15 |
| Sensor 3 | Y3 | Pin 12 |
| Sensor 4 | Y4 | Pin 1 |
| Sensor 5 | Y5 | Pin 5 |
| Sensor 6 | Y6 | Pin 2 |
| Sensor 7 | Y7 | Pin 4 |

**Sensor 8 (Direct Connection):**
1. VCC → ESP32 3.3V
2. GND → ESP32 GND
3. SIGNAL → ESP32 GPIO 35

#### C. Relay Module Setup

1. Connect relay VCC to ESP32 5V (or separate 5V supply)
2. Connect relay GND to ESP32 GND
3. Connect relay IN to ESP32 GPIO 23

4. **Pump connection (HIGH VOLTAGE - BE CAREFUL):**
   - Relay COM → Pump power supply (+) terminal
   - Relay NO → Pump motor (+) terminal
   - Pump motor (-) → Pump power supply (-) terminal

⚠️ **Safety:**
- Use proper insulation
- Keep high voltage away from low voltage circuits
- Consider using optoisolator relay for extra safety

### STEP 3: Physical Installation

#### A. Sensor Placement (3×3 Grid)

Using the field layout:

```
[6]    [7]    [8]     ← Top row (near water source)
[3]    [4]    [5]     ← Middle row
[0]    [1]    [2]     ← Bottom row (downslope)
```

1. **Measure field area**
2. **Mark sensor positions** (10m or 10ft spacing recommended)
3. **Install sensors:**
   - Insert vertically into soil
   - Ensure full probe depth (usually 5-10cm)
   - Avoid air gaps around probe
   - Route cables neatly to ESP32 location

4. **Label cables** clearly (S0, S1, ... S8)

#### B. Control Box Setup

1. Mount ESP32 + breadboard in weatherproof enclosure
2. Route sensor cables through cable glands
3. Mount relay module securely
4. Ensure proper ventilation for heat dissipation
5. Add power supply (battery + solar panel or AC adapter)

#### C. Pump Installation

1. Position pump near water source
2. Connect pump to relay via waterproof junction box
3. Route water line to irrigation area
4. Install check valve to prevent backflow
5. Test pump manually before connecting to system

---

## 9. System Calibration

### STEP 1: Power On System

1. Connect power to ESP32
2. Open Serial Monitor (115200 baud)
3. Verify boot sequence completes
4. Note IP address for web dashboard

### STEP 2: Read Initial Sensor Values

Wait for first automatic reading (every 60 seconds) or trigger manually:

In Serial Monitor, send:
```
PRINT
```

Expected output:
```
┌───────────────────────────────────────────────────────┐
│ REAL-TIME SENSOR STATUS                               │
└───────────────────────────────────────────────────────┘
S0 (10,10): 3245 = 10% - DRY
S1 (20,10): 3180 = 12% - DRY
S2 (30,10): 2950 = 20% - DRY
S3 (10,20): 2800 = 25% - Medium
S4 (20,20): 2650 = 30% - Medium
S5 (30,20): 2500 = 35% - Medium
S6 (10,30): 2200 = 45% - WET
S7 (20,30): 2100 = 48% - WET
S8 (30,30): 1950 = 52% - WET
```

### STEP 3: Calibrate DRY Value

1. **Remove one sensor from soil** (e.g., Sensor 0)
2. **Let it air dry** for 2-3 minutes
3. **Read value** in Serial Monitor
4. Note the value (e.g., 3500)

In Serial Monitor:
```
CONFIG_DRY:3500
```

Response:
```
Updated CALIBRATION_DRY to 3500
```

### STEP 4: Calibrate WET Value

1. **Submerge same sensor in water** for 30 seconds
2. **Read value** in Serial Monitor
3. Note the value (e.g., 1200)

In Serial Monitor:
```
CONFIG_WET:1200
```

Response:
```
Updated CALIBRATION_WET to 1200
```

### STEP 5: Set Thresholds

Based on your field conditions:

**DRY_THRESHOLD** (trigger irrigation when above this):
```
CONFIG_DRY_THRESH:3000
```

**WET_THRESHOLD** (stop irrigation when all sensors below this):
```
CONFIG_WET_THRESH:1500
```

### STEP 6: Save Configuration

All settings auto-save to SPIFFS. Verify by rebooting:

```
REBOOT
```

After reboot, check settings persist.

### STEP 7: Test Irrigation Cycle

**Method 1: Force cluster (recommended for testing):**

```
S0:3500
S1:3500
S3:3500
```

Expected behavior:
```
TOPOLOGY ALERT: Dry cluster of 3 - triggering irrigation!
Pump turned ON.
State: IRRIGATING
```

System will:
1. Run pump for minimum 5 minutes
2. Check sensors every 10 seconds
3. Turn off pump when all sensors wet
4. Enter 4-hour wait period

**Method 2: Manual pump test:**

```
PUMP_ON
```

Wait 30 seconds, then:

```
PUMP_OFF
```

Verify pump responds correctly.

---

## 10. Production Deployment

### STEP 1: Create config.json File

Create a file named `config.json` with optimal settings:

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

### STEP 2: Upload Configuration

**Via Web Dashboard:**
1. Open web dashboard: `http://192.168.1.XXX`
2. Click **⚙️ Upload Config** tab
3. Click **Choose File**
4. Select your `config.json` file
5. Click **Upload**
6. Verify success message

**Via Serial Monitor:**

Send entire config as one command:
```
CONFIG:{"CALIBRATION_DRY":3500,"CALIBRATION_WET":1200}
```

### STEP 3: Monitor First 24 Hours

1. **Check every 2 hours** for first day
2. **Monitor Serial output** for:
   - Sensor readings
   - Irrigation triggers
   - System faults
3. **Access web dashboard** remotely to view:
   - Live sensor status
   - Moisture heatmap
   - System state

### STEP 4: Verify Automation

Let system run autonomously for 3 days:

**Expected behavior:**
1. Monitors sensors every 60 seconds
2. Triggers irrigation when 3+ dry sensors clustered
3. Runs pump minimum 5 minutes
4. Checks moisture every 10 seconds during irrigation
5. Stops when all sensors below WET_THRESHOLD
6. Waits 4 hours before next possible irrigation
7. Detects irrigation failures (no moisture change after 3 checks)
8. Detects leaks (≥98% moisture)

### STEP 5: Adjust Timing (if needed)

Based on field observations:

**If irrigating too frequently:**
```
CONFIG_WAIT:21600000
```
(Increases wait time to 6 hours)

**If soil not getting wet enough:**
```
CONFIG_MIN_ON:600000
```
(Increases minimum pump time to 10 minutes)

**If irrigating too long:**
```
CONFIG_MAX_ON:2700000
```
(Decreases maximum pump time to 45 minutes)

---

## 11. Troubleshooting

### Issue: WiFi Not Connecting

**Symptoms:**
```
WiFi connecting...........
WiFi connection failed!
Operating without WiFi.
```

**Solutions:**
1. Check SSID and password spelling (case-sensitive)
2. Verify WiFi is 2.4GHz (not 5GHz)
3. Check WiFi router is powered on
4. Move ESP32 closer to router
5. Check router allows new device connections

### Issue: Sensor Readings Stuck at 0 or 4095

**Symptoms:**
```
S0: 0 (0%) - DRY
S1: 4095 (100%) - WET
```

**Solutions:**
1. **For 0 reading:**
   - Check sensor VCC connection
   - Verify sensor not shorted to GND
   - Test sensor with multimeter

2. **For 4095 reading:**
   - Check sensor SIGNAL wire connected
   - Verify MUX control pins wired correctly
   - Test with direct GPIO 35 connection

3. **Check wiring:**
   ```
   Sensor VCC → ESP32 3.3V ✓
   Sensor GND → ESP32 GND ✓
   Sensor SIGNAL → MUX Y# or GPIO 35 ✓
   ```

### Issue: Sensor 8 Not Working

**Symptoms:**
```
Direct GPIO 35 -> S8 (30,30)
S8: 0 (0%) - DRY
```

**Solutions:**
1. Verify GPIO 35 is input-only (cannot be output)
2. Check direct connection to GPIO 35 (not via MUX)
3. Test with another sensor to rule out hardware fault
4. Verify ADC attenuation set:
   ```cpp
   analogSetPinAttenuation(SENSOR_9_PIN, ADC_11db);
   ```

### Issue: Multiplexer Channels Mixed Up

**Symptoms:**
- Sensor readings swap when changing moisture
- Wrong sensor triggers irrigation

**Solutions:**
1. Verify CD4051 pin connections:
   ```
   S0 (Pin 11) → GPIO 19 ✓
   S1 (Pin 10) → GPIO 18 ✓
   S2 (Pin 9)  → GPIO 21 ✓
   ```

2. Check Y# connections match code:
   ```
   Y0 (Pin 13) → Sensor 0 ✓
   Y1 (Pin 14) → Sensor 1 ✓
   (etc.)
   ```

3. Run POST test and verify mapping

### Issue: Relay Not Switching

**Symptoms:**
```
Manual PUMP_ON command received.
(No relay click)
```

**Solutions:**
1. Check GPIO 23 connection to relay IN
2. Verify relay VCC connected to 5V (not 3.3V)
3. Test relay with external 5V source
4. Check relay LED indicator
5. Measure voltage at GPIO 23 (should be 3.3V when ON)
6. Try different GPIO pin if 23 is faulty

### Issue: Pump Runs But No Water

**Symptoms:**
```
Pump turned ON.
!!! WARNING: Soil is not getting wetter! (Check 1)
```

**Solutions:**
1. Check pump power supply voltage
2. Verify water source not empty
3. Check for clogged intake filter
4. Inspect water lines for leaks/kinks
5. Test pump separately with direct power
6. Verify relay switching high-voltage side correctly

### Issue: Irrigation Never Triggers

**Symptoms:**
- Sensors show dry values
- No irrigation cycle starts

**Solutions:**
1. Check cluster detection:
   ```
   S0:3500
   S1:3500
   S3:3500
   ```
   Should trigger if thresholds correct

2. Verify thresholds:
   ```
   DRY_THRESHOLD should be < CALIBRATION_DRY
   Example: DRY=3000, CALIBRATION_DRY=3500
   ```

3. Check MIN_DRY_SENSORS_TO_TRIGGER = 3

4. Verify sensors are neighbors (within 15 units distance)

### Issue: System Shows SYSTEM_FAULT

**Symptoms:**
```
State: SYSTEM_FAULT
```

**Causes:**
1. Irrigation failure (no moisture change after 3 checks)
2. Pump ran for MAX_PUMP_ON_TIME without wetting soil

**Recovery:**
1. Check Serial Monitor for specific error
2. Fix underlying issue (pump, sensors, etc.)
3. Send command:
   ```
   RESET_FAULT
   ```

### Issue: Web Dashboard Not Loading

**Symptoms:**
- Browser shows "Can't reach this page"

**Solutions:**
1. Verify ESP32 connected to WiFi:
   ```
   WiFi connected! IP: 192.168.1.XXX
   ```

2. Verify computer on same WiFi network
3. Try IP address directly (not hostname)
4. Clear browser cache
5. Try different browser
6. Ping ESP32 IP:
   ```bash
   ping 192.168.1.XXX
   ```

### Issue: Config Upload Fails

**Symptoms:**
```
Upload failed! File must be named config.json
```

**Solutions:**
1. Verify filename exactly `config.json` (no .txt)
2. Check JSON syntax with validator
3. Ensure file size < 3KB
4. Try uploading via Serial Monitor instead

---

## Serial Command Reference

### Sensor Simulation
```
S0:3500         # Set sensor 0 to 3500 (dry)
S8:1200         # Set sensor 8 to 1200 (wet)
```

### Pump Control
```
PUMP_ON         # Turn pump on manually
PUMP_OFF        # Turn pump off manually
```

### Configuration
```
CONFIG_DRY:3500             # Set dry calibration
CONFIG_WET:1200             # Set wet calibration
CONFIG_DRY_THRESH:3000      # Set dry threshold
CONFIG_WET_THRESH:1500      # Set wet threshold
CONFIG_MIN_ON:300000        # Min pump time (ms)
CONFIG_MAX_ON:3600000       # Max pump time (ms)
CONFIG_WAIT:14400000        # Post-irrigation wait (ms)
```

### System Commands
```
PRINT           # Print all sensor readings
REPORT          # Generate daily report
MONTHLY         # Generate monthly report
RESET_FAULT     # Clear system fault
REBOOT          # Restart ESP32
```

---

## Maintenance Schedule

### Daily (First Week)
- [ ] Check sensor readings via web dashboard
- [ ] Verify irrigation cycles complete successfully
- [ ] Monitor Serial output for errors

### Weekly
- [ ] Clean sensor probes (wipe with cloth)
- [ ] Check wiring connections for corrosion
- [ ] Verify pump operation
- [ ] Check water source level

### Monthly
- [ ] Re-calibrate sensors if readings drift
- [ ] Clean pump intake filter
- [ ] Inspect relay contacts
- [ ] Backup configuration file
- [ ] Review monthly reports for trends

### Seasonal
- [ ] Full system test before growing season
- [ ] Replace worn sensors or components
- [ ] Update firmware if new version available
- [ ] Adjust thresholds for seasonal conditions

---

## Support & Resources

### Documentation Files
- `CODE_REVIEW_FIXES.md` - Recent bug fixes
- `HYBRID_SENSOR_UPDATE.md` - Pin configuration details
- `automatic irrigation esp 32.ino` - Main code file

### Web Resources
- ESP32 Documentation: https://docs.espressif.com/
- Arduino Reference: https://www.arduino.cc/reference/
- ArduinoJson Library: https://arduinojson.org/

### Common Serial Monitor Commands
```
PRINT          - Show sensor readings
REPORT         - Daily statistics
MONTHLY        - Monthly statistics
CONFIG_DRY:X   - Set dry calibration
CONFIG_WET:X   - Set wet calibration
PUMP_ON/OFF    - Manual pump control
RESET_FAULT    - Clear fault state
REBOOT         - Restart system
```

---

## Success Checklist

### Hardware Assembly ✓
- [ ] ESP32 connected to computer
- [ ] CD4051 multiplexer wired correctly
- [ ] All 9 sensors connected (8 via MUX, 1 direct)
- [ ] Relay module connected to GPIO 23
- [ ] Pump connected to relay
- [ ] All grounds connected together
- [ ] Power supplies adequate for load

### Software Setup ✓
- [ ] Arduino IDE installed
- [ ] ESP32 board support installed
- [ ] Required libraries installed
- [ ] WiFi credentials configured
- [ ] Code compiled without errors
- [ ] Code uploaded successfully

### Initial Testing ✓
- [ ] Boot sequence completes
- [ ] WiFi connects successfully
- [ ] Web dashboard accessible
- [ ] All 9 sensors detected in POST
- [ ] Sensor readings respond to simulation
- [ ] Relay switches on command
- [ ] Pump responds to relay

### Calibration ✓
- [ ] DRY value calibrated (sensor in air)
- [ ] WET value calibrated (sensor in water)
- [ ] DRY_THRESHOLD set appropriately
- [ ] WET_THRESHOLD set appropriately
- [ ] Test irrigation cycle completes
- [ ] Configuration saved to SPIFFS

### Production Deployment ✓
- [ ] Sensors installed in field (3×3 grid)
- [ ] Cables routed and secured
- [ ] Control box weatherproofed
- [ ] Pump positioned and tested
- [ ] 24-hour monitoring completed
- [ ] Automation working correctly
- [ ] Emergency shutdown accessible

---

**Deployment Guide Complete!**

Version: 4.2.3
Last Updated: 2025-12-03

For questions or issues, refer to troubleshooting section or review Serial Monitor output for diagnostic information.
