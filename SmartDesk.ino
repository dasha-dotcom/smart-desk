#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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

unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 1000;


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
}


// -------------------- Main loop --------------------

void loop() {
  updateButton();
  updateState();
  updateTimer();
  updateLED();

  readEnvironment();
  updateDisplay();
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
      lastPrintTime = startTime;
      elapsedSessionSeconds = 0;

      Serial.println("State: RUNNING");
    } else {
      Serial.println("State: IDLE");
    }
  }

  previousButtonState = buttonState;
}


// -------------------- Timer --------------------

void updateTimer() {
  if (!isIdle) {
    elapsedSessionSeconds = (millis() - startTime) / 1000;

    if (millis() - lastPrintTime >= PRINT_INTERVAL) {

      unsigned long hours = elapsedSessionSeconds / 3600;
      unsigned long minutes = (elapsedSessionSeconds % 3600) / 60;
      unsigned long seconds = elapsedSessionSeconds % 60;

      Serial.print("Elapsed: ");

      if (hours > 0) {
        Serial.print(hours);
        Serial.print("h ");
      }

      if (minutes > 0 || hours > 0) {
        Serial.print(minutes);
        Serial.print("m ");
      }

      Serial.print(seconds);
      Serial.println("s");

      lastPrintTime = millis();
    }
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

      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println("%");

      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" C");
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