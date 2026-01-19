# -IntelliFlow-Municipal-Water-Pump-Automation-Protection-System
IntelliFlow™ is an experimental municipal-grade water pump automation and protection system built on ESP32-P4. It integrates flow, pressure, cavitation, and dry-run monitoring to enable safe unattended operation, enhance water efficiency, protect motors, and improve reliability for household and small utility water systems.

Author: MD Hashim
Status: R&D / Field-tested Experimental Build
Platform: ESP32 (Arduino framework)
Operation: Fully Offline (No Wi-Fi, No Cloud, No Blynk)

Overview

This project implements a municipal-grade automatic water pump control and protection system using an ESP32.
It is designed for unattended operation, prioritizing motor safety, water efficiency, and system reliability under unstable supply conditions.

The controller continuously monitors three water electrodes (inlet, flow confirmation, and tank level) and operates a pump using a deterministic, non-blocking state machine with multiple layers of fault protection.

The design philosophy mirrors industrial pump controllers, adapted for household and small utility systems.

Key Design Goals

Eliminate manual pump operation

Prevent dry-run and no-flow damage

Ensure stable startup before motor engagement

Protect against intermittent water supply

Recover automatically from fault conditions

Remain fully offline for maximum reliability

Core Features
1. Deterministic State Machine

The firmware operates using four well-defined system states:

State	Description
WAITING_FOR_WATER	Monitors inlet availability (Electrode 1)
STARTUP_STABILIZATION	Verifies stable water presence for 3 minutes
PUMP_RUNNING	Active pumping with continuous monitoring
FAILURE_COOLDOWN	Safety lockout and timed auto-reset

This guarantees predictable behavior and safe transitions.

2. Multi-Layer Water Detection

Three electrodes are used with time-division ADC multiplexing:

Electrode 1: Inlet water availability

Electrode 2: Flow confirmation after pump start

Electrode 3: Tank full detection

Each electrode is excited independently to eliminate ADC cross-coupling and noise.

3. Startup Stabilization Logic

Before starting the pump:

Water must be continuously present at the inlet

Verified for 3 minutes

Any interruption resets the timer

This prevents motor stress from unstable supply.

4. Dry-Run & Flow Protection

While running:

If Electrode 2 is not detected within 5 minutes, pump shuts down

Single sensor loss → shutdown after 15 seconds

Dual sensor loss → shutdown after 7 seconds

These thresholds balance false positives vs motor safety.

5. Tank Full Protection

When the tank-full electrode is detected:

3-second confirmation delay

Pump stops cleanly

Non-blocking continuous buzzer alarm for 5 seconds

6. Automatic Recovery (Fail-Safe)

After any shutdown:

System enters 20-minute cooldown

Prevents short-cycling and motor overheating

Automatically resets to waiting state afterward

7. Reset & Fault Awareness

On every boot:

ESP32 reset reason is analyzed

Unexpected resets trigger a 1-hour pulsing error LED

Helps diagnose brownouts, watchdog resets, or crashes

8. Fully Non-Blocking Architecture

millis()-based timing throughout

No long blocking delays in control logic

Buzzer alarms handled asynchronously

Suitable for long-term unattended operation

Hardware Requirements
Controller

ESP32 (tested with ESP32 Dev modules)

Sensors

Conductive water electrodes (3)

External resistor network (recommended)

Outputs

Relay or contactor (with opto-isolation)

Piezo buzzer

Status LEDs (optional but recommended)

Power

Stable 5V / 3.3V regulated supply

Brownout protection strongly recommended

GPIO Mapping
Electrode Excitation
Function	GPIO
Electrode 1	GPIO 21
Electrode 2	GPIO 22
Electrode 3	GPIO 23
ADC Inputs
Function	GPIO
Inlet (E1)	GPIO 32
Flow (E2)	GPIO 33
Tank Full (E3)	GPIO 34 (input-only)
Outputs
Function	GPIO
Pump Relay	GPIO 18
Buzzer	GPIO 19
LEDs
Indicator	GPIO
Electrode 1	GPIO 25
Electrode 2	GPIO 26
Tank Full	GPIO 27
Stabilization Phase	GPIO 4
Flow-Wait Phase	GPIO 16
Auto-Reset Phase	GPIO 17
Error / Reset LED	GPIO 2

All GPIOs were selected to avoid ESP32 strapping pins.

LED Indications
LED Color	Meaning
Blue	Startup stabilization active
Yellow	Waiting for flow confirmation
Red	Failure cooldown / auto-reset
Error LED	Unexpected system reset
Serial Diagnostics

The firmware provides detailed serial logs:

System state

Raw ADC values

Timing countdowns

Reset causes

Active alarms

Useful for tuning thresholds and debugging installations.

Safety Disclaimer

⚠ WARNING

This firmware controls high-voltage and high-power equipment.

Use proper electrical isolation

Use contactors/relays rated for motor load

Add fuses, MCBs, and surge protection

Do not connect pumps directly to GPIO pins

The author assumes no liability for misuse, damage, or injury.

License
This project is released under the MIT License.
You are free to use, modify, and distribute the code with attribution.

Project Status & Future Work

✔ Field-tested in household environment
✔ Stable long-duration operation
🔧 Future: pressure sensor integratio
🔧 Future: data logging / statistics
🔧 Future: configurable thresholds

Author

MD Hashim
Mechanical Engineer | Embedded Automation | Control Systems
