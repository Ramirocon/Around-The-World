#include <Arduino.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <FastLED.h>

// =========================
// BUTTON PINS - LOLIN32 LITE
// =========================
const int BUTTON1_PIN = 27;
const int BUTTON2_PIN = 33;
const int BUTTON3_PIN = 15;

const int M1_IN1 = 25;   // A1
const int M1_IN2 = 26;   // A0

const int M2_IN1 = 14;
const int M2_IN2 = 13;

const int MATRIX_PIN = 32;

const int DF_RX = 7;    // Feather RX <- DFPlayer TX
const int DF_TX = 8;    // Feather TX -> DFPlayer RX    // ESP32 TX -> DFPlayer RX

// =========================
// DFPLAYER PINS
// =========================
  // ESP32 TX -> DFPlayer RX

// =========================
// MATRIX SETTINGS
// =========================
 // change if needed
const int NUM_LEDS = 64;

CRGB leds[NUM_LEDS];
bool matrixEnabled = false;
unsigned long lastMatrixFlash = 0;
const unsigned long MATRIX_FLASH_INTERVAL = 100; // ms

// =========================
// DFPLAYER OBJECT
// =========================
HardwareSerial dfSerial(2);
DFRobotDFPlayerMini dfPlayer;

// =========================
// PWM SETTINGS
// =========================
const int PWM_FREQ = 500;
const int PWM_RESOLUTION = 8;   // 0-255
const int MOTOR_SPEED = 220;     // slow speed

// =========================
// COMBO MODE SETTINGS
// =========================
const unsigned long COMBO_DURATION = 45000UL; // 45 seconds
bool comboActive = false;
unsigned long comboStartTime = 0;

// =========================
// DEBOUNCE
// =========================
const unsigned long DEBOUNCE_MS = 30;
unsigned long lastReadTime = 0;

// =========================
// BUTTON HELPERS
// =========================
bool button1Pressed() {
  return digitalRead(BUTTON1_PIN) == LOW;
}

bool button2Pressed() {
  return digitalRead(BUTTON2_PIN) == LOW;
}

bool button3Pressed() {
  return digitalRead(BUTTON3_PIN) == LOW;
}

// =========================
// MOTOR CONTROL
// Newer ESP32 LEDC API style
// =========================
void stopMotor1() {
  ledcWrite(M1_IN1, 0);
  ledcWrite(M1_IN2, 0);
}

void stopMotor2() {
  ledcWrite(M2_IN1, 0);
  ledcWrite(M2_IN2, 0);
}

// Motor 1 direction
void runMotor1Slow() {
  ledcWrite(M1_IN1, MOTOR_SPEED);
  ledcWrite(M1_IN2, 0);
}

// Motor 2 opposite direction
void runMotor2Slow() {
  ledcWrite(M2_IN1, 0);
  ledcWrite(M2_IN2, MOTOR_SPEED);
}

void setupPWM() {
  ledcAttach(M1_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(M1_IN2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(M2_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(M2_IN2, PWM_FREQ, PWM_RESOLUTION);
}

// =========================
// MATRIX CONTROL
// =========================
void matrixOn() {
  matrixEnabled = true;
}

void matrixOff() {
  matrixEnabled = false;
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void updateMatrix() {
  if (!matrixEnabled) return;

  unsigned long now = millis();
  if (now - lastMatrixFlash >= MATRIX_FLASH_INTERVAL) {
    lastMatrixFlash = now;

    // one random full-brightness color across whole matrix
    CRGB c = CHSV(random8(), 255, 255);
    fill_solid(leds, NUM_LEDS, c);
    FastLED.show();
  }
}

// =========================
// AUDIO CONTROL
// =========================
void startSong() {
  dfPlayer.play(1);   // plays 0001.mp3
  Serial.println("Playing song");
}

void stopSong() {
  dfPlayer.stop();
  Serial.println("Stopping song");
}

// =========================
// COMBO MODE CONTROL
// =========================
void startComboMode() {
  comboActive = true;
  comboStartTime = millis();

  runMotor1Slow();
  runMotor2Slow();
  matrixOn();
  startSong();

  Serial.println("COMBO MODE STARTED");
}

void stopComboMode() {
  comboActive = false;

  stopMotor1();
  stopMotor2();
  matrixOff();
  stopSong();

  Serial.println("COMBO MODE ENDED");
}

// =========================
// NORMAL BUTTON CONTROL
// =========================
void applyNormalButtonControl() {
  if (button1Pressed()) {
    runMotor1Slow();
  } else {
    stopMotor1();
  }

  if (button2Pressed()) {
    runMotor2Slow();
  } else {
    stopMotor2();
  }

  if (button3Pressed()) {
    matrixOn();
  } else {
    matrixOff();
  }
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);

  pinMode(M1_IN1, OUTPUT);
  pinMode(M1_IN2, OUTPUT);
  pinMode(M2_IN1, OUTPUT);
  pinMode(M2_IN2, OUTPUT);

  setupPWM();

  stopMotor1();
  stopMotor2();

  // If your matrix is WS2815, change WS2812 to WS2815 below
  FastLED.addLeds<WS2815, MATRIX_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear(true);
  matrixOff();

  dfSerial.begin(9600, SERIAL_8N1, DF_RX, DF_TX);

  if (!dfPlayer.begin(dfSerial)) {
    Serial.println("DFPlayer not found. Check wiring and SD card.");
    while (true) {
      delay(1000);
    }
  }

  dfPlayer.volume(22); // 0 to 30
  randomSeed(micros());

  Serial.println("System ready.");
}

// =========================
// LOOP
// =========================
void loop() {
  unsigned long now = millis();

  // keep matrix updating whenever enabled
  updateMatrix();

  // if combo mode is active, keep everything on for full 45 seconds
  if (comboActive) {
    runMotor1Slow();
    runMotor2Slow();
    matrixOn();

    if (now - comboStartTime >= COMBO_DURATION) {
      stopComboMode();
    }

    return;
  }

  // debounced button read/control
  if (now - lastReadTime >= DEBOUNCE_MS) {
    lastReadTime = now;

    bool b1 = button1Pressed();
    bool b2 = button2Pressed();
    bool b3 = button3Pressed();

    if (b1 && b2 && b3) {
      startComboMode();
    } else {
      applyNormalButtonControl();
    }
  }
}