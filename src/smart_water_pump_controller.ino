/*
 * SMART WATER PUMP CONTROLLER - PRODUCTION READY
 * Author: MD Hashim
 * OPTIMAL GPIO SELECTION: 4, 16, 17 for phase LEDs
 * FIXED: ADC timing and multiplexing issues
 * FIXED: Non-blocking buzzer for continuous alarms
 * FULLY OFFLINE - No Blynk dependencies
 */


/*
 * SYSTEM STATE MACHINE
 * Four operational states with clear transitions
 */
enum SystemState {
  STATE_WAITING_FOR_WATER,       // Initial state - Monitoring Electrode 1
  STATE_STARTUP_STABILIZATION,   // 3-minute water stability verification
  STATE_PUMP_RUNNING,            // Active pumping with flow monitoring
  STATE_FAILURE_COOLDOWN         // Safety timeout after any shutdown
};


// Forward declarations
String getResetReason(esp_reset_reason_t reason);
String getStateName(SystemState state);


/*
 * HARDWARE PIN DEFINITIONS
 * Electrode excitation and sensing pins
 */
#define EXCITE1 21
#define EXCITE2 22
#define EXCITE3 23


#define FLOW_ADC1 32  // ADC1_CH4 - Electrode 1 (Inlet detection)
#define FLOW_ADC2 33  // ADC1_CH5 - Electrode 2 (Flow confirmation)  
#define TANK_ADC  34  // ADC1_CH6 - Electrode 3 (Tank full) - Input only


/*
 * STATUS INDICATOR LEDS
 * Electrode status monitoring - INVERTED LOGIC (LED OFF when water detected)
 */
#define LED1 25  // Electrode 1 status
#define LED2 26  // Electrode 2 status  
#define LED3 27  // Electrode 3 status


/*
 * PHASE INDICATION LEDs - OPTIMAL SAFE GPIOs
 * Visual indicators for timing phases
 */
#define PHASE_STABILIZATION_LED 4    // GPIO 4 - 100% safe - BLUE: 3-min stabilization
#define PHASE_ELECTRODE2_WAIT_LED 16 // GPIO 16 - 100% safe - YELLOW: 5-min E2 wait  
#define PHASE_AUTORESET_LED 17       // GPIO 17 - 100% safe - RED: 20-min auto-reset


/*
 * ERROR LED - System reset indicator
 * GPIO 2 (Built-in LED) - Perfect for error indication
 */
#define ERROR_LED 2  // Pulses when system reset detected


/*
 * CONTROL OUTPUTS
 * Relay for pump control, Buzzer for audible feedback
 */
#define RELAY 18   // Pump control relay
#define BUZZER 19  // Piezo buzzer for status feedback


/*
 * TIMING PARAMETERS - All values in milliseconds
 * Critical timing constants for system operation
 */
const int threshold1 = 1000;    // Water detection threshold - INVERTED LOGIC
const int threshold2 = 1000;    // (Reading < threshold = WATER detected)  
const int threshold3 = 1000;    


const unsigned long STABILIZATION_DELAY = 180000;        // 3 minutes - Electrode 1 stability
const unsigned long MAX_WAIT_FOR_ELECTRODE2 = 300000;    // 5 minutes - Max wait for flow confirmation
const unsigned long SINGLE_LOSS_DELAY = 15000;           // 15 seconds - Single sensor loss timeout
const unsigned long DUAL_LOSS_DELAY = 7000;              // 7 seconds - Dual sensor loss timeout
const unsigned long TANK_FULL_STOP_DELAY = 3000;         // 3 seconds - Tank full confirmation delay
const unsigned long AUTO_RESET_DELAY = 1200000;          // 20 minutes - System recovery period


/*
 * ERROR LED TIMING - Non-blocking pulse pattern
 * 500ms ON, 1000ms OFF for 1 hour after system reset
 */
const unsigned long ERROR_LED_DURATION = 3600000;        // 1 hour error indication
const unsigned long ERROR_LED_ON_TIME = 500;             // 500ms ON time
const unsigned long ERROR_LED_OFF_TIME = 1000;           // 1000ms OFF time


// Global system state variable
SystemState systemState = STATE_WAITING_FOR_WATER;


/*
 * TIMING CONTROL VARIABLES
 * Millis-based non-blocking timing for all operations
 */
unsigned long water1DetectedTime = 0;    // When Electrode 1 first detected water
unsigned long pumpStartTime = 0;         // When pump was started
unsigned long lastWater1Time = 0;        // Last time Electrode 1 had water
unsigned long lastWater2Time = 0;        // Last time Electrode 2 had water
unsigned long bothLostTime = 0;          // When both sensors lost water
unsigned long tankFullDetectedTime = 0;  // When tank full was detected
unsigned long failureStartTime = 0;      // When failure cooldown started


/*
 * ERROR TRACKING VARIABLES
 * System reset detection and error LED control
 */
unsigned long errorLedStartTime = 0;     // When error LED was activated
bool systemResetDetected = false;        // System reset detected flag
bool errorLedActive = false;             // Error LED currently active


/*
 * NEW: NON-BLOCKING BUZZER CONTROL VARIABLES
 * Continuous alarm timing without blocking the main loop
 */
unsigned long continuousBuzzerStartTime = 0;
unsigned long continuousBuzzerDuration = 0;
bool continuousBuzzerActive = false;


/*
 * OPERATIONAL STATE FLAGS
 * Track current system conditions
 */
bool bothLost = false;           // Both sensors currently without water
bool tankFullTriggered = false;  // Tank full condition detected


/*
 * PHASE TRACKING VARIABLES
 * Monitor active timing phases for LED indication
 */
bool stabilizationPhaseActive = false;   // 3-min stabilization in progress
bool electrode2WaitPhaseActive = false;  // 5-min E2 wait in progress
bool autoResetPhaseActive = false;       // 20-min auto-reset in progress


/*
 * SETUP FUNCTION - Hardware initialization
 * Configures pins and detects reset cause
 */
void setup() {
  Serial.begin(115200);


  /*
   * PIN MODE CONFIGURATION
   * Set all GPIO directions before use
   */
  pinMode(EXCITE1, OUTPUT);
  pinMode(EXCITE2, OUTPUT);
  pinMode(EXCITE3, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(BUZZER, OUTPUT);
 
  // Electrode status LEDs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
 
  // Phase indication LEDs
  pinMode(PHASE_STABILIZATION_LED, OUTPUT);
  pinMode(PHASE_ELECTRODE2_WAIT_LED, OUTPUT);
  pinMode(PHASE_AUTORESET_LED, OUTPUT);


  // Error LED
  pinMode(ERROR_LED, OUTPUT);


  /*
   * INITIAL STATE CONFIGURATION
   * Ensure all outputs start in safe state
   */
  digitalWrite(RELAY, LOW);    // Pump OFF
  digitalWrite(BUZZER, LOW);   // Buzzer OFF
 
  // Electrode status LEDs - INVERTED LOGIC (ON when no water)
  digitalWrite(LED1, HIGH);    // No water at E1
  digitalWrite(LED2, HIGH);    // No water at E2  
  digitalWrite(LED3, HIGH);    // Tank not full
 
  // Phase indication LEDs - ALL OFF initially
  digitalWrite(PHASE_STABILIZATION_LED, LOW);
  digitalWrite(PHASE_ELECTRODE2_WAIT_LED, LOW);
  digitalWrite(PHASE_AUTORESET_LED, LOW);


  // Error LED - OFF initially
  digitalWrite(ERROR_LED, LOW);


  /*
   * SYSTEM BOOT ANALYSIS
   * Detect system resets for stability monitoring
   */
  checkResetCause();


  /*
   * SYSTEM STARTUP MESSAGES
   * Clear status output for debugging
   */
  Serial.println();
  Serial.println("=== SMART PUMP CONTROLLER - PRODUCTION READY ===");
  Serial.println("OPTIMAL GPIO SELECTION: 4, 16, 17 for phase LEDs");
  Serial.println("Phase LED Indicators:");
  Serial.println("- BLUE (GPIO 4): 3-minute stabilization active");
  Serial.println("- YELLOW (GPIO 16): 5-minute E2 wait active");
  Serial.println("- RED (GPIO 17): 20-minute auto-reset active");
  Serial.println("Error LED: GPIO 2 - Pulses if system reset detected");
  Serial.println("ADC Timing: Fixed multiplexing with proper isolation");
  Serial.println("Buzzer: Non-blocking continuous alarm for tank full");
 
  if (systemResetDetected) {
    Serial.println("⚠ SYSTEM RESET DETECTED - Previous session ended unexpectedly");
    Serial.println("🔴 ERROR LED will pulse for 1 hour (500ms ON, 1000ms OFF)");
  } else {
    Serial.println("✅ Clean startup - Normal power-on detected");
  }
 
  Serial.println();
  Serial.println("State: Waiting for water at inlet...");
}


/*
 * CHECK RESET CAUSE - Detect system resets
 * Analyzes why ESP32 rebooted to detect unexpected restarts
 */
void checkResetCause() {
  esp_reset_reason_t resetReason = esp_reset_reason();
 
  Serial.println("=== SYSTEM BOOT ANALYSIS ===");
  Serial.println("Reset Reason: " + String(getResetReason(resetReason)));
 
  // Consider any non-power-on reset as potentially unexpected
  if (resetReason != ESP_RST_POWERON && resetReason != ESP_RST_UNKNOWN) {
    systemResetDetected = true;
    errorLedActive = true;
    errorLedStartTime = millis();
    Serial.println("❌ SYSTEM RESET DETECTED - Previous session ended unexpectedly");
  } else {
    systemResetDetected = false;
    errorLedActive = false;
    Serial.println("✅ Normal power-on sequence detected");
  }
  Serial.println("============================");
}


/*
 * FIXED ADC READING FUNCTIONS
 * Proper time-division multiplexing with minimal noise
 */


/*
 * READ SINGLE ELECTRODE - Clean excitation and reading
 * Only toggles the specific excitation pin needed
 */
int readSingleElectrode(int excitePin, int adcPin) {
  // Turn ON only the specific electrode's excitation pin
  digitalWrite(excitePin, HIGH);
 
  // Short delay for voltage stabilization (500μs is sufficient)
  delayMicroseconds(500);


  // Read the ADC value
  int reading = analogRead(adcPin);
 
  // Turn OFF the excitation pin immediately after reading
  digitalWrite(excitePin, LOW);
 
  return reading;
}


/*
 * READ ALL ELECTRODES - Proper time-division multiplexing with averaging
 * Ensures only one electrode is active at any time
 */
void readAllElectrodes(int &water1, int &water2, int &water3) {
  long sum1 = 0, sum2 = 0, sum3 = 0;
  const int num_samples = 3; // 3-sample averaging for noise reduction


  for (int i = 0; i < num_samples; i++) {
    // Read Electrode 1 with proper isolation
    sum1 += readSingleElectrode(EXCITE1, FLOW_ADC1);
    delay(5); // 5ms gap between electrode readings
   
    // Read Electrode 2 with proper isolation  
    sum2 += readSingleElectrode(EXCITE2, FLOW_ADC2);
    delay(5); // 5ms gap between electrode readings
   
    // Read Electrode 3 with proper isolation
    sum3 += readSingleElectrode(EXCITE3, TANK_ADC);
    delay(5); // 5ms gap before next sample cycle
  }
 
  // Return averaged values
  water1 = sum1 / num_samples;
  water2 = sum2 / num_samples;
  water3 = sum3 / num_samples;
}


/*
 * MAIN LOOP - Core operational logic
 * Completely non-blocking state machine with precise timing
 */
void loop() {
  // Handle error LED pulsing (non-blocking)
  handleErrorLED();


  // NEW: Handle non-blocking continuous buzzer
  handleBuzzerTiming();


  // Read all electrodes with FIXED time-division multiplexing
  int water1, water2, water3;
  readAllElectrodes(water1, water2, water3);


  // INVERTED LOGIC - Electrodes read HIGH when dry, LOW when wet
  bool waterPresent1 = (water1 < threshold1);
  bool waterPresent2 = (water2 < threshold2);  
  bool tankFull = (water3 < threshold3);


  // Update electrode status LEDs - INVERTED: LED OFF when water detected
  digitalWrite(LED1, !waterPresent1);
  digitalWrite(LED2, !waterPresent2);
  digitalWrite(LED3, !tankFull);


  // Update phase indication LEDs based on current state
  updatePhaseLEDs(waterPresent1, waterPresent2);


  // Handle audible feedback for system events
  handleBuzzerFeedback(waterPresent1, waterPresent2, tankFull);


  /*
   * MAIN STATE MACHINE
   * Four states with clear transitions and timing
   */
  switch (systemState) {
    case STATE_WAITING_FOR_WATER:
      handleWaitingForWater(waterPresent1);
      break;
     
    case STATE_STARTUP_STABILIZATION:
      handleStartupStabilization(waterPresent1, waterPresent2);
      break;
     
    case STATE_PUMP_RUNNING:
      handlePumpRunning(waterPresent1, waterPresent2, tankFull);
      break;
     
    case STATE_FAILURE_COOLDOWN:
      handleFailureCooldown();
      break;
  }


  // Status display every 10 seconds (non-blocking)
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 10000) {
    printSystemStatus(waterPresent1, waterPresent2, tankFull, water1, water2, water3);
    lastStatus = millis();
  }


  delay(100); // Main loop delay for stability
}


/*
 * HANDLE ERROR LED - Non-blocking pulse control
 * 500ms ON, 1000ms OFF for 1 hour after system reset
 */
void handleErrorLED() {
  if (!errorLedActive) return;
 
  // Check if 1-hour error indication period has elapsed
  if (millis() - errorLedStartTime > ERROR_LED_DURATION) {
    digitalWrite(ERROR_LED, LOW);
    errorLedActive = false;
    Serial.println("✅ ERROR LED OFF - 1-hour indication period complete");
    return;
  }
 
  // Non-blocking pulse: 500ms ON, 1000ms OFF
  unsigned long currentTime = millis();
  unsigned long cycleTime = (currentTime - errorLedStartTime) % (ERROR_LED_ON_TIME + ERROR_LED_OFF_TIME);
 
  if (cycleTime < ERROR_LED_ON_TIME) {
    digitalWrite(ERROR_LED, HIGH);
  } else {
    digitalWrite(ERROR_LED, LOW);
  }
}


/*
 * NEW: HANDLE BUZZER TIMING - Non-blocking control for continuous alarm
 * Prevents 5-second blocking during tank full notification
 */
void handleBuzzerTiming() {
  if (!continuousBuzzerActive) return;


  // Check if the continuous beep duration has elapsed
  if (millis() - continuousBuzzerStartTime >= continuousBuzzerDuration) {
    digitalWrite(BUZZER, LOW);
    continuousBuzzerActive = false;
    continuousBuzzerDuration = 0;
    Serial.println("🔇 Continuous Buzzer alarm stopped.");
  } else {
    // Keep buzzer HIGH while active (piezo buzzer can be held HIGH)
    digitalWrite(BUZZER, HIGH);
  }
}


/*
 * UPDATE PHASE LEDs - Visual timing indicators
 * Shows active timing phases with color-coded LEDs
 */
void updatePhaseLEDs(bool waterPresent1, bool waterPresent2) {
  // Reset all phase LEDs first
  digitalWrite(PHASE_STABILIZATION_LED, LOW);
  digitalWrite(PHASE_ELECTRODE2_WAIT_LED, LOW);
  digitalWrite(PHASE_AUTORESET_LED, LOW);
 
  // Activate appropriate phase LED based on current state
  switch (systemState) {
    case STATE_STARTUP_STABILIZATION:
      // BLUE LED: 3-minute stabilization active
      digitalWrite(PHASE_STABILIZATION_LED, HIGH);
      stabilizationPhaseActive = true;
      electrode2WaitPhaseActive = false;
      autoResetPhaseActive = false;
      break;
     
    case STATE_PUMP_RUNNING:
      if (!waterPresent2 && (millis() - pumpStartTime < MAX_WAIT_FOR_ELECTRODE2)) {
        // YELLOW LED: 5-minute E2 wait active (only if E2 not detected yet)
        digitalWrite(PHASE_ELECTRODE2_WAIT_LED, HIGH);
        electrode2WaitPhaseActive = true;
      } else {
        // No phase LED when E2 detected or wait period expired
        electrode2WaitPhaseActive = false;
      }
      stabilizationPhaseActive = false;
      autoResetPhaseActive = false;
      break;
     
    case STATE_FAILURE_COOLDOWN:
      // RED LED: 20-minute auto-reset active
      digitalWrite(PHASE_AUTORESET_LED, HIGH);
      autoResetPhaseActive = true;
      stabilizationPhaseActive = false;
      electrode2WaitPhaseActive = false;
      break;
     
    case STATE_WAITING_FOR_WATER:
    default:
      // All phase LEDs OFF
      stabilizationPhaseActive = false;
      electrode2WaitPhaseActive = false;
      autoResetPhaseActive = false;
      break;
  }
}


void handleWaitingForWater(bool waterPresent1) {
  if (waterPresent1) {
    systemState = STATE_STARTUP_STABILIZATION;
    water1DetectedTime = millis();
    Serial.println("✅ Electrode 1: Water detected!");
    Serial.println("💙 BLUE LED ON (GPIO 4): 3-minute stabilization started");
    Serial.println("→ State: Startup Stabilization");
    beep(1);
  }
}


void handleStartupStabilization(bool waterPresent1, bool waterPresent2) {
  if (!waterPresent1) {
    systemState = STATE_WAITING_FOR_WATER;
    Serial.println("❌ WATER LOST - Returning to Waiting state");
    Serial.println("💙 BLUE LED OFF: Stabilization cancelled");
    Serial.println("→ State: Waiting for Water");
    beep(2);
    return;
  }


  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - water1DetectedTime;


  if (elapsed >= STABILIZATION_DELAY) {
    digitalWrite(RELAY, HIGH);
    systemState = STATE_PUMP_RUNNING;
    pumpStartTime = currentTime;
    lastWater1Time = currentTime;
    lastWater2Time = currentTime;
   
    Serial.println("🎉 PUMP STARTED! 3-minute stabilization complete");
    Serial.println("💙 BLUE LED OFF: Stabilization complete");
    Serial.println("💛 YELLOW LED ON (GPIO 16): Waiting for Electrode 2 detection (5 minutes max)");
    Serial.println("→ State: Pump Running");
    beep(3);
  }
  else {
    unsigned long remaining = (STABILIZATION_DELAY - elapsed) / 1000;
    static unsigned long lastProgress = 0;
    if (currentTime - lastProgress > 30000) {
      Serial.println("⏳ Stabilization: " + String(remaining) + "s remaining | 💙 BLUE LED ACTIVE");
      lastProgress = currentTime;
    }
  }
}


void handlePumpRunning(bool waterPresent1, bool waterPresent2, bool tankFull) {
  unsigned long currentTime = millis();
 
  if (waterPresent1) lastWater1Time = currentTime;
  if (waterPresent2) lastWater2Time = currentTime;


  static bool lastWater2State = false;
  if (waterPresent2 && !lastWater2State) {
    Serial.println("✅ Electrode 2: Flow confirmed!");
    Serial.println("💛 YELLOW LED OFF: Electrode 2 detected successfully");
    digitalWrite(PHASE_ELECTRODE2_WAIT_LED, LOW);
    electrode2WaitPhaseActive = false;
  }
  lastWater2State = waterPresent2;


  if (!waterPresent2 && (currentTime - pumpStartTime >= MAX_WAIT_FOR_ELECTRODE2)) {
    Serial.println("💛 YELLOW LED OFF: Electrode 2 wait timeout");
    stopPump("Electrode 2 not detected within 5 minutes");
    return;
  }


  if (tankFull && !tankFullTriggered) {
    tankFullTriggered = true;
    tankFullDetectedTime = currentTime;
    Serial.println("⚠ Tank full detected - Stopping pump in 3 seconds");
  }


  if (tankFullTriggered && (currentTime - tankFullDetectedTime >= TANK_FULL_STOP_DELAY)) {
    stopPump("Tank full");
    return;
  }


  bool currentBothLost = !waterPresent1 && !waterPresent2;
  if (currentBothLost && !bothLost) {
    bothLost = true;
    bothLostTime = currentTime;
    Serial.println("⚠ Dual loss detected - Stopping pump in 7 seconds");
  }


  if (bothLost && (currentTime - bothLostTime >= DUAL_LOSS_DELAY)) {
    stopPump("Both sensors lost water for 7 seconds");
  }
  else if (!waterPresent1 && (currentTime - lastWater1Time >= SINGLE_LOSS_DELAY)) {
    stopPump("Electrode 1 lost water for 15 seconds");
  }
  else if (!waterPresent2 && (currentTime - lastWater2Time >= SINGLE_LOSS_DELAY)) {
    stopPump("Electrode 2 lost water for 15 seconds");
  }
}


void stopPump(String reason) {
  digitalWrite(RELAY, LOW);
  systemState = STATE_FAILURE_COOLDOWN;
  failureStartTime = millis();
  bothLost = false;
  tankFullTriggered = false;
 
  digitalWrite(PHASE_STABILIZATION_LED, LOW);
  digitalWrite(PHASE_ELECTRODE2_WAIT_LED, LOW);
 
  Serial.println("❌ Pump STOPPED - " + reason);
  Serial.println("❤️ RED LED ON (GPIO 17): 20-minute auto-reset started");
  Serial.println("→ State: Failure Cooldown");
 
  if (reason == "Tank full") {
    // NEW: Non-blocking continuous buzzer activation
    continuousBuzzerActive = true;
    continuousBuzzerStartTime = millis();
    continuousBuzzerDuration = 5000;
    Serial.println("🔊 Continuous 5-second alarm activated (non-blocking)");
  } else {
    // Short blocking beep (0.5s) - acceptable for transient feedback
    beep(2);
  }
}


void handleFailureCooldown() {
  if (millis() - failureStartTime >= AUTO_RESET_DELAY) {
    systemState = STATE_WAITING_FOR_WATER;
    Serial.println("🔄 System AUTO-RESET after 20 minutes");
    Serial.println("❤️ RED LED OFF: Auto-reset complete");
    Serial.println("→ State: Waiting for water");
    beep(1);
  }
}


void handleBuzzerFeedback(bool water1, bool water2, bool tankFull) {
  static bool buzzed1 = false, buzzed2 = false, buzzed3 = false;
 
  if (water1 && !buzzed1) {
    buzzed1 = true;
  }
 
  if (water2 && !buzzed2) {
    beep(2);
    buzzed2 = true;
    Serial.println("✓ Electrode 2: Water flow confirmed!");
  }
 
  if (tankFull && !buzzed3) {
    beep(3);
    buzzed3 = true;
    Serial.println("⚠ Tank full condition detected!");
  }
 
  if (!water1) buzzed1 = false;
  if (!water2) buzzed2 = false;
  if (!tankFull) buzzed3 = false;
}


void beep(int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(150);
    digitalWrite(BUZZER, LOW);
    if (i < count - 1) delay(200);
  }
}


void printSystemStatus(bool water1, bool water2, bool tankFull, int raw1, int raw2, int raw3) {
  Serial.println("--- System Status ---");
  Serial.println("State: " + getStateName(systemState));
  Serial.println("Electrode 1: " + String(raw1) + " = " + (water1 ? "WATER" : "DRY"));
  Serial.println("Electrode 2: " + String(raw2) + " = " + (water2 ? "WATER" : "DRY"));
  Serial.println("Electrode 3: " + String(raw3) + " = " + (tankFull ? "FULL" : "NOT FULL"));
  Serial.println("Pump: " + String(digitalRead(RELAY) ? "ON" : "OFF"));
 
  Serial.println("Phase LEDs: " +
    String(digitalRead(PHASE_STABILIZATION_LED) ? "BLUE" : "") + " " +
    String(digitalRead(PHASE_ELECTRODE2_WAIT_LED) ? "YELLOW" : "") + " " +
    String(digitalRead(PHASE_AUTORESET_LED) ? "RED" : ""));
 
  if (errorLedActive) {
    Serial.println("🔴 ERROR LED: ACTIVE (System reset detected)");
  }
 
  if (continuousBuzzerActive) {
    unsigned long remaining = (continuousBuzzerDuration - (millis() - continuousBuzzerStartTime)) / 1000;
    Serial.println("🔊 BUZZER: ACTIVE (" + String(remaining) + "s remaining)");
  }
 
  if (systemState == STATE_STARTUP_STABILIZATION) {
    unsigned long elapsed = millis() - water1DetectedTime;
    unsigned long remaining = (STABILIZATION_DELAY - elapsed) / 1000;
    Serial.println("Stabilization: " + String(remaining) + "s remaining");
  }
  else if (systemState == STATE_PUMP_RUNNING) {
    unsigned long pumpTime = (millis() - pumpStartTime) / 1000;
    Serial.println("Pump running: " + String(pumpTime) + "s");
   
    if (!water2) {
      unsigned long waitTime = (MAX_WAIT_FOR_ELECTRODE2 - (millis() - pumpStartTime)) / 1000;
      Serial.println("Wait for E2: " + String(waitTime) + "s remaining");
    }
  }
  else if (systemState == STATE_FAILURE_COOLDOWN) {
    unsigned long remaining = (AUTO_RESET_DELAY - (millis() - failureStartTime)) / 1000;
    Serial.println("Auto-reset in: " + String(remaining) + "s");
  }
  Serial.println("---------------------");
}


String getStateName(SystemState state) {
  switch (state) {
    case STATE_WAITING_FOR_WATER: return "Waiting for Water";
    case STATE_STARTUP_STABILIZATION: return "Startup Stabilization";
    case STATE_PUMP_RUNNING: return "Pump Running";
    case STATE_FAILURE_COOLDOWN: return "Failure Cooldown";
    default: return "Unknown";
  }
}


String getResetReason(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:    return "Unknown";
    case ESP_RST_POWERON:    return "Power On";
    case ESP_RST_EXT:        return "External Reset";
    case ESP_RST_SW:         return "Software Reset";
    case ESP_RST_PANIC:      return "Exception/Panic";
    case ESP_RST_INT_WDT:    return "Interrupt Watchdog";
    case ESP_RST_TASK_WDT:   return "Task Watchdog";
    case ESP_RST_WDT:        return "Other Watchdog";
    case ESP_RST_DEEPSLEEP:  return "Deep Sleep";
    case ESP_RST_BROWNOUT:   return "Brownout";
    case ESP_RST_SDIO:       return "SDIO";
    default:                 return "Unknown";
  }
}

