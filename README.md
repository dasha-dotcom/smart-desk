# ESP32 Smart Desk System

A small desk device built with an ESP32 that tracks study sessions, measures basic environmental conditions, and sends the data to a local dashboard.

The physical device uses a pushbutton to start and stop study sessions, an LED to show when a session is active, a DHT11 sensor to measure temperature and humidity, and an OLED to display the information locally.

V2 adds Wi-Fi communication without making the physical device dependent on the network. The ESP32 sends telemetry to a local Flask server, which stores the data in SQLite and serves a browser dashboard with full-day temperature and humidity graphs and study-session statistics.

## Why I Built It

I wanted this project to be more than following a complete electronics tutorial from beginning to end. My goal was to learn how the individual parts of an embedded system work and then figure out how to combine them into one useful device.

I used tutorials and library examples when I needed to learn how a specific component worked, such as initializing an OLED, reading a DHT11, connecting an ESP32 to Wi-Fi, or making an HTTP request. However, I built the overall project logic myself, including the button state handling, debounce logic, study timer, non-blocking sensor and display updates, networking behavior, telemetry format, database structure, session tracking, dashboard logic, and error handling.

I also built the project one subsystem at a time. Each component or new layer had to work on its own before I added it to the complete system.

V1 focused entirely on the embedded device:

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
Integrated physical system
```

Only after V1 was stable did I begin V2:

```text
Wi-Fi connection
      ↓
Test HTTP request
      ↓
Flask endpoint
      ↓
Real JSON telemetry
      ↓
SQLite storage
      ↓
Browser dashboard
      ↓
Failure and recovery testing
```

Working this way made it much easier to understand what each layer was doing and to isolate problems when something stopped working.

## Features

### Physical Smart Desk

The ESP32 device can:

* Start and stop a study session using a physical pushbutton
* Debounce the button so one press produces one state change
* Turn an LED on while a study session is running
* Track study-session time in seconds, minutes, and hours
* Measure temperature and relative humidity with a DHT11
* Display the current state, session time, temperature, and humidity on a 128×64 OLED
* Update the button, sensor, timer, and display without relying on long `delay()` calls
* Detect failed DHT11 readings without overwriting the last valid measurement
* Detect whether the OLED is available during startup and continue running if it is missing

### V2 Networking and Dashboard

V2 adds:

* ESP32 Wi-Fi station mode
* Automatic Wi-Fi reconnection
* JSON telemetry sent over HTTP approximately every 30 seconds
* A local Python/Flask server
* SQLite storage with timestamps
* Separate storage for environmental telemetry and study-session events
* A browser dashboard showing the latest device state
* Full-day temperature history
* Full-day humidity history
* Five-minute averaging for readable day-long environmental graphs
* Daily focus-time totals
* Number of study sessions for the day
* Longest study session for the day
* Local-time day boundaries from midnight to midnight
* HTTP connection and response timeouts
* Retry handling for failed session-event requests
* Continued physical operation when Wi-Fi or the Flask server is unavailable

The most important requirement for V2 was that networking remain an **additional layer**, not a requirement for the physical Smart Desk to function.

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

The 10 kΩ resistor acts as a pull-down resistor. When the button is not pressed, it keeps GPIO19 at a known LOW voltage instead of allowing the input to float randomly.

Pressing the button connects the input to 3.3 V, producing a HIGH reading.

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

### ESP32 Firmware

The firmware is written in C/C++ using the Arduino framework for ESP32.

Libraries used:

* `Wire`
* `Adafruit GFX Library`
* `Adafruit SSD1306`
* `DHT sensor library by Adafruit`
* `WiFi`
* `HTTPClient`

I also used Wokwi during V1 development to test parts of the circuit and firmware before transferring them to the physical ESP32.

One difference I had to account for was that Wokwi provides a DHT22 simulation, while the physical LAFVIN kit contains a DHT11. The same general code structure works for both, but the sensor type must be changed to match the physical hardware.

### Local Server

The V2 backend is written in Python using:

* Flask
* Python's built-in `sqlite3`
* `datetime`
* `zoneinfo`

The dashboard uses HTML, Jinja templates, JavaScript, and Chart.js.

## Repository Structure

```text
.
├── SmartDesk.ino
├── secrets.example.h
├── .gitignore
├── docs/
│   ├── diagram.json
│   └── images/
│       ├── smart_desk1.pdf
│       ├── smart_desk2.pdf
│       ├── smart_desk_demo.MOV
│       └── smart_desk_diagram.png
└── server/
    ├── app.py
    └── templates/
        └── dashboard.html
```

`secrets.h` and the generated SQLite database are intentionally excluded from Git.

## Configuration

The firmware expects a private `secrets.h` file containing the Wi-Fi credentials and local Flask server address.

A safe template is included as:

```text
secrets.example.h
```

Create a copy named:

```text
secrets.h
```

and fill in the values:

```cpp
#pragma once

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL = "http://YOUR_COMPUTER_IP:8000";
```

For example, the server URL will usually use the local IP address of the computer running Flask.

The real `secrets.h` file is ignored by Git so Wi-Fi credentials are not committed to the repository.

## Running V2

### 1. Start the Flask server

From the `server` directory, install Flask if necessary:

```bash
python3 -m pip install flask
```

Then run:

```bash
python3 app.py
```

The Flask server listens on port `8000`.

### 2. Upload the ESP32 firmware

Configure `secrets.h`, compile `SmartDesk.ino`, and upload it to the ESP32.

The ESP32 will attempt to join the configured Wi-Fi network.

Networking does not need to succeed for the physical desk functions to operate.

### 3. Open the dashboard

On the computer running Flask, open:

```text
http://127.0.0.1:8000/
```

The dashboard reads the stored SQLite data and displays the current device information, today's environmental history, and study-session statistics.

## Firmware Design

Instead of putting all behavior directly inside `loop()`, the firmware is divided into functions with specific responsibilities.

Conceptually, the main loop is:

```cpp
void loop() {
    updateButton();
    updateState();
    updateTimer();
    updateLED();

    readEnvironment();
    updateDisplay();
    updateWiFi();

    // send periodic telemetry if it is time

    // send/retry a pending session event if necessary
}
```

Each part of the system is responsible for a different task.

### `updateButton()`

Reads GPIO19 and implements time-based button debounce.

A mechanical button does not produce one perfectly clean electrical transition when pressed. Its contacts can rapidly switch between HIGH and LOW for a short period.

Without debounce logic, one physical press can therefore be interpreted as several presses.

### `updateState()`

Detects a valid button press and switches the physical system between:

```text
IDLE ↔ RUNNING
```

When a session starts, it records the timer start point.

When the state changes, V2 also creates a pending session event for the server.

### `updateTimer()`

Uses `millis()` to calculate how much time has passed since the current session began.

The timer keeps track of elapsed seconds and converts that value into hours, minutes, and seconds for the OLED.

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

the firmware compares timestamps from `millis()`.

This lets the ESP32 continue checking the button and updating other parts of the system while waiting for the next sensor reading.

### `updateDisplay()`

Updates the OLED approximately every 250 ms instead of redrawing it on every pass through `loop()`.

The display shows information similar to:

```text
Smart Desk
State: RUNNING
Time: 12m 34s
Humidity: 45.00%
Temp: 23.00 C
```

### `updateWiFi()`

Checks the Wi-Fi connection state without using a blocking loop that waits indefinitely for a connection.

It detects transitions between connected and disconnected states and allows the rest of the physical Smart Desk to keep operating even when networking is unavailable.

### `sendTelemetry()`

Packages the current device state as JSON and sends it to the Flask server approximately every 30 seconds.

A telemetry payload looks similar to:

```json
{
  "temperature_c": 23.1,
  "humidity_pct": 45.0,
  "session_active": true,
  "session_elapsed_s": 754
}
```

The server records the timestamp when the measurement arrives.

### `sendSessionEvent()`

Study-session transitions are sent separately from the periodic environmental telemetry.

A start event looks like:

```json
{
  "event": "start"
}
```

A stop event includes the final session duration:

```json
{
  "event": "stop",
  "duration_s": 754
}
```

This separation became important because environmental measurements and study sessions represent two different kinds of data.

Telemetry is a **sampled time series**.

Session starts and stops are **events**.

## Non-Blocking Timing

One of the most important changes I made as the project grew was replacing long delays with timestamp-based timing.

For example, instead of stopping the entire program for two seconds between sensor readings, the firmware does something conceptually like:

```cpp
if (millis() - lastDHTRead >= DHT_INTERVAL) {
    // read sensor
}
```

The same pattern is used for:

* DHT11 readings
* OLED refreshes
* 30-second telemetry intervals
* Session-event retry intervals

This became increasingly important as more subsystems were combined.

A long delay used by one component would otherwise make the button, timer, display, and other components unresponsive.

HTTP requests themselves are synchronous, so V2 also uses connection and response timeouts to limit how long a failed request can hold the firmware.

## V2 Data Flow

The complete system now looks approximately like this:

```text
                          ┌───────────────┐
Button ─────────────────→│               │────→ Status LED
                         │     ESP32     │
DHT11 ──────────────────→│               │────→ OLED
                         └───────┬───────┘
                                 │
                            Wi-Fi / HTTP
                                 │
                                 ▼
                         ┌───────────────┐
                         │ Flask Server  │
                         └───────┬───────┘
                                 │
                              SQLite
                         ┌───────┴────────┐
                         │                │
                    telemetry         sessions
                         │                │
                         └───────┬────────┘
                                 │
                                 ▼
                         Browser Dashboard
```

The physical device remains useful even if everything below the Wi-Fi connection is unavailable.

## Telemetry Storage

The Flask server receives environmental telemetry at:

```text
POST /telemetry
```

Each measurement is stored in a SQLite `telemetry` table containing:

```text
id
timestamp
temperature_c
humidity_pct
session_active
session_elapsed_s
```

The database keeps the raw measurements approximately every 30 seconds.

That means a full day can contain thousands of individual readings.

I wanted to preserve that raw data without making the dashboard graph unnecessarily crowded.

## Full-Day Environmental Graphs

The dashboard displays temperature and humidity for the current calendar day.

Rather than drawing every 30-second reading directly, Flask groups the measurements into five-minute buckets and calculates an average for each bucket.

Conceptually:

```text
Raw data every 30 seconds
          ↓
SQLite keeps all measurements
          ↓
Flask groups today's readings
          ↓
5-minute averages
          ↓
Chart.js graphs
```

This separates **storage resolution** from **visualization resolution**.

The database keeps the detailed measurements while the dashboard shows a more readable representation of the day.

A complete day contains at most 288 five-minute graph points.

The dashboard defines a day from local midnight to local midnight using the `America/New_York` timezone while the SQLite timestamps are stored in UTC.

## Study-Session Tracking

Study sessions are stored separately from environmental telemetry.

The `sessions` table contains:

```text
id
started_at
ended_at
duration_s
```

When a session starts, Flask creates one open session.

When the session stops, Flask closes the open session and stores its duration.

The server also prevents duplicate start requests from creating several simultaneous open sessions.

This became important during retry testing because repeated HTTP requests should not create multiple copies of the same active session.

The dashboard uses this data to calculate:

* Focus time today
* Number of sessions today
* Longest session today

It also correctly handles a session that overlaps midnight by counting only the portion that belongs to the current day.

## Dashboard

The browser dashboard currently displays:

### Current Environment

* Temperature
* Humidity

### Current Session

* IDLE or RUNNING
* Most recently reported elapsed session time

### Today

* Total focus time
* Session count
* Longest session

### Environmental History

* Temperature throughout the current day
* Humidity throughout the current day

The dashboard queries SQLite whenever the page is refreshed.

## Error Handling and Reliability

### DHT11 Failure

A failed DHT11 reading can return `NaN`.

Instead of immediately replacing the existing temperature and humidity variables with invalid values, the firmware stores new measurements temporarily.

Only successful readings replace the previous values.

If a reading fails:

* An error is printed to Serial
* The OLED reports a DHT error
* The most recent valid temperature and humidity values are preserved

### OLED Failure

During startup, the ESP32 checks whether a device responds at the OLED's I²C address.

If the OLED is unavailable, the system reports the error through Serial and continues running instead of making the rest of the device depend on the display.

### Wi-Fi Failure

The ESP32 does not wait forever for Wi-Fi during startup.

If Wi-Fi is unavailable:

* The button still works
* The LED still works
* The study timer still works
* The OLED still updates
* The DHT11 continues to be read

The ESP32 is configured to reconnect automatically when Wi-Fi becomes available again.

### Flask Server Failure

If Wi-Fi is connected but the Flask server is unavailable, HTTP requests eventually time out rather than blocking forever.

Environmental telemetry simply tries again at the next normal telemetry interval.

Session events use a pending retry mechanism with a five-second retry interval.

### Recovery Testing

V2 was deliberately tested with:

* Flask stopped while the ESP32 remained running
* Flask restarted without resetting the ESP32
* Wi-Fi unavailable during startup
* Wi-Fi interrupted and restored
* Missing telemetry intervals
* HTTP requests failing while the physical device continued operating

The physical V1 behavior continued functioning through the network failures, and telemetry resumed after connectivity returned.

## Debugging and Lessons Learned

A large part of this project ended up being learning how to debug hardware and systems rather than simply writing code.

### 1. Serial Monitor showed question marks

At one point, Serial Monitor was displaying unreadable question marks even though the ESP32 was running.

The firmware used:

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

This gave me one value and then never changed.

That helped me understand two separate ideas:

* `millis()` returns milliseconds since the ESP32 started
* Saving `millis()` once records a timestamp rather than creating a timer

For the study timer, saving a timestamp when the button is pressed is exactly what I needed:

```cpp
startTime = millis();
```

and then:

```cpp
elapsed = millis() - startTime;
```

That distinction became the basis for much of the timing used throughout the project.

### 3. Identifying the DHT11 pins

The markings on my physical DHT11 breakout were difficult to read. The signal pin was marked `S`, but the other two connections were not obvious.

Rather than connecting power based on an assumption, I used a multimeter to investigate the module.

There was even an extra debugging step here: at first the multimeter appeared to show an open circuit no matter what I measured. I eventually realized I was touching the protective plastic caps on the probes instead of the exposed metal tips.

It was a simple mistake, but it reinforced an important lesson from the rest of the project: before assuming something complicated is wrong, verify the basic physical setup.

### 4. Simulated hardware is not always identical to real hardware

Wokwi was useful for testing code and circuit structure, but it did not perfectly match the physical LAFVIN kit.

For example, Wokwi uses a DHT22 while the actual device uses a DHT11.

Successful simulation was useful evidence, but it was never a replacement for testing on the real hardware.

### 5. Build one subsystem at a time

The most useful debugging strategy throughout the project was adding one component or layer at a time.

When a subsystem worked independently before integration, I had a known-good reference point if something broke later.

This approach continued into V2: I first connected the ESP32 to Wi-Fi, then sent one test HTTP request, then created the Flask endpoint, then sent real telemetry, then added SQLite, and only then built the dashboard.

### 6. An HTTP error was not necessarily an HTTP-data problem

During the first ESP32-to-Mac test, the ESP32 returned:

```text
HTTP response code: -1
```

The JSON was not the problem.

The ESP32 had joined Wi-Fi correctly, but it could not establish a connection to the server.

Testing the Flask/Python server separately and checking the Mac's local IP address helped isolate the problem before changing the payload.

### 7. Flask expects templates in a specific directory

When I first added the browser dashboard, telemetry continued reaching Flask correctly, but visiting the dashboard returned a server error.

The problem was simply that `dashboard.html` was not inside Flask's expected:

```text
templates/
```

directory.

It was another example of a small configuration problem appearing much more serious than it actually was.

### 8. Repeated retries need their own timing

During failure testing, a pending session event originally retried on every pass through `loop()`.

When Flask was unavailable, this caused the ESP32 to repeatedly print HTTP errors and immediately attempt the same request again.

I fixed this using another `millis()`-based interval:

```text
failed request
      ↓
wait 5 seconds
      ↓
retry
```

This applied the same timing idea from the sensors and display to network recovery.

### 9. Duplicate open sessions caused impossible daily totals

At one point, the dashboard reported more than 24 hours of focus time in a single day.

The arithmetic itself was not the original problem.

Several old test rows in the database had:

```text
ended_at = NULL
```

so the dashboard correctly interpreted them as sessions that were still running.

Because multiple open sessions existed at once, their durations were added together.

I cleaned the invalid test data and changed the server so a new `start` event does not create another row if an open session already exists.

This was one of the clearest examples in the project of how incorrect stored state can make correct calculations produce incorrect results.

### 10. Raw data and displayed data do not have to use the same resolution

The ESP32 sends telemetry approximately every 30 seconds.

Originally, the dashboard simply graphed the last 30 measurements. That worked for testing but did not give a useful picture of an entire day.

Instead of reducing how much data I stored, I kept every raw measurement in SQLite and changed only how it is displayed.

The dashboard now calculates five-minute averages for the current day.

That helped me understand the difference between collecting detailed data and presenting it at a useful resolution.

## Current Status

V1 and the planned V2 functionality are complete.

### V1

* Button input and debounce work
* IDLE/RUNNING state switching works
* LED state indication works
* Study-session timing works
* DHT11 readings work
* OLED output works
* All components operate together
* The main loop uses non-blocking timing
* DHT11 and OLED error handling work

### V2

* ESP32 connects to Wi-Fi
* V1 remains usable without Wi-Fi
* ESP32 sends structured JSON telemetry
* Flask receives the telemetry
* SQLite stores timestamped environmental data
* Study sessions are stored separately from periodic telemetry
* The dashboard displays current device status
* Full-day temperature and humidity graphs work
* Daily focus time is calculated
* Daily session count is calculated
* Longest session is calculated
* Wi-Fi reconnection works
* Telemetry resumes after server/network recovery
* Network failures do not prevent the physical Smart Desk from functioning

## Limitations

V2 is intentionally still a local project rather than a cloud product.

Current limitations include:

* The Flask server must be running on a computer on the local network for telemetry to be stored
* The server address is configured using the computer's local IP address
* The dashboard does not currently update automatically; it refreshes when the page is reloaded
* The latest dashboard device state can be up to roughly one telemetry interval old
* Chart.js is loaded from a CDN, so the graphs currently depend on internet access even though ESP32-to-Flask communication is local
* HTTP requests are synchronous, although two-second timeouts limit how long failures can block
* Session-event retry uses one pending event slot rather than a persistent offline queue
* Multiple session state changes during a prolonged server outage may therefore not all be preserved
* Environmental telemetry is not queued during an outage; the resulting graph simply contains a gap
* Study state and session history are not persisted on the ESP32 itself through a power cycle
* The DHT11 provides basic environmental information rather than high-precision measurements
* The physical system is still assembled on a breadboard instead of a permanent PCB or enclosure
* The device tracks deliberate button-controlled study sessions rather than trying to automatically decide whether someone is productive

These limitations are intentional boundaries for V2 rather than unfinished requirements.

## Possible Future Extensions

Possible future versions could explore:

* Persistent offline session-event queueing
* Automatic dashboard refresh
* Historical date selection for previous days
* Hosting Chart.js locally for a completely offline dashboard
* Ambient-light sensing
* Presence/motion sensing
* Session-completion sounds
* RGB status indication
* Saving small settings using ESP32 non-volatile storage
* A permanent enclosure
* A custom PCB
* A small TinyML experiment after enough useful sensor data has been collected

These are deliberately separate from V2 so the project has a clear stopping point.

## What I Learned

This project gave me my first experience building several hardware and software layers into one complete system instead of testing each component independently.

Some of the most important things I learned were:

* How GPIO inputs and outputs work
* Why LEDs require current-limiting resistors
* Why digital inputs need a defined HIGH or LOW state
* How button bounce affects real hardware
* How I²C devices are addressed
* How to scan an I²C bus instead of assuming a device address
* How to read a digital environmental sensor
* How `millis()` can be used for several independent timers without blocking the main loop
* How to divide firmware into functions with separate responsibilities
* How simulation differs from physical hardware
* How useful Serial Monitor and a multimeter are for debugging
* Why testing one subsystem at a time makes integration easier
* How an ESP32 joins an existing Wi-Fi network
* The difference between a network client and server
* What IP addresses, ports, HTTP methods, and routes represent
* How to send JSON from C++ to Python
* How Flask can receive data from a physical device
* How to store measurements using SQLite
* How parameterized SQL queries work
* The difference between sampled telemetry and discrete events
* How to aggregate time-series data for visualization
* How to handle UTC database timestamps and local calendar days
* How to design retries without creating a tight retry loop
* Why network behavior should fail independently from core embedded behavior
* Why credentials and generated databases should not be committed to Git

More importantly, I became much more comfortable treating both hardware and software problems as things I could investigate systematically instead of randomly changing wires or code until something worked.

V2 also changed the way I thought about the ESP32 itself. In V1, it was the entire system. In V2, it became one part of a larger system that crossed from physical hardware into networking, a Python backend, a database, and a browser interface.

## Version

**V2.0 — ESP32 Smart Desk System**

Built with the LAFVIN Basic Starter Kit for ESP32.

### V1

Physical embedded Smart Desk:

```text
button + DHT11
       ↓
     ESP32
       ↓
OLED + LED + study timer
```

### V2

Networked Smart Desk:

```text
physical V1 device
       ↓
      Wi-Fi
       ↓
HTTP / JSON
       ↓
Flask
       ↓
SQLite
       ↓
daily browser dashboard
```

The central design rule remains the same: the networked features extend the Smart Desk, but the physical device remains functional on its own.
