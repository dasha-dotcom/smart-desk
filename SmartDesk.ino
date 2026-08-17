#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "secrets.h"
#include <HTTPClient.h>

// -------------------- Wi-Fi --------------------

bool wasWiFiConnected = false;
unsigned long lastTelemetrySend = 0;
const unsigned long TELEMETRY_INTERVAL = 30000;
bool hasSentTelemetry = false;
bool sessionEventPending = false;
bool pendingSessionIsStart = false;
unsigned long pendingSessionDuration = 0;
unsigned long lastSessionEventAttempt = 0;
const unsigned long SESSION_RETRY_INTERVAL = 5000;

// -------------------- Pins --------------------

const int LED_PIN = 18;
const int BUTTON_PIN = 19;

const int DHT_PIN = 4;
const int DHT_TYPE = DHT11;


// -------------------- Button / state --------------------

bool isIdle = true;

int buttonState = LOW;
int previousButtonState = LOW;
int lastReading = LOW;

unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;


// -------------------- Session timer --------------------

unsigned long startTime = 0;
unsigned long elapsedSessionSeconds = 0;


// -------------------- DHT11 --------------------

unsigned long lastDHTRead = 0;
const unsigned long DHT_INTERVAL = 2000;

bool dhtError = false;
bool hasDHTReading = false;

float humidity = 0;
float temperature = 0;

DHT dht(DHT_PIN, DHT_TYPE);


// -------------------- OLED --------------------

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int SCREEN_ADDRESS = 0x3C;

bool oledAvailable = false;

unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 250;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


// -------------------- OLED Check --------------------

bool checkOLED() {
  Wire.beginTransmission(SCREEN_ADDRESS);
  return Wire.endTransmission() == 0;
}


// -------------------- Setup --------------------

void setup() {
  Serial.begin(115200);

  Serial.println("Smart Desk starting...");

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  digitalWrite(LED_PIN, LOW);

  dht.begin();
  Serial.println("DHT11 initialized");

  Wire.begin(21, 22);

  oledAvailable = checkOLED();

  if (oledAvailable) {
    Serial.println("OLED found at 0x3C");

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      Serial.println("ERROR: OLED initialization failed");
      oledAvailable = false;
    }
  } else {
    Serial.println("ERROR: OLED not found");
  }

  if (oledAvailable) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Smart Desk");
    display.display();
  } else {
    Serial.println("Continuing without display");
  }

  Serial.println("System ready");
  
  startWiFi();
}


// -------------------- Main loop --------------------

void loop() {
  updateButton();
  updateState();
  updateTimer();
  updateLED();

  readEnvironment();
  updateDisplay();
  updateWiFi();

  if (WiFi.status() == WL_CONNECTED &&
      hasDHTReading &&
      (!hasSentTelemetry ||
      millis() - lastTelemetrySend >= TELEMETRY_INTERVAL)) {

    sendTelemetry();
    lastTelemetrySend = millis();
    hasSentTelemetry = true;
    }

    if (sessionEventPending &&
    WiFi.status() == WL_CONNECTED &&
    millis() - lastSessionEventAttempt >= SESSION_RETRY_INTERVAL) {

      lastSessionEventAttempt = millis();
      
      if (sendSessionEvent()) {
        sessionEventPending = false;
      }
    }
}


// -------------------- Button --------------------

void updateButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime >= DEBOUNCE_DELAY) {
    buttonState = reading;
  }

  lastReading = reading;
}


// -------------------- State --------------------

void updateState() {
  if (buttonState == HIGH && previousButtonState == LOW) {
    isIdle = !isIdle;

    if (!isIdle) {
      startTime = millis();
      elapsedSessionSeconds = 0;

      sessionEventPending = true;
      pendingSessionIsStart = true;
      pendingSessionDuration = 0;

      lastSessionEventAttempt =
          millis() - SESSION_RETRY_INTERVAL;

      Serial.println("State: RUNNING");
    } else {
      elapsedSessionSeconds = (millis() - startTime) / 1000;

      sessionEventPending = true;
      pendingSessionIsStart = false;
      pendingSessionDuration = elapsedSessionSeconds;

      lastSessionEventAttempt =
          millis() - SESSION_RETRY_INTERVAL;

      Serial.println("State: IDLE");
    }
  }

  previousButtonState = buttonState;
}


// -------------------- Timer --------------------

void updateTimer() {
  if (!isIdle) {
    elapsedSessionSeconds = (millis() - startTime) / 1000;
  }
}


// -------------------- LED --------------------

void updateLED() {
  if (isIdle) {
    digitalWrite(LED_PIN, LOW);
  } else {
    digitalWrite(LED_PIN, HIGH);
  }
}


// -------------------- DHT11 --------------------

void readEnvironment() {
  unsigned long currentTime = millis();

  if (currentTime - lastDHTRead >= DHT_INTERVAL) {

    float newHumidity = dht.readHumidity();
    float newTemperature = dht.readTemperature();

    if (isnan(newHumidity) || isnan(newTemperature)) {
      dhtError = true;
      Serial.println("ERROR: Failed to read from DHT11");
    } else {
      dhtError = false;
      hasDHTReading = true;

      humidity = newHumidity;
      temperature = newTemperature;
    }

    lastDHTRead = currentTime;
  }
}


// -------------------- OLED --------------------

void updateDisplay() {
  if (!oledAvailable) {
    return;
  }

  unsigned long currentTime = millis();

  if (currentTime - lastDisplayUpdate >= DISPLAY_INTERVAL) {

    display.clearDisplay();
    display.setCursor(0, 0);

    display.println("Smart Desk");

    display.print("State: ");
    if (isIdle) {
      display.println("IDLE");
    } else {
      display.println("RUNNING");
    }

    unsigned long hours = elapsedSessionSeconds / 3600;
    unsigned long minutes = (elapsedSessionSeconds % 3600) / 60;
    unsigned long seconds = elapsedSessionSeconds % 60;

    display.print("Time: ");

    if (hours > 0) {
      display.print(hours);
      display.print("h ");
    }

    if (minutes > 0 || hours > 0) {
      display.print(minutes);
      display.print("m ");
    }

    display.print(seconds);
    display.println("s");

    if (hasDHTReading) {
      display.print("Humidity: ");
      display.print(humidity);
      display.println("%");

      display.print("Temp: ");
      display.print(temperature);
      display.println(" C");
    } else {
      display.println("Humidity: --");
      display.println("Temp: --");
    }

    if (dhtError) {
      display.println("DHT: ERROR");
    }

    display.display();

    lastDisplayUpdate = currentTime;
  }
}

// -------------------- Wi-Fi --------------------

void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void updateWiFi() {
  bool isWiFiConnected = (WiFi.status() == WL_CONNECTED);

  if (isWiFiConnected && !wasWiFiConnected) {
    Serial.println("Wi-Fi connected!");

    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    }

  if (!isWiFiConnected && wasWiFiConnected) {
    Serial.println("Wi-Fi connection lost.");
  }

  wasWiFiConnected = isWiFiConnected;
}

void sendTelemetry() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot send telemetry: Wi-Fi not connected");
    return;
  }

  HTTPClient http;
  http.setConnectTimeout(2000);
  http.setTimeout(2000);

  String telemetryUrl =
    String(SERVER_URL) + "/telemetry";

  http.begin(telemetryUrl);

  http.addHeader("Content-Type", "application/json");

  String payload = "{";

  payload += "\"temperature_c\":";
  payload += String(temperature, 1);

  payload += ",\"humidity_pct\":";
  payload += String(humidity, 1);

  payload += ",\"session_active\":";
  payload += isIdle ? "false" : "true";

  payload += ",\"session_elapsed_s\":";
  payload += String(elapsedSessionSeconds);

  payload += "}";

  int responseCode = http.POST(payload);

  if (responseCode < 0) {
  Serial.print("Telemetry HTTP error: ");
  Serial.println(http.errorToString(responseCode));
} else if (responseCode < 200 || responseCode >= 300) {
  Serial.print("Telemetry rejected. HTTP status: ");
  Serial.println(responseCode);
}

  http.end();
}

bool sendSessionEvent() {
  HTTPClient http;
  http.setConnectTimeout(2000);
  http.setTimeout(2000);

  String sessionUrl =
    String(SERVER_URL) + "/session";

  http.begin(sessionUrl);
  http.addHeader("Content-Type", "application/json");

  String payload;

  if (pendingSessionIsStart) {
    payload = "{\"event\":\"start\"}";
  } else {
    payload = "{\"event\":\"stop\",\"duration_s\":";
    payload += String(pendingSessionDuration);
    payload += "}";
  }

  int responseCode = http.POST(payload);

    if (responseCode < 0) {
      Serial.print("Session event HTTP error: ");
      Serial.println(http.errorToString(responseCode));
    } else if (responseCode >= 400) {
      Serial.print("Session event rejected. HTTP status: ");
      Serial.println(responseCode);
  }

  bool success = responseCode >= 200 && responseCode < 300;
  bool clientError = responseCode >= 400 && responseCode < 500;

  http.end();

  return success || clientError;
}