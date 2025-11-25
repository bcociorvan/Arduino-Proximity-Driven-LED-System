# Proximity-Driven LED System

An Arduino Mega project that controls **17 LEDs** using **3 proximity sensors**, featuring non-blocking timing (no `delay()`), full software debouncing, and a robust state-machine architecture.

- **Board:** Arduino Mega 2560  
- **Outputs:** 17 LEDs  
- **Inputs:** 3 proximity sensors (digital)  
- **Logic:** Finite State Machine (FSM), debounced inputs, `millis()` timing  

---

## Features

### Sensor 1 (MASTER) – Pin 9
The master sensor has **highest priority** and overrides all other sequences.

**While `sensor1Pin` is HIGH:**
- LEDs turn ON sequentially **1 → 17**
- LED 17 stays ON briefly (peak dwell)
- All LEDs turn OFF instantly
- Sequence repeats (1→17 → dwell → OFF → repeat)

**If `sensor1Pin` becomes LOW during a sequence:**
- The current sequence **finishes normally**  
  (never interrupted halfway)
- System ensures **all LEDs end up ON**
- LEDs stay ON for **2 minutes**
- After 2 minutes:
  - LEDs turn OFF in reverse order **17 → 1**

---

### Sensor 2 – Pin 11
Active only when the system is in **IDLE** (no master activity).

When triggered:
1. LEDs turn ON **1 → 17**
2. Remain ON for **2 minutes**
3. Turn OFF **17 → 1**

**If triggered again during the 2-minute hold → timer resets.**

---

### Sensor 3 – Pin 7
Similar to Sensor 2, but reversed:

1. LEDs turn ON **17 → 1**
2. Remain ON for **2 minutes**
3. Turn OFF **1 → 17**

**Retrigger during ON time resets the timer.**

---

### Priority Handling
- **Sensor 1 overrides everything**
- Whenever `sensor1Pin` becomes HIGH:
  - Any Sensor 2 or 3 sequence is immediately terminated
  - Master sequence begins
- All sensors are **debounced (50 ms)** so noise cannot trigger false events

---

## Hardware Connections

### LED Outputs (17 pins)

LED 1  -> Pin 31  
LED 2  -> Pin 32  
LED 3  -> Pin 33  
LED 4  -> Pin 34  
LED 5  -> Pin 35  
LED 6  -> Pin 36  
LED 7  -> Pin 37  
LED 8  -> Pin 38  
LED 9  -> Pin 39  
LED 10 -> Pin 40  
LED 11 -> Pin 41  
LED 12 -> Pin 42  
LED 13 -> Pin 43  
LED 14 -> Pin 44  
LED 15 -> Pin 45  
LED 16 -> Pin 46  
LED 17 -> Pin 47  

### Sensor Inputs

Sensor1 (MASTER) -> Pin 9  
Sensor2          -> Pin 11  
Sensor3          -> Pin 7  


> Ensure your sensors output clean HIGH/LOW levels.  
> Hardware must include proper pull-up or pull-down resistors (module-built or external).

---

## Software Architecture

This project uses:

- **Finite State Machine (FSM)** controlling all LED modes
- **Non-blocking timing** using `millis()`  
  (system stays responsive to sensor inputs)
- **Software debouncing** for all three sensors
- **Separate sequences**:
  - Master: repeated sweep + OFF + repeat
  - S2/S3: one-time sweep → hold → reverse-off

### Main Timing Constants

```cpp
STEP_MS_MASTER    = 200 ms   // LED step speed for Sensor 1
STEP_MS_S2_S3     = 25 ms    // LED step speed for Sensor 2/3 (set 300 ms if preferred)
HOLD_DURATION_MS  = 2 minutes // Sensor 2/3 ON duration
HOLD_S1_MS        = 2 minutes // Sensor 1 post-release ON duration
S1_TOP_DWELL_MS   = 200 ms   // dwell time on LED 17

LED 15 -> Pin 45
LED 16 -> Pin 46
LED 17 -> Pin 47

## Software Architecture

This project uses:

- **Finite State Machine (FSM)** controlling all LED modes
- **Non-blocking timing** using `millis()`  
  (system stays responsive to sensor inputs)
- **Software debouncing** for all three sensors
- **Separate sequences**:
  - Master: repeated sweep + OFF + repeat
  - S2/S3: one-time sweep → hold → reverse-off

### Main Timing Constants

```cpp
STEP_MS_MASTER    = 200 ms   // LED step speed for Sensor 1
STEP_MS_S2_S3     = 25 ms    // LED step speed for Sensor 2/3 (set 300 ms if preferred)
HOLD_DURATION_MS  = 2 minutes // Sensor 2/3 ON duration
HOLD_S1_MS        = 2 minutes // Sensor 1 post-release ON duration
S1_TOP_DWELL_MS   = 200 ms   // dwell time on LED 17
