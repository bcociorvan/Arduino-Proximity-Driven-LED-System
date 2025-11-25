/* --------------------------
   17 LEDs + 3 Proximity Sensors (debounced)
   - sensor1Pin (MASTER): While HIGH -> ON 1->17, brief dwell at top, then ALL OFF, repeat.
     If LOW mid-run: finish current sequence, ensure ALL ON, hold 30s, then OFF 17->1.
   - sensor2Pin: ON 1->17, hold 30s, OFF 17->1 (STEP_MS_S2_S3). Retrigger resets hold.
   - sensor3Pin: ON 17->1, hold 30s, OFF 1->17 (STEP_MS_S2_S3). Retrigger resets hold.
   Non-blocking (millis), safe indices, clean state machine.
--------------------------- */

const int ledPins[] = {31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
const int numLeds = 17;

const int sensor1Pin = 9;   // MASTER
const int sensor2Pin = 11;
const int sensor3Pin = 7;

// Timing
const unsigned long DEBOUNCE_MS      = 50;
const unsigned long STEP_MS_MASTER   = 200;               // S1 step time
const unsigned long STEP_MS_S2_S3    = 25;                // S2/S3 step time (set 300 if desired)
const unsigned long HOLD_DURATION_MS = 30UL * 1000UL;     // S2/S3 hold (30s)
const unsigned long HOLD_S1_MS       = 30UL * 1000UL;     // S1 hold (30s)
const unsigned long S1_TOP_DWELL_MS  = STEP_MS_MASTER;    // brief dwell at "all ON" so LED 17 is visibly on

// State machine
enum State {
  IDLE,

  // Sensor1 (master) states
  S1_SWEEP_ON,           // turning on 1->17
  S1_PEAK_DWELL,         // brief dwell with ALL ON before OFF
  S1_OFF_INSTANT,        // turn all off instantly (single-shot)
  S1_FINISH_ON_TO_HOLD,  // after release if we were OFF, do one ON sweep to reach ALL ON
  S1_HOLD_ON,            // hold all ON for 30s after release
  S1_TURNING_OFF_REV,    // reverse off 17->1 after hold

  // Sensor2 states
  S2_TURNING_ON,
  S2_HOLD_ON,
  S2_TURNING_OFF,

  // Sensor3 states
  S3_TURNING_ON,
  S3_HOLD_ON,
  S3_TURNING_OFF
};

State state = IDLE;

// Sequencing
int currentLed = 0;                // index into ledPins (0..16)
unsigned long lastStepTime = 0;    // last time we stepped an LED
unsigned long holdStartTime = 0;   // when we started a hold

// Master release handling
bool s1Released = false;           // sensor1 went LOW while S1 was running

// Debounce storage
int  s1Stable = LOW, s2Stable = LOW, s3Stable = LOW;
int  s1LastRead = LOW, s2LastRead = LOW, s3LastRead = LOW;
unsigned long s1LastChange = 0, s2LastChange = 0, s3LastChange = 0;

// ------------- Helpers -------------
void allLedsOff() {
  for (int i = 0; i < numLeds; i++) digitalWrite(ledPins[i], LOW);
}

void allLedsOn() {
  for (int i = 0; i < numLeds; i++) digitalWrite(ledPins[i], HIGH);
}

void resetToIdle() {
  allLedsOff();
  state = IDLE;
  currentLed = 0;
  lastStepTime = 0;
  holdStartTime = 0;
  s1Released = false;
}

// Debounce one sensor; returns its stable state (LOW/HIGH)
int debounceRead(int pin, int &lastRead, unsigned long &lastChange, int &stable) {
  unsigned long now = millis();
  int reading = digitalRead(pin);
  if (reading != lastRead) {
    lastChange = now;
    lastRead = reading;
  }
  if (now - lastChange > DEBOUNCE_MS) {
    stable = reading; // stable long enough
  }
  return stable;
}

// Safe digitalWrite for an LED index
inline void setLed(int idx, bool on) {
  if (idx >= 0 && idx < numLeds) digitalWrite(ledPins[idx], on ? HIGH : LOW);
}

// ------------- Arduino setup/loop -------------
void setup() {
  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // Using INPUT based on your wiring (you said hardware provides proper levels)
  pinMode(sensor1Pin, INPUT);
  pinMode(sensor2Pin, INPUT);
  pinMode(sensor3Pin, INPUT);
}

void loop() {
  unsigned long now = millis();

  // Debounce all sensors
  debounceRead(sensor1Pin, s1LastRead, s1LastChange, s1Stable);
  debounceRead(sensor2Pin, s2LastRead, s2LastChange, s2Stable);
  debounceRead(sensor3Pin, s3LastRead, s3LastChange, s3Stable);

  // ========================= SENSOR 1 (MASTER) =========================
  // Start or maintain S1 while pin is HIGH
  if (s1Stable == HIGH) {
    if (!(state == S1_SWEEP_ON || state == S1_PEAK_DWELL || state == S1_OFF_INSTANT)) {
      // Take control if not already in the S1 running states or post-release phases
      s1Released = false;
      allLedsOff();
      state = S1_SWEEP_ON;
      currentLed = 0;
      lastStepTime = now;
    }
  } else {
    // s1 is LOW: if we are in running S1 states, mark release so we finish sequence
    if (state == S1_SWEEP_ON || state == S1_PEAK_DWELL || state == S1_OFF_INSTANT) {
      s1Released = true;
    }
  }

  // --- S1: ON sweep 1->17 ---
  if (state == S1_SWEEP_ON) {
    if (now - lastStepTime >= STEP_MS_MASTER) {
      lastStepTime = now;
      setLed(currentLed, true);
      currentLed++;
      if (currentLed >= numLeds) {
        // Finished ON sweep
        if (!s1Released && s1Stable == HIGH) {
          // Dwell briefly with all ON so LED 17 is visibly on
          state = S1_PEAK_DWELL;
          lastStepTime = now; // reset for dwell
        } else {
          // Released during ON sweep: we are now ALL ON -> hold for 30s
          holdStartTime = now;
          state = S1_HOLD_ON;
        }
      }
    }
    return; // while S1 running, ignore S2/S3
  }

  // --- S1: peak dwell at "all ON" before OFF ---
  if (state == S1_PEAK_DWELL) {
    if (now - lastStepTime >= S1_TOP_DWELL_MS) {
      if (!s1Released && s1Stable == HIGH) {
        // After brief dwell, do the OFF phase and repeat
        state = S1_OFF_INSTANT;
      } else {
        // If released by now, go to the 30s hold with all LEDs ON
        holdStartTime = now;
        state = S1_HOLD_ON;
      }
    }
    return;
  }

  // --- S1: turn all OFF instantly (single shot) ---
  if (state == S1_OFF_INSTANT) {
    allLedsOff();
    if (!s1Released && s1Stable == HIGH) {
      // Continue the run: start another ON sweep
      currentLed = 0;
      lastStepTime = now;
      state = S1_SWEEP_ON;
    } else {
      // Released during/at this OFF phase:
      // finish sequence -> ensure ALL ON, then hold 30s, then reverse OFF
      currentLed = 0;
      lastStepTime = now;
      state = S1_FINISH_ON_TO_HOLD; // do one more ON sweep to get to ALL ON
    }
    return;
  }

  // --- S1: finish to ALL ON (after release) ---
  if (state == S1_FINISH_ON_TO_HOLD) {
    if (now - lastStepTime >= STEP_MS_MASTER) {
      lastStepTime = now;
      setLed(currentLed, true);
      currentLed++;
      if (currentLed >= numLeds) {
        holdStartTime = now;
        state = S1_HOLD_ON;
      }
    }
    return;
  }

  // --- S1: hold ALL ON for 30 seconds after release ---
  if (state == S1_HOLD_ON) {
    // If sensor1 goes HIGH again during hold, immediately resume S1 run
    if (s1Stable == HIGH) {
      s1Released = false;
      allLedsOff();
      currentLed = 0;
      lastStepTime = now;
      state = S1_SWEEP_ON;
      return;
    }
    // Hold for 30 seconds
    if (now - holdStartTime >= HOLD_S1_MS) {
      // Then reverse off 17->1
      currentLed = numLeds - 1;
      lastStepTime = now;
      state = S1_TURNING_OFF_REV;
    }
    return;
  }

  // --- S1: reverse OFF 17->1 after hold ---
  if (state == S1_TURNING_OFF_REV) {
    // If sensor1 goes HIGH during reverse-off, immediately resume the S1 run
    if (s1Stable == HIGH) {
      s1Released = false;
      allLedsOff();
      currentLed = 0;
      lastStepTime = now;
      state = S1_SWEEP_ON;
      return;
    }
    if (now - lastStepTime >= STEP_MS_MASTER) {
      lastStepTime = now;
      setLed(currentLed, false);
      currentLed--;
      if (currentLed < 0) {
        resetToIdle();
      }
    }
    return;
  }
  // ====================== END SENSOR 1 (MASTER) ======================

  // ----- IDLE: accept S2/S3 triggers -----
  if (state == IDLE) {
    if (s2Stable == HIGH) {
      // Start Sensor2 sequence: ON 1->17
      state = S2_TURNING_ON;
      currentLed = 0;           // first LED index
      lastStepTime = now;
      allLedsOff();
    } else if (s3Stable == HIGH) {
      // Start Sensor3 sequence: ON 17->1
      state = S3_TURNING_ON;
      currentLed = numLeds - 1; // start from last LED
      lastStepTime = now;
      allLedsOff();
    } else {
      // remain idle
    }
  }

  // ----- Sensor 2 sequence -----
  if (state == S2_TURNING_ON) {
    if (now - lastStepTime >= STEP_MS_S2_S3) {
      lastStepTime = now;
      setLed(currentLed, true);
      currentLed++;
      if (currentLed >= numLeds) {
        // fully ON
        holdStartTime = now;
        state = S2_HOLD_ON;
      }
    }
  } else if (state == S2_HOLD_ON) {
    // Retrigger resets hold timer
    if (s2Stable == HIGH) holdStartTime = now;
    if (now - holdStartTime >= HOLD_DURATION_MS) {
      // start turning off reverse: 17->1
      state = S2_TURNING_OFF;
      currentLed = numLeds - 1;
      lastStepTime = now;
    }
  } else if (state == S2_TURNING_OFF) {
    if (now - lastStepTime >= STEP_MS_S2_S3) {
      lastStepTime = now;
      setLed(currentLed, false);
      currentLed--;
      if (currentLed < 0) {
        resetToIdle();
      }
    }
  }

  // ----- Sensor 3 sequence -----
  if (state == S3_TURNING_ON) {
    if (now - lastStepTime >= STEP_MS_S2_S3) {
      lastStepTime = now;
      setLed(currentLed, true);
      currentLed--;
      if (currentLed < 0) {
        // fully ON
        holdStartTime = now;
        state = S3_HOLD_ON;
      }
    }
  } else if (state == S3_HOLD_ON) {
    // Retrigger resets hold timer
    if (s3Stable == HIGH) holdStartTime = now;
    if (now - holdStartTime >= HOLD_DURATION_MS) {
      // start turning off normal: 1->17
      state = S3_TURNING_OFF;
      currentLed = 0;
      lastStepTime = now;
    }
  } else if (state == S3_TURNING_OFF) {
    if (now - lastStepTime >= STEP_MS_S2_S3) {
      lastStepTime = now;
      setLed(currentLed, false);
      currentLed++;
      if (currentLed >= numLeds) {
        resetToIdle();
      }
    }
  }
}