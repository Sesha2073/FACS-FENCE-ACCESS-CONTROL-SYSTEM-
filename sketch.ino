#include <Wire.h>
#include <LiquidCrystal_I2C.h>
const uint8_t PULSE_PIN = 2;     
const uint8_t GEN_PIN   = 3;     
const uint8_t LED_CUT   = 9;     
const uint8_t LED_WEAK  = 10;    
const uint8_t BUZZER    = 11;    
const uint8_t A_FENCE   = A0;    
const uint8_t PIR_PIN   = 6;     
const uint8_t SMOKE_PIN = A1;    
const unsigned long CUT_TIMEOUT_MS = 3000;    
const int FENCE_WEAK_THRESHOLD = 300;         
const unsigned long ALARM_DURATION_MS = 30000;
const int SMOKE_THRESHOLD = 400;              
LiquidCrystal_I2C lcd(0x27, 16, 2);
volatile unsigned long lastPulseMillis = 0;
volatile unsigned long lastPulseIsrMillis = 0;
const unsigned long DEBOUNCE_MS = 5;
bool alarmActive = false;
unsigned long alarmStartTime = 0;
String lastStatus = "";
void pulseISR() {
  unsigned long t = millis();
  if (t - lastPulseIsrMillis < DEBOUNCE_MS) return;
  lastPulseIsrMillis = t;
  lastPulseMillis = t;
}

void logEvent(const String &msg) {
  unsigned long now = millis() / 1000;
  int h = (now / 3600) % 24;
  int m = (now / 60) % 60;
  int s = now % 60;
  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", h, m, s);
  Serial.print("[");
  Serial.print(buf);
  Serial.print("] ");
  Serial.println(msg);
}

void setup() {
  Serial.begin(115200);

  pinMode(PULSE_PIN, INPUT_PULLUP);
  pinMode(GEN_PIN, OUTPUT);
  pinMode(LED_CUT, OUTPUT);
  pinMode(LED_WEAK, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pinMode(PIR_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(PULSE_PIN), pulseISR, FALLING);

  lastPulseMillis = millis();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Fence Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();

  Serial.println("Fence monitor started with LCD, PIR, and Smoke detection.");
}

void loop() {
  unsigned long now = millis();

  static unsigned long lastGen = 0;
  if (now - lastGen >= 1000) { 
    digitalWrite(GEN_PIN, HIGH);
    delay(50);                 
    digitalWrite(GEN_PIN, LOW);
    lastGen = now;
  }

  bool cutAlarm = (now - lastPulseMillis > CUT_TIMEOUT_MS);
  int fenceLevel = analogRead(A_FENCE);
  bool weakAlarm = (fenceLevel < FENCE_WEAK_THRESHOLD);

  bool motionDetected = digitalRead(PIR_PIN);

  int smokeLevel = analogRead(SMOKE_PIN);
  bool smokeDetected = (smokeLevel > SMOKE_THRESHOLD);

  String currentStatus;
  if (cutAlarm) {
    currentStatus = "CUT";
  } else if (weakAlarm) {
    currentStatus = "WEAK";
  } else {
    currentStatus = "OK";
  }

  if (motionDetected) currentStatus = "MOTION";
  if (smokeDetected) currentStatus = "SMOKE";

  static String lastEvent = "";
  if (currentStatus != lastEvent) {
    logEvent("Fence " + currentStatus);
    lastEvent = currentStatus;
  }

  if (cutAlarm) {
    if (!alarmActive) {
      alarmActive = true;
      alarmStartTime = now;
    }
    if (now - alarmStartTime < ALARM_DURATION_MS) {
      digitalWrite(LED_CUT, HIGH);
      tone(BUZZER, 1000);
    } else {
      digitalWrite(LED_CUT, HIGH); 
      noTone(BUZZER);              
    }
  } else {
    digitalWrite(LED_CUT, LOW);
    noTone(BUZZER);
    alarmActive = false;
  }

  digitalWrite(LED_WEAK, weakAlarm ? HIGH : LOW);

  if (motionDetected) {
    digitalWrite(LED_CUT, HIGH);
    tone(BUZZER, 1500); 
  }
  if (smokeDetected) {
    digitalWrite(LED_WEAK, HIGH);
    tone(BUZZER, 2000); 
  }

  lcd.setCursor(0, 0);
  lcd.print("Fence Status:   ");
  lcd.setCursor(0, 1);
  lcd.print("                "); 
  lcd.setCursor(0, 1);
  lcd.print(currentStatus);

  delay(100);
}
