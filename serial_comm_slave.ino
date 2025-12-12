/*************************************************************
 *                       ESP32 SLAVE NODE
 *           Industrial Sensor Acquisition & Safety Unit
 *
 *  ▌ HARDWARE COMPONENTS
 *    - ESP32 Dev Module
 *    - DHT11 Sensor (Temperature + Humidity)
 *    - MQ2 Gas Sensor (Analog smoke/gas detection)
 *    - Relay Module (load switching / emergency cutoff)
 *    - GPIO-based Blynk virtual control support
 *
 *  ▌ SOFTWARE COMPONENTS
 *    - MQTT Client (HiveMQ Broker)
 *    - Blynk IoT Platform for dashboard updates & control
 *    - ArduinoJson for data packaging in JSON format
 *    - DHT Sensor Library
 *
 *  ▌ CORE RESPONSIBILITIES
 *    - Continuously read Temperature, Humidity, and Gas levels
 *    - Publish structured sensor data to MASTER via MQTT
 *    - Detect WARNING and CRITICAL environmental conditions
 *    - Trigger emergency relay OFF during CRITICAL alerts
 *    - Publish ALERT and STATUS messages for master processing
 *    - Receive relay control commands via MQTT/Blynk
 *    - Sync sensor readings and relay status to Blynk dashboard
 *
 *  ▌ KEY FUNCTIONS
 *    - connectToWiFi()         → Connects ESP32 to WiFi network
 *    - connectToMQTT()         → Ensures stable MQTT connection
 *    - mqttCallback()          → Handles incoming RELAY_ON/OFF commands
 *    - publishSensorData()     → Sends temperature/humidity/gas JSON packet
 *    - checkThresholds()       → Evaluates WARNING/CRITICAL conditions
 *    - publishAlert()          → Publishes alert level to MASTER
 *    - publishStatus()         → Sends relay state & uptime
 *    - updateBlynk()           → Updates real-time dashboard widgets
 *
 *************************************************************/

/* ====================== */
/*      SLAVE NODE        */
/* ====================== */

/* ===== BLYNK CONFIGURATION ===== */
#define BLYNK_TEMPLATE_ID   "TMPL31MQgk8Ns"
#define BLYNK_TEMPLATE_NAME "INDUSTRIAL PROJECT"
#define BLYNK_AUTH_TOKEN    "B-W4p-gdly7J-fKZyo0zxOGaSNTiqogV"

/* ===== LIBRARIES ===== */
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <BlynkSimpleEsp32.h>

/* ===== WIFI CONFIG ===== */
#define WIFI_SSID       "Devansh"
#define WIFI_PASSWORD   "devansh01"

/* ===== MQTT CONFIG ===== */
#define MQTT_SERVER     "broker.hivemq.com"
#define MQTT_PORT       1883
#define MQTT_USER       "hivemq.webclient.1754161246290"
#define MQTT_PASSWORD   "pwJQFG8$9f<ksPN#z07!"

/* ===== MQTT TOPICS ===== */
#define TOPIC_DATA      "industrial/slave/data"
#define TOPIC_ALERT     "industrial/slave/alert"
#define TOPIC_STATUS    "industrial/slave/status"
#define TOPIC_CONTROL   "industrial/slave/control"

/* ===== HARDWARE PINS ===== */
#define DHTPIN          4
#define DHTTYPE         DHT11
#define MQ2_PIN         34
#define RELAY_PIN       25

/* ===== THRESHOLDS ===== */
#define TEMP_HIGH       40
#define HUMID_HIGH      80
#define GAS_WARNING     300
#define GAS_CRITICAL    500

/* ===== BLYNK VIRTUAL PINS ===== */
#define VPIN_TEMP       V0
#define VPIN_HUMID      V1
#define VPIN_GAS        V2
#define VPIN_RELAY      V3
#define VPIN_CONTROL    V4

/* ===== GLOBAL OBJECTS ===== */
WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

/* ===== GLOBAL VARIABLES ===== */
bool relayState = false;
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 10000; // 10 seconds

/* ===== FUNCTION DECLARATIONS ===== */
void connectToWiFi();
void connectToMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishSensorData();
void checkThresholds(float temp, float hum, int gas);
void updateBlynk(float temp, float hum, int gas);
void publishAlert(const char* level);
void publishStatus();

/* ===== SETUP ===== */
void setup() {
  Serial.begin(115200);
  Serial.println("[SYSTEM] Booting SLAVE node...");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relay ON initially
  dht.begin();
  Serial.println("[INIT] DHT sensor initialized.");
  
  connectToWiFi();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  Serial.println("[MQTT] Configured broker and callback.");

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  Serial.println("[BLYNK] Connected and ready!");

  timer.setInterval(publishInterval, publishSensorData);
}

/* ===== MAIN LOOP ===== */
void loop() {
  Blynk.run();
  timer.run();
  
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();
}

/* ===== BLYNK CONTROL ===== */
BLYNK_WRITE(VPIN_CONTROL) {
  relayState = param.asInt();
  digitalWrite(RELAY_PIN, relayState);
  publishStatus();
  Serial.printf("[BLYNK] Relay set to: %s\n", relayState ? "ON" : "OFF");
}

/* ===== NETWORK FUNCTIONS ===== */
void connectToWiFi() {
  Serial.print("[WIFI] Connecting...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WIFI] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
}
void connectToMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Connecting to broker...");
    if (mqttClient.connect("ESP32_Slave_Node", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected!");
      mqttClient.subscribe(TOPIC_CONTROL);
      Serial.println("[MQTT] Subscribed to: " TOPIC_CONTROL);
    } else {
      Serial.printf("failed, rc=%d. Retrying...\n", mqttClient.state());
      delay(200);
    }
  }
}

/* ===== DATA PUBLISHING FUNCTIONS ===== */
void publishSensorData() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int gas = analogRead(MQ2_PIN);

  // Validate readings
  if (isnan(temp)) temp = -1;
  if (isnan(hum)) hum = -1;

  // Publish to MQTT
  StaticJsonDocument<200> doc;
  doc["node"] = "slave_1";
  doc["type"] = "data";
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  doc["gas"] = gas;
  doc["timestamp"] = millis();

  char payload[200];
  serializeJson(doc, payload);
  mqttClient.publish(TOPIC_DATA, payload);
  Serial.printf("[MQTT] Published data: %s\n", payload);

  // Update Blynk
  updateBlynk(temp, hum, gas);

  // Check thresholds
  checkThresholds(temp, hum, gas);
}

void publishAlert(const char* level) {
  StaticJsonDocument<150> doc;
  doc["node"] = "slave_1";
  doc["type"] = "alert";
  doc["level"] = level;
  doc["timestamp"] = millis();

  char payload[150];
  serializeJson(doc, payload);
  mqttClient.publish(TOPIC_ALERT, payload);
  Serial.printf("[ALERT] Published: %s\n", payload);
  
  Blynk.logEvent("system_alert", String("Alert: ") + level);
}

void publishStatus() {
  StaticJsonDocument<100> doc;
  doc["node"] = "slave_1";
  doc["type"] = "status";
  doc["relay"] = digitalRead(RELAY_PIN) ? "ON" : "OFF";
  doc["uptime"] = millis() / 10000;

  char payload[100];
  serializeJson(doc, payload);
  mqttClient.publish(TOPIC_STATUS, payload, true);
  Serial.printf("[STATUS] Published: %s\n", payload);
}

/* ===== SAFETY FUNCTIONS ===== */
void checkThresholds(float temp, float hum, int gas) {
  if (gas > GAS_CRITICAL || temp > TEMP_HIGH || hum > HUMID_HIGH) {
    Serial.println("CRITICAL threshold exceeded!");
    publishAlert("CRITICAL");
    digitalWrite(RELAY_PIN, LOW); // Emergency OFF
    relayState = false;
    Blynk.virtualWrite(VPIN_CONTROL, 0);
  } 
  else if (gas > GAS_WARNING) {
    Serial.println("Warning: Gas level high");
    publishAlert("WARNING");
  }
}

/* ===== BLYNK UPDATE ===== */
void updateBlynk(float temp, float hum, int gas) {
  if (!isnan(temp)) Blynk.virtualWrite(VPIN_TEMP, temp);
  if (!isnan(hum)) Blynk.virtualWrite(VPIN_HUMID, hum);
  Blynk.virtualWrite(VPIN_GAS, gas);
  Blynk.virtualWrite(VPIN_RELAY, relayState ? 1 : 0);
}

/* ===== MQTT CALLBACK ===== */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String message = (char*)payload;
  Serial.printf("[MQTT] Received on %s: %s\n", topic, message.c_str());

  if (strcmp(topic, TOPIC_CONTROL) == 0) {
    if (message == "RELAY_ON") {
      digitalWrite(RELAY_PIN, HIGH);
      relayState = true;
    } 
    else if (message == "RELAY_OFF") {
      digitalWrite(RELAY_PIN, LOW);
      relayState = false;
    }
    Blynk.virtualWrite(VPIN_CONTROL, relayState ? 1 : 0);
    publishStatus();
  }
}