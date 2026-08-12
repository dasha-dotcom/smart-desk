# ESP32 Smart Desk System

A small desk device built with an ESP32 that tracks study sessions and displays basic environmental information.

The current version uses a physical button to start and stop a study session, an LED to show when a session is active, a DHT11 sensor to measure temperature and humidity, and an OLED screen to display all of the information together.

## Why I Built It

I wanted this project to be more than following a complete electronics tutorial from beginning to end. My goal was to learn how the individual parts of an embedded system work and then figure out how to combine them into one useful device.

I used tutorials and library examples when I needed to learn how a specific component worked, such as initializing an OLED or reading a DHT11. However, I built the overall program logic myself, including the button state handling, debounce logic, study timer, non-blocking sensor updates, display updates, and error handling.

I also built the project one subsystem at a time. Each component had to work on its own before I added it to the full system. This made it much easier to understand what was happening and to isolate problems when something stopped working.

## Features

The current V1 system can:

* Start and stop a study session using a physical pushbutton
* Debounce the button so one press produces one state change
* Turn an LED on while a study session is running
* Track study-session time in seconds, minutes, and hours
* Measure temperature and relative humidity with a DHT11
* Display the current state, session time, temperature, and humidity on a 128×64 OLED
* Update the button, sensor, timer, and display without relying on long `delay()` calls
* Detect failed DHT11 readings without overwriting the last valid measurement
* Detect whether the OLED is available during startup and continue running through Serial if it is missing

## Hardware

This project was built using parts from a LAFVIN ESP32 Basic Starter Kit.

| Component                   | Purpose                                                   |
| --------------------------- | --------------------------------------------------------- |
| ESP32 development board     | Main microcontroller                                      |
| 0.96" 128×64 SSD1306 OLED   | Main display                                              |
| DHT11 module                | Temperature and humidity                                  |
| Pushbutton                  | Starts/stops study sessions                               |
| LED                         | Indicates an active session                               |
| 220 Ω resistor              | Limits current through the LED                            |
| 10 kΩ resistor              | Pulls the button input LOW when the button is not pressed |
| Breadboard and jumper wires | Circuit construction                                      |

## Wiring

| ESP32 Pin | Connection                    |
| --------- | ----------------------------- |
| GPIO18    | LED through 220 Ω resistor    |
| GPIO19    | Pushbutton input              |
| GPIO4     | DHT11 signal                  |
| GPIO21    | OLED SDA                      |
| GPIO22    | OLED SCL                      |
| 3V3       | OLED, DHT11, and button power |
| GND       | Common ground                 |

### LED

```text
GPIO18 → 220 Ω resistor → LED → GND
```

The 220 Ω resistor limits the amount of current flowing through the LED. A plain LED should not be connected directly between an ESP32 GPIO and ground.

### Button

```text
3.3 V → Button → GPIO19
                    |
                  10 kΩ
                    |
                   GND
```

The 10 kΩ resistor acts as a pull-down resistor. When the button is not pressed, it keeps GPIO19 at a known LOW voltage instead of allowing the input to float randomly. Pressing the button connects the input to 3.3 V, producing a HIGH reading.

### DHT11

```text
VCC  → 3.3 V
DATA → GPIO4
GND  → GND
```

The labels on my particular DHT11 breakout were not very obvious. Only the `S` signal marking was clearly visible, so I used a multimeter and the module's circuit layout to verify the remaining connections instead of guessing.

### OLED

```text
VCC → 3.3 V
GND → GND
SDA → GPIO21
SCL → GPIO22
```

Before writing the OLED program, I ran an I²C scanner on the physical hardware.

The address detected on my display was:

```text
0x3C
```

I used the detected address rather than assuming the value from the kit tutorial.

## Software

The firmware is written in C/C++ using the Arduino framework for ESP32.

Libraries used:

* `Wire`
* `Adafruit GFX Library`
* `Adafruit SSD1306`
* `DHT sensor library by Adafruit`

I also used Wokwi during development to test parts of the circuit and firmware before transferring them to the physical ESP32.

One difference I had to account for was that Wokwi provides a DHT22 simulation, while the physical LAFVIN kit contains a DHT11. The same general code structure works for both, but the sensor type must be changed from:

```cpp
DHT22
```

to:

```cpp
DHT11
```

for the actual device.

## Firmware Design

Instead of putting all of the behavior directly inside `loop()`, I separated the program into functions with specific responsibilities.

The main loop is conceptually:

```cpp
void loop() {
    updateButton();
    updateState();
    updateTimer();
    updateLED();
    readEnvironment();
    updateDisplay();
}
```

Each function handles one part of the system.

### `updateButton()`

Reads GPIO19 and implements time-based button debounce.

A real mechanical button does not produce one perfectly clean electrical transition when pressed. Its contacts can rapidly switch between HIGH and LOW for a short period. Without debounce logic, one physical press can be interpreted as several presses.

### `updateState()`

Detects a valid button press and switches the system between:

```text
IDLE ↔ RUNNING
```

When a new session starts, it also records the session start time.

### `updateTimer()`

Uses `millis()` to calculate how much time has passed since the session began.

The timer internally keeps track of total elapsed seconds and converts that value into hours, minutes, and seconds when it is displayed.

For example:

```text
42s
3m 42s
1h 3m 42s
```

### `updateLED()`

Keeps the physical LED synchronized with the application state:

```text
IDLE     → LED OFF
RUNNING  → LED ON
```

### `readEnvironment()`

Reads temperature and humidity from the DHT11 approximately once every two seconds.

Instead of using:

```cpp
delay(2000);
```

the program compares timestamps from `millis()`.

This allows the ESP32 to continue checking the button and updating other parts of the system while it waits for the next DHT11 reading.

### `updateDisplay()`

Updates the OLED approximately every 250 ms rather than redrawing it on every pass through `loop()`.

The display shows information similar to:

```text
Smart Desk
State: RUNNING
Time: 12m 34s
Humidity: 45.00%
Temp: 23.00 C
```

## Non-Blocking Timing

One of the most important changes I made as the project grew was replacing long delays with non-blocking timing.

For example, instead of stopping the entire program for two seconds between sensor readings, the program does something conceptually like:

```cpp
if (millis() - lastDHTRead >= 2000) {
    // read sensor
}
```

The ESP32 can therefore continue running the rest of the loop while waiting.

This became important once the button, timer, DHT11, LED, and OLED were all running at the same time. A long delay used for one component would otherwise make the other components unresponsive.

## Error Handling

The current firmware includes several basic diagnostics.

### DHT11 failure

A failed DHT11 reading can return `NaN`.

Instead of immediately replacing the existing temperature and humidity variables with invalid values, the program first stores the new measurements temporarily.

Only successful measurements replace the previous values.

If a reading fails:

* An error is printed to Serial
* The OLED reports a DHT error
* The most recent valid temperature and humidity values are preserved

### OLED failure

During startup, the ESP32 checks whether a device responds at the OLED's I²C address.

If the OLED is unavailable, the system reports the error through Serial and continues running instead of depending on the display to function.

## Debugging and Lessons Learned

A large part of this project ended up being learning how to debug hardware rather than simply writing code.

### 1. Serial Monitor showed question marks

At one point, Serial Monitor was displaying unreadable question marks even though the ESP32 was running.

The problem was not the program itself. The firmware used:

```cpp
Serial.begin(115200);
```

so Serial Monitor also needed to be configured for **115200 baud**.

This was a good example of a problem that looked like broken code but was actually a communication-setting mismatch.

### 2. Understanding `millis()`

While building the first OLED uptime display, I originally saved:

```cpp
uptimeSeconds = millis();
```

inside `setup()`.

This gave me a value of roughly 600 and then never changed.

That helped me understand two separate ideas:

* `millis()` returns milliseconds since the ESP32 started
* Saving `millis()` once records a timestamp rather than creating a timer

For device uptime, I needed to continuously use the current `millis()` value. For the study timer, however, saving a timestamp when the button is pressed is exactly what I needed:

```cpp
startTime = millis();
```

and then:

```cpp
elapsed = millis() - startTime;
```

That distinction became the basis of the study timer.

### 3. Identifying the DHT11 pins

The markings on my physical DHT11 breakout were difficult to read. The signal pin was marked `S`, but the other two connections were not obvious.

Rather than connecting power based on an assumption, I used a multimeter to investigate the module.

There was even an extra debugging step here: at first the multimeter appeared to show an open circuit no matter what I measured. I eventually realized I was touching the protective plastic caps on the probes instead of the exposed metal tips.

It was a simple mistake, but it reinforced an important lesson from the rest of the project: before assuming something complicated is wrong, verify the basic physical setup.

### 4. Simulated hardware is not always identical to real hardware

Wokwi was useful for testing my code and circuit structure, but it did not perfectly match the LAFVIN kit.

For example, Wokwi uses a DHT22 while my actual device uses a DHT11.

That meant successful simulation was useful evidence, but it was never a replacement for testing the final program on the physical hardware.

### 5. Build one subsystem at a time

The most useful debugging strategy throughout the project was adding one component at a time.

The build progressed approximately like this:

```text
ESP32 / Serial
      ↓
LED
      ↓
Button
      ↓
Button debounce + state
      ↓
OLED
      ↓
DHT11
      ↓
Integrated system
```

When a component worked by itself before integration, I had a known-good reference point if something broke later.

## System Architecture

```text
                     ┌──────────────┐
Button ─────────────→│              │────→ Status LED
                     │    ESP32     │
DHT11 ──────────────→│              │────→ OLED
                     └──────────────┘
                            │
                            │
                       Study Timer
                       State Logic
```

The button and DHT11 provide inputs to the ESP32. The firmware maintains the current session state and timer, and the OLED and LED communicate that state back to the user.

## Current Status

The main V1 functionality is complete:

* Button input and debounce work
* IDLE/RUNNING state switching works
* LED state indication works
* Study-session timing works
* DHT11 readings work
* OLED output works
* All components operate together
* The main loop uses non-blocking timing
* Basic DHT11 and OLED error handling has been implemented

The remaining V1 work is final reliability testing and documentation.

Before marking V1.0 complete, I plan to:

* Power-cycle the finished device at least five times
* Test DHT11 failure handling
* Test startup without the OLED connected
* Run the complete device continuously for at least one hour
* Record any failures found during testing
* Clean up the final breadboard wiring
* Add final wiring and display photos
* Record a short demonstration video

## Limitations

V1 is intentionally small.

It currently:

* Does not connect to Wi-Fi
* Does not store session history after power loss
* Does not have a phone or web application
* Does not automatically determine whether someone is actually studying
* Uses the inexpensive DHT11, so temperature and humidity measurements are intended as basic environmental information rather than high-precision measurements
* Is still assembled on a breadboard rather than a permanent PCB or enclosure

Keeping those features out of V1 allowed me to focus on understanding the embedded system itself before adding networking or more complicated hardware.

## Possible V2

After V1 is fully tested, the next version could add Wi-Fi communication.

The current plan is for the ESP32 to send small telemetry updates to a local Python server containing information such as:

```text
temperature
humidity
session state
session elapsed time
timestamp
```

The server could then store the measurements and provide a simple browser dashboard showing:

* Temperature and humidity history
* Study-session history
* Daily focus-time totals

The important design requirement for V2 is that networking remains optional. If Wi-Fi or the server fails, the physical Smart Desk device should continue functioning normally.

## Possible Future Extensions

Other ideas I may explore later include:

* Ambient-light sensing
* Presence/motion sensing
* Session-completion sounds
* RGB status indication
* Saving small settings using ESP32 non-volatile storage
* A permanent enclosure or PCB
* A small TinyML experiment after enough useful sensor data has been collected

These are intentionally separate from V1 rather than requirements for finishing the project.

## What I Learned

This project gave me my first experience building several hardware and software components into one embedded system instead of testing each component independently.

Some of the most important things I learned were:

* How GPIO inputs and outputs work
* Why LEDs require current-limiting resistors
* Why digital inputs need a defined HIGH or LOW state
* How button bounce affects real hardware
* How I²C devices are addressed
* How to scan an I²C bus instead of assuming a device address
* How to read a digital environmental sensor
* How `millis()` can be used for timing without blocking the program
* How to divide firmware into functions with separate responsibilities
* How simulation differs from physical hardware
* How useful Serial Monitor and a multimeter are for debugging
* Why testing one subsystem at a time makes integration much easier

More importantly, I became much more comfortable treating hardware problems as things I could investigate systematically instead of randomly changing wires or code until something worked.

## Version

**V1 — ESP32 Smart Desk System**

Built with the LAFVIN Basic Starter Kit for ESP32.

V1.0 will be frozen after the final reliability tests are completed.
