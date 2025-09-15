// Blynk and sensor configuration
#define BLYNK_TEMPLATE_ID "TMPL32GPDborV"
#define BLYNK_TEMPLATE_NAME "Smart sync"
#define BLYNK_AUTH_TOKEN "5H3WYsRfokCa508peykuzEcJ0Uit3IgR"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "DHTesp.h"

// WiFi credentials
char ssid[] = "sam";
char password[] = "wsnt4796";

// Pin definitions
#define SOIL_PIN 34
#define RELAY_PIN 26
#define DHTPIN 4
#define TRIG_PIN 12     // Ultrasonic TRIG pin
#define ECHO_PIN 14     // Ultrasonic ECHO pin

// Soil sensor calibration values
int dryValue = 3000;   // ADC in dry soil
int wetValue = 1200;   // ADC in fully wet soil

// Objects
DHTesp dht;

// Pump control variables
int manualPumpState = -1; // -1: auto, 0: manual OFF
bool autoEnabled = true;  // true: auto mode enabled
bool pumpAutoOn = false;  // track pump state

// Tank height in cm (adjust this to your actual tank depth!)
int tankHeight = 30;  

// Blynk: Pump Manual Control (V6)
BLYNK_WRITE(6) {
  int value = param.asInt();
  if (value == 0) {
    manualPumpState = 0; 
    digitalWrite(RELAY_PIN, HIGH);
    Blynk.virtualWrite(V3, 0);  // Pump OFF → LED OFF
    Serial.println("Pump OFF (manual override)");
  } else if (value == 1) {
    manualPumpState = -1; 
    Serial.println("Manual ON, auto resumes");
  }
}

// Blynk: Auto Pump Mode (V5)
BLYNK_WRITE(5) {
  autoEnabled = param.asInt() == 1;
  Serial.print("Auto pump mode: ");
  Serial.println(autoEnabled ? "ENABLED" : "DISABLED");
}

// Ultrasonic read function
long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  long distance = duration * 0.034 / 2;
  return distance;
}

void setup() {
  Serial.begin(115200);

  // Pin setup
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Pump OFF initially
  dht.setup(DHTPIN, DHTesp::DHT11);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // WiFi setup
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected!");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
}

void loop() {
  Blynk.run();

  // Soil sensor with calibration
  int soilRaw = analogRead(SOIL_PIN);
  int soilPercent = map(soilRaw, dryValue, wetValue, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  // DHT sensor
  TempAndHumidity data = dht.getTempAndHumidity();
  float t = data.temperature;
  float h = data.humidity;

  // Ultrasonic water gauge
  long distance = readDistanceCM();
  int waterLevel = tankHeight - distance; 
  if (waterLevel < 0) waterLevel = 0;
  int waterPercent = map(waterLevel, 0, tankHeight, 0, 100);

  // Serial monitor
  Serial.print("Soil Raw: "); Serial.print(soilRaw);
  Serial.print(" | Soil %: "); Serial.println(soilPercent);
  Serial.print("Temperature: "); Serial.print(t); Serial.print(" °C | Humidity: ");
  Serial.print(h); Serial.println(" %");
  Serial.print("Water Distance: "); Serial.print(distance); Serial.print(" cm | Water %: ");
  Serial.println(waterPercent);

  // Blynk updates
  Blynk.virtualWrite(V2, soilPercent);   // Soil %
  Blynk.virtualWrite(V0, t);             // Temperature
  Blynk.virtualWrite(V1, h);             // Humidity
  Blynk.virtualWrite(V7, waterPercent);  // Tank level %

  // Pump control
  if (manualPumpState == 0) {
    // Manual OFF
    digitalWrite(RELAY_PIN, HIGH);
    Blynk.virtualWrite(V4, 0);  // LED OFF
    Serial.println("Pump Status: OFF (manual override)");

  } else if (autoEnabled && manualPumpState == -1) {
    // Auto mode
    if (soilPercent < 35 && waterPercent > 20) {  // Only ON if enough water
      pumpAutoOn = true;
    }
    if (soilPercent > 98 || waterPercent <= 20) { // OFF if soil wet or tank low
      pumpAutoOn = false;
    }

    if (pumpAutoOn) {
      digitalWrite(RELAY_PIN, LOW);
      Blynk.virtualWrite(V3, 1);  // LED ON
      Serial.println("Pump Status: ON (auto)");
    } else {
      digitalWrite(RELAY_PIN, HIGH);
      Blynk.virtualWrite(V3, 0);  // LED OFF
      if (waterPercent <= 20) {
        Serial.println("Pump Status: OFF (low water safety cutoff)");
      } else {
        Serial.println("Pump Status: OFF (auto)");
      }
    }
  }

  Serial.println("-----------------------------");
  delay(2000);
}
