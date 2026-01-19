# -IntelliFlow-Municipal-Water-Pump-Automation-Protection-System
IntelliFlow™ is an experimental municipal-grade water pump automation and protection system built on ESP32-P4. It integrates flow, pressure, cavitation, and dry-run monitoring to enable safe unattended operation, enhance water efficiency, protect motors, and improve reliability for household and small utility water systems.

**Author:** MD Hashim  
**Status:** R&D / Field-Tested Experimental Build  
**Platform:** ESP32 (Arduino Framework)  
**Operation:** Fully Offline (No Wi-Fi, No Cloud, No Blynk)

---

## Overview

This project implements a **municipal-grade automatic water pump control and protection system** using an ESP32.  
It is designed for **unattended operation**, prioritizing **motor safety, water efficiency, and system reliability** under unstable water supply conditions.

The controller continuously monitors **three water electrodes** (inlet, flow confirmation, and tank level) and operates the pump using a **deterministic, non-blocking state machine** with multiple layers of protection.

The design philosophy follows **industrial pump controller principles**, adapted for household and small-utility systems.

---

## Key Design Goals

- Eliminate manual pump operation  
- Prevent dry-run and no-flow damage  
- Ensure stable startup before motor engagement  
- Protect against intermittent water supply  
- Enable automatic fault recovery  
- Remain **fully offline** for maximum reliability  

---

## System Architecture

### State Machine

The firmware operates using **four well-defined system states**:

| State | Description |
|------|------------|
| `STATE_WAITING_FOR_WATER` | Monitors inlet water availability |
| `STATE_STARTUP_STABILIZATION` | Verifies stable water for 3 minutes |
| `STATE_PUMP_RUNNING` | Active pumping with live monitoring |
| `STATE_FAILURE_COOLDOWN` | Safety lockout with timed auto-reset |

This guarantees **predictable behavior** and **safe state transitions**.

---

## Core Features

### Multi-Layer Water Detection
Three conductive electrodes are used with **time-division ADC multiplexing**:

- **Electrode 1:** Inlet water detection  
- **Electrode 2:** Flow confirmation after pump start  
- **Electrode 3:** Tank full detection  

Only **one electrode is excited at a time**, eliminating ADC noise and cross-coupling.

---

### Startup Stabilization Logic
Before starting the pump:
- Water must be continuously present at inlet
- Verified for **3 minutes**
- Any interruption resets the timer

This prevents motor stress from unstable supply.

---

### Dry-Run & Flow Protection

While the pump is running:

- Electrode-2 not detected within **5 minutes** → shutdown  
- Single sensor loss → shutdown after **15 seconds**  
- Dual sensor loss → shutdown after **7 seconds**

This balances **false-trigger resistance** and **motor safety**.

---

### Tank Full Protection

- 3-second confirmation delay
- Pump stops cleanly
- **Non-blocking continuous buzzer** for 5 seconds

---

### Automatic Recovery (Fail-Safe)

After any shutdown:
- System enters **20-minute cooldown**
- Prevents short-cycling and overheating
- Automatically returns to waiting state

---

### Reset & Fault Awareness

- ESP32 reset reason is analyzed on boot
- Unexpected resets trigger **1-hour pulsing error LED**
- Helps diagnose brownouts, watchdogs, or crashes

---

### Non-Blocking Architecture

- `millis()`-based timing throughout
- No blocking delays in control logic
- Buzzer alarms handled asynchronously
- Suitable for long-term unattended operation

---

## Hardware Requirements

### Controller
- ESP32 development board (tested on ESP32 Dev modules)

### Sensors
- 3 × conductive water electrodes  
- External resistor network (recommended)

### Outputs
- Relay or contactor (with opto-isolation)
- Piezo buzzer
- Status LEDs (recommended)

### Power
- Stable regulated supply (5V / 3.3V)
- Brownout protection strongly recommended

---

## GPIO Mapping

### Electrode Excitation

| Function | GPIO |
|--------|------|
| Electrode 1 | GPIO 21 |
| Electrode 2 | GPIO 22 |
| Electrode 3 | GPIO 23 |

### ADC Inputs

| Function | GPIO |
|--------|------|
| Inlet (E1) | GPIO 32 |
| Flow (E2) | GPIO 33 |
| Tank Full (E3) | GPIO 34 (Input-only) |

### Outputs

| Function | GPIO |
|--------|------|
| Pump Relay | GPIO 18 |
| Buzzer | GPIO 19 |

### LEDs

| Indicator | GPIO |
|---------|------|
| Electrode 1 Status | GPIO 25 |
| Electrode 2 Status | GPIO 26 |
| Tank Full Status | GPIO 27 |
| Stabilization Phase | GPIO 4 |
| Flow-Wait Phase | GPIO 16 |
| Auto-Reset Phase | GPIO 17 |
| Error / Reset LED | GPIO 2 |

> All GPIOs are chosen to **avoid ESP32 strapping pins**.

---

## LED Indications

| LED Color | Meaning |
|---------|---------|
| Blue | Startup stabilization active |
| Yellow | Waiting for flow confirmation |
| Red | Failure cooldown / auto-reset |
| Error LED | Unexpected system reset |

---

## Serial Diagnostics

The firmware provides detailed serial logs:
- Current system state
- Raw ADC values
- Timing countdowns
- Reset causes
- Active alarms

Useful for debugging and field tuning.

---

## Safety Disclaimer

⚠ **WARNING**

This firmware controls **high-voltage and high-power equipment**.

- Use proper electrical isolation
- Use relays/contactors rated for motor load
- Add fuses, MCBs, and surge protection
- Never connect pumps directly to GPIO pins

The author assumes **no liability** for misuse or damage.

---

## License

This project is licensed under the **Apache License, Version 2.0**.

You may use, modify, and distribute this software in compliance with the License.
The license also provides an explicit grant of patent rights and includes
limitations of liability.

See the `LICENSE` file for full details.

---

## Project Status & Future Work

- ✔ Field-tested in household environment  
- ✔ Stable long-duration operation  
- 🔧 Planned: pressure sensor integration  
- 🔧 Planned: data logging & statistics  
- 🔧 Planned: configurable thresholds  

---

## Author

**MD Hashim**  
Mechanical Engineer | Embedded Automation | Control Systems
