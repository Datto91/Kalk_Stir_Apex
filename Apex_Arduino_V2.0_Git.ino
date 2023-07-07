#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <base64.h>
#include <AccelStepper.h>

const int ledPin = 2;
const int enablePin = 15;
const int ms1Pin = 13;
const int ms2Pin = 12;

#define STEP_PIN 14
#define DIR_PIN 16

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

#define WIFI_SSID "YOUR_WIFI_SSID" 
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD" 

const char* url = "http://YOUR_URL/cgi-bin/status.json";
const char* username = "YOUR_USERNAME";
const char* password = "YOUR_PASSWORD";

unsigned long previousMillis = 0;
const unsigned long interval = 5000; // fetch every 5 seconds


String getAuthorizationHeader(const char *username, const char *password) {
  String credentials = String(username) + ":" + String(password);
  String authHeader = "Basic " + base64::encode(credentials);
  return authHeader;
}

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  pinMode(ms1Pin, OUTPUT);
  pinMode(ms2Pin, OUTPUT);

  stepper.setMaxSpeed(2000);
  stepper.setAcceleration(100);

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void controlStepperMotor(bool on) {
  if (on) {
    stepper.runSpeed();
  } else {
    stepper.stop();
    stepper.setCurrentPosition(0);
  }
}

void setMicrostepping(int ms1, int ms2) {
  digitalWrite(ms1Pin, ms1);
  digitalWrite(ms2Pin, ms2);
}

void printJsonStatus(String& json) {
  DynamicJsonDocument doc(10240);
  deserializeJson(doc, json);

  JsonArray outputs = doc["istat"]["outputs"];

  for (JsonObject obj : outputs) {
    String name = obj["name"].as<String>();
    if (name == "KALK_STIR") {
      String status = obj["status"][0].as<String>();
      if (status == "ON"||status == "AON") {
        digitalWrite(ledPin, LOW);
        digitalWrite(enablePin, LOW);
        setMicrostepping(LOW, LOW);
        controlStepperMotor(true);
      } else if (status == "OFF"||status == "AOF") {
        digitalWrite(ledPin, HIGH);
        digitalWrite(enablePin, HIGH);
        controlStepperMotor(false);
      }
      Serial.print("Status of ");
      Serial.print(name);
      Serial.print(": ");
      Serial.println(status);
    }
  }
}

bool direction = true; // Global variable to store the direction

// ...

void fetchHttpResponse() {
  WiFiClient wifiClient;
  HTTPClient http;
  http.begin(wifiClient, url);
  http.addHeader("Authorization", getAuthorizationHeader(username, password));

  int httpCode = http.GET();

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      printJsonStatus(payload);

      // Change the direction
      if (direction) {
        stepper.setSpeed(1000);
      } else {
        stepper.setSpeed(-1000);
      }
      direction = !direction; // Reverse the direction for the next fetch

    } else {
      Serial.print("HTTP request failed with code: ");
      Serial.println(httpCode);
    }
  } else {
    Serial.println("Error on HTTP request");
  }

  http.end();
}

// ...

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    fetchHttpResponse();
  }

  if (stepper.speed() != 0) {
    stepper.runSpeed();
  }
}
