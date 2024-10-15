#include <Wire.h>
#include <MQUnifiedsensor.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

/************************* WiFi and Server Config *************************/
const char* ssid = "your_wifi";
const char* password = "your_password_wifi";
const char* serverUrl = "http://yourip/data";  // Python Server URL

/************************* DHT22 Configuration *******************************/
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

/************************* MQ-2 Configuration ******************************/
#define Board                   ("ESP-32")
#define Pin                     (35)  // Analog input for MQ-2 sensor
#define Voltage_Resolution      (3.3)
#define ADC_Bit_Resolution      (12)
#define RatioMQ2CleanAir        (9.83)

MQUnifiedsensor MQ2(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, "MQ-2");

/************************* LED and Buzzer Pins ******************************/
#define LED1_GREEN 13  // Green LED
#define LED2_YELLOW 12 // Yellow LED
#define LED3_RED 14    // Red LED
#define BUZZER 16      // Buzzer

/************************* Timing Variables ******************************/
unsigned long lastDataSent = 0;
unsigned long startTimeCO = 0;
unsigned long startTimeSmoke = 0;
unsigned long startTimeLPG = 0;
unsigned long startTimeHeatIndex = 0;
unsigned long recoveryStartTime = 0; // Track time when danger condition returns to normal

bool dangerActive = false;
bool warningActive = false;
bool recoveryActive = false; // Track if system is in recovery state

/************************* Setup Function *******************************/
void setup() {
  Serial.begin(115200);

  // Initialize DHT22 sensor
  dht.begin();

  // Initialize LEDs and Buzzer
  pinMode(LED1_GREEN, OUTPUT);
  pinMode(LED2_YELLOW, OUTPUT);
  pinMode(LED3_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(LED1_GREEN, HIGH);  // Default state: Green LED ON
  digitalWrite(LED2_YELLOW, LOW);
  digitalWrite(LED3_RED, LOW);
  digitalWrite(BUZZER, LOW);

  // Initialize MQ-2 sensor
  MQ2.setRegressionMethod(1);  // _PPM = a * ratio^b
  MQ2.init();

  // Calibrate MQ-2 in clean air
  Serial.print("Calibrating MQ-2, please wait...");
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ2.update();
    calcR0 += MQ2.calibrate(RatioMQ2CleanAir);
    Serial.print(".");
  }
  MQ2.setR0(calcR0 / 10);
  Serial.println("\nMQ-2 calibration done.");
  MQ2.serialDebug(true);  // Enable detailed output

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

/************************* Danger Alert Function *******************************/
void checkDangerConditions(float co_ppm, float smoke_ppm, float lpg_ppm, float heatIndexC) {
  unsigned long currentTime = millis();

  // Reset warning and danger flags
  warningActive = false;
  dangerActive = false;

  // CO Condition Checks
  if (co_ppm >= 1000) {
    if (startTimeCO == 0) startTimeCO = currentTime;
    if (currentTime - startTimeCO >= 5000) {  // 5 sec
      dangerActive = true;
    } else {
      warningActive = true;
    }
  } else if (co_ppm >= 400) {
    if (startTimeCO == 0) startTimeCO = currentTime;
    if (currentTime - startTimeCO >= 24000) {  // 4 minutes
      dangerActive = true;
    } else {
      warningActive = true;
    }
  } else if (co_ppm >= 150) {
    if (startTimeCO == 0) startTimeCO = currentTime;
    if (currentTime - startTimeCO >= 600000) {  // 10 minutes
      dangerActive = true;
    } else {
      warningActive = true;
    }
  } else {
    startTimeCO = 0;  // Reset timer if condition not met
  }

  // Smoke Condition Checks
  if (smoke_ppm >= 1000) {
    if (startTimeSmoke == 0) startTimeSmoke = currentTime;
    if (currentTime - startTimeSmoke >= 5000) {  // 5 sec
      dangerActive = true;
    } else {
      warningActive = true;
    }
  }  else if (smoke_ppm >= 350) {
    if (startTimeSmoke == 0) startTimeSmoke = currentTime;
    if (currentTime - startTimeSmoke >= 2400) {  // 4 minutes
      dangerActive = true;
    } else {
      warningActive = true;
    }
  } else {
    startTimeSmoke = 0;  // Reset timer if condition not met
  }

  // LPG Condition Checks
  if (lpg_ppm >= 1000) {
    if (startTimeLPG == 0) startTimeLPG = currentTime;
    if (currentTime - startTimeLPG >= 5000) {  // 5 seconds
      dangerActive = true;
    } else {
      warningActive = true;
    }
  } else if (lpg_ppm >= 500) {
    if (startTimeLPG == 0) startTimeLPG = currentTime;
    if (currentTime - startTimeLPG >= 180000) {  // 3 minutes
      dangerActive = true;
    } else {
      warningActive = true;
    }
  } else {
    startTimeLPG = 0;  // Reset timer if condition not met
  }

  // Heat Index Condition Checks
  if (heatIndexC >= 54 && heatIndexC <= 68) {
    if (startTimeHeatIndex == 0) startTimeHeatIndex = currentTime;
    if (currentTime - startTimeHeatIndex >= 180000) {  // 3 minutes
      dangerActive = true;
    } else {
      warningActive = true;
    }
  } else if (heatIndexC >= 36 && heatIndexC <= 53) {
    if (startTimeHeatIndex == 0) startTimeHeatIndex = currentTime;
    if (currentTime - startTimeHeatIndex >= 360000) {  // 6 minutes
      dangerActive = true;
    } else {
      warningActive = true;
    }
  } else {
    startTimeHeatIndex = 0;  // Reset timer if condition not met
  }

  // Update LED status based on flags
  if (dangerActive) {
    activateDanger();  // Danger condition met
    recoveryStartTime = 0; // Reset recovery time
  } else if (warningActive) {
    activateWarning();  // Warning condition met
    recoveryStartTime = 0; // Reset recovery time
  } else {
    // Normal state, check if recovery is needed
    if (recoveryActive) {
      if (currentTime - recoveryStartTime >= 60000) {  // 1 minute of normal readings
        recoveryActive = false;  // Exit recovery state after 1 minute
        resetToNormal();  // Reset to normal state
      }
    } else {
      recoveryActive = true;
      recoveryStartTime = currentTime;  // Start recovery timer
    }
  }
}

/************************* Activate Warning Function *******************************/
void activateWarning() {
  // Turn on Yellow LED (Warning state)
  digitalWrite(LED1_GREEN, LOW);
  digitalWrite(LED2_YELLOW, HIGH);
  digitalWrite(LED3_RED, LOW);
  digitalWrite(BUZZER, LOW);
}

/************************* Activate Danger Function *******************************/
void activateDanger() {
  // Turn on Red LED and Buzzer (Danger state)
  digitalWrite(LED1_GREEN, LOW);
  digitalWrite(LED2_YELLOW, LOW);
  digitalWrite(LED3_RED, HIGH);
  digitalWrite(BUZZER, HIGH);
  recoveryStartTime = 0;  // Reset recovery time during danger
  recoveryActive = false; // Danger state overrides recovery
}

/************************* Reset to Normal Function *******************************/
void resetToNormal() {
  // Turn on Green LED and turn off other LEDs and buzzer (Normal state)
  digitalWrite(LED1_GREEN, HIGH);
  digitalWrite(LED2_YELLOW, LOW);
  digitalWrite(LED3_RED, LOW);
  digitalWrite(BUZZER, LOW);
}

/************************* Main Loop Function *******************************/
void loop() {
  // Read DHT22 data
  float humidity = dht.readHumidity();
  float temperatureC = dht.readTemperature();
  float heatIndexC = dht.computeHeatIndex(temperatureC, humidity, false);

  // Read MQ-2 data (LPG, CO, Smoke)
  MQ2.update();
  MQ2.setA(574.25); MQ2.setB(-2.222);  // LPG
  float lpg_ppm = MQ2.readSensor();
  
  MQ2.setA(36974); MQ2.setB(-3.109);   // CO
  float co_ppm = MQ2.readSensor();

    MQ2.setA(941.51); MQ2.setB(-2.338);  // Smoke
  float smoke_ppm = MQ2.readSensor();

  // Constrain gas values to a safe range (0-1000 PPM)
  lpg_ppm = constrain(lpg_ppm, 0, 1000);
  co_ppm = constrain(co_ppm, 0, 1000);
  smoke_ppm = constrain(smoke_ppm, 0, 1000);

  // Send sensor data to the Serial Monitor
  Serial.println("====== Sensor Measurements ======");
  Serial.print("Temperature: ");
  Serial.print(temperatureC);
  Serial.println(" °C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Heat Index: ");
  Serial.print(heatIndexC);
  Serial.println(" °C");

  Serial.print("LPG: ");
  Serial.print(lpg_ppm);
  Serial.println(" ppm");

  Serial.print("CO: ");
  Serial.print(co_ppm);
  Serial.println(" ppm");

  Serial.print("Smoke: ");
  Serial.print(smoke_ppm);
  Serial.println(" ppm");
  Serial.println("===============================");

  // Send sensor data to the Python server
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Specify the server URL
    http.begin(serverUrl);

    // Create a JSON string with sensor data
    String jsonData = String("{\"temperature\":") + temperatureC + 
                      ",\"humidity\":" + humidity + 
                      ",\"heatIndex\":" + heatIndexC + 
                      ",\"lpg\":" + lpg_ppm + 
                      ",\"co\":" + co_ppm + 
                      ",\"smoke\":" + smoke_ppm + "}";

    // Specify content type as JSON
    http.addHeader("Content-Type", "application/json");

    // Send HTTP POST request
    int httpResponseCode = http.POST(jsonData);

    // Check response
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);  // HTTP response code
      Serial.println(response);          // Response message
    } else {
      Serial.print("Error in sending POST request: ");
      Serial.println(httpResponseCode);
    }

    // End the HTTP connection
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }

  // Check for danger or warning conditions based on sensor readings
  checkDangerConditions(co_ppm, smoke_ppm, lpg_ppm, heatIndexC);

  // Delay to avoid spamming the loop too quickly
  delay(30000);  // 30-sec delay for readability
}

