/*************************************************************
 *                     ESP32 MASTER NODE
 *        Industrial Wireless Monitoring & Control Unit
 *
 *  ▌ HARDWARE COMPONENTS
 *    - ESP32 Dev Module (Master Controller)
 *    - WiFi Router (Network Access)
 *
 *  ▌ SOFTWARE COMPONENTS
 *    - MQTT Broker (HiveMQ) for Slave ↔ Master communication
 *    - Blynk IoT Platform for dashboard visualization & control
 *    - Telegram Bot API for remote alert notifications
 *    - WebServer Library for local dashboard hosting
 *    - ArduinoJson for JSON message parsing
 *
 *  ▌ CORE RESPONSIBILITIES
 *    - Receive real-time sensor data from Slave (Temp, Humidity, Gas)
 *    - Process WARNING / CRITICAL alerts from Slave
 *    - Force safety shutdown (RELAY_OFF) on critical conditions
 *    - Provide multi-channel dashboards:
 *          • Blynk App Dashboard
 *          • Local Web Dashboard
 *    - Provide remote-only user control of relay via Blynk/Web
 *    - Forward system alerts through Telegram for safety
 *
 *  ▌ KEY FUNCTIONS
 *    - connectToWiFi()       → Establishes WiFi connection
 *    - connectToMQTT()       → Connects to MQTT broker & subscribes
 *    - mqttCallback()        → Handles incoming MQTT messages
 *    - handleSensorData()    → Updates and stores environmental data
 *    - handleAlert()         → Processes WARNING/CRITICAL alerts
 *    - handleStatus()        → Updates relay state from Slave
 *    - sendControlCommand()  → Sends RELAY_ON / RELAY_OFF to Slave
 *    - sendTelegramAlert()   → Sends Telegram notifications
 *    - handleRoot()          → Renders local web dashboard
 *    - handleToggle()        → Web button handler for relay control
 *    - updateDashboard()     → Syncs Blynk dashboard values
 *
 *************************************************************/

/* ====================== */
/*      MASTER NODE       */
/* ====================== */

/* ===== BLYNK CONFIGURATION ===== */
#define BLYNK_TEMPLATE_ID   "TMPL31MQgk8Ns"
#define BLYNK_TEMPLATE_NAME "INDUSTRIAL PROJECT"
#define BLYNK_AUTH_TOKEN    "B-W4p-gdly7J-fKZyo0zxOGaSNTiqogV"

/* ===== LIBRARIES ===== */
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <BlynkSimpleEsp32.h>
#include <WebServer.h>

/* ===== CONFIGURATION ===== */
// WiFi
#define WIFI_SSID       "Devansh"
#define WIFI_PASSWORD   "devansh01"

// MQTT
#define MQTT_SERVER     "broker.hivemq.com"
#define MQTT_PORT       1883
#define MQTT_USER       "hivemq.webclient.1754161246290"
#define MQTT_PASSWORD   "pwJQFG8$9f<ksPN#z07!"

// MQTT Topics
#define TOPIC_DATA      "industrial/slave/data"
#define TOPIC_ALERT     "industrial/slave/alert"
#define TOPIC_STATUS    "industrial/slave/status"
#define TOPIC_CONTROL   "industrial/slave/control"

// Telegram
#define TELEGRAM_BOT_TOKEN "7899174353:AAFQ-yLp_D9sGwahzpjGgpF4snHXudIOCrc"
#define CHAT_ID            "1135443651"

// Blynk Virtual Pins
#define VPIN_TEMP       V0
#define VPIN_HUMID      V1
#define VPIN_GAS        V2
#define VPIN_RELAY      V3
#define VPIN_ALERT      V4

/* ===== GLOBAL OBJECTS ===== */
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiClientSecure secureClient;
UniversalTelegramBot bot(TELEGRAM_BOT_TOKEN, secureClient);
WebServer server(80);
BlynkTimer timer;

/* ===== GLOBAL VARIABLES ===== */
struct SensorData {
  float temperature = 0;
  float humidity = 0;
  int gas = 0;
  String relayState = "OFF";
  String lastAlert = "NORMAL";
  unsigned long lastUpdate = 0;
} slaveData;

/* ===== FUNCTION DECLARATIONS ===== */
void connectToWiFi();
void connectToMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleSensorData(const JsonObject& data);
void handleAlert(const JsonObject& alert);
void handleStatus( const JsonObject& status);
void sendControlCommand(const char* command);
void sendTelegramAlert(const String& message);
void handleRoot();
void handleToggle();
void updateDashboard();

/* ===== SETUP ===== */
void setup() {
  Serial.begin(115200);
  Serial.println("[SYSTEM] Booting MASTER node...");

  connectToWiFi();

  secureClient.setInsecure();
  Serial.println("[TELEGRAM] Bot client set to insecure mode.");
  Serial.println("[TELEGRAM] Initialized. Ready to send alerts.");

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  Serial.println("[MQTT] Configured broker and callback.");

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  Serial.println("[BLYNK] Connected and ready!");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/toggle", HTTP_POST, handleToggle);
  server.begin();
  Serial.println("[WEB SERVER] Server started on port 80.");
  Serial.println("[WEB SERVER] Access dashboard at: http://" + WiFi.localIP().toString());

  timer.setInterval(15000, updateDashboard);
}


/* ===== MAIN LOOP ===== */
void loop() {
  Blynk.run();
  timer.run();
  server.handleClient();
  
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();

  // Check for Telegram messages every 2 seconds
  static unsigned long lastTelegramCheck = 0;
  if (millis() - lastTelegramCheck > 2000) {
    bot.getUpdates(bot.last_message_received + 1);
    lastTelegramCheck = millis();
  }
}

/* ===== NETWORK FUNCTIONS ===== */
void connectToWiFi() {
  Serial.print("[WIFI] Connecting...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.printf("\n[WIFI] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
}

void connectToMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("[MQTT] Connecting to broker...");
    if (mqttClient.connect("ESP32_Master_Node", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected!");
      mqttClient.subscribe(TOPIC_DATA);
      mqttClient.subscribe(TOPIC_ALERT);
      mqttClient.subscribe(TOPIC_STATUS);
      Serial.println("[MQTT] Subscribed to: DATA, ALERT, STATUS");
    } else {
      Serial.printf("failed, rc=%d. Retrying...\n", mqttClient.state());
      delay(3000);
    }
  }
}


/* ===== MQTT CALLBACK ===== */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String json = (char*)payload;
  
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.printf("JSON parse error: %s\n", err.c_str());
    return;
  }

  String type = doc["type"];
  if (strcmp(topic, TOPIC_DATA) == 0 && type == "data") {
    handleSensorData(doc.as<JsonObject>());
  } 
  else if (strcmp(topic, TOPIC_ALERT) == 0 && type == "alert") {
    handleAlert(doc.as<JsonObject>());
  } 
  else if (strcmp(topic, TOPIC_STATUS) == 0 && type == "status") {
    handleStatus(doc.as<JsonObject>());
  }
}

/* ===== DATA HANDLERS ===== */
void handleSensorData(const JsonObject& data) {
  slaveData.temperature = data["temperature"];
  slaveData.humidity = data["humidity"];
  slaveData.gas = data["gas"];
  slaveData.lastUpdate = millis();
  
  Serial.printf("[DATA] Temp: %.1fC, Hum: %.1f%%, Gas: %d\n", 
               slaveData.temperature, slaveData.humidity, slaveData.gas);
}

void handleAlert( const JsonObject& alert) {
  String level = alert["level"];
  slaveData.lastAlert = level;
  
  Serial.printf("[ALERT] Level: %s\n", level.c_str());
  
  if (level == "CRITICAL") {
    sendControlCommand("RELAY_OFF");
    sendTelegramAlert("🚨 CRITICAL ALERT! Relay forced OFF");
    Blynk.logEvent("critical_alert", "CRITICAL ALERT from slave!");
  } 
  else if (level == "WARNING") {
    sendTelegramAlert("⚠ Warning from slave node");
  }
}

void handleStatus(const JsonObject& status) {
  slaveData.relayState = status["relay"].as<String>();
  Serial.printf("[STATUS] Relay: %s\n", slaveData.relayState.c_str());
  Blynk.virtualWrite(VPIN_RELAY, slaveData.relayState == "ON" ? 1 : 0);
}

/* ===== CONTROL FUNCTIONS ===== */
void sendControlCommand(const char* command) {
  mqttClient.publish(TOPIC_CONTROL, command);
  Serial.printf("[CONTROL] Sent: %s\n", command);
}

BLYNK_WRITE(VPIN_RELAY) {
  int state = param.asInt();
  sendControlCommand(state ? "RELAY_ON" : "RELAY_OFF");
}

/* ===== TELEGRAM FUNCTIONS ===== */
void sendTelegramAlert(const String& message) {
  bot.sendMessage(CHAT_ID, "🔔 Industrial Alert:\n" + message, "");
  Serial.printf("[TELEGRAM] Sent alert: %s\n", message.c_str());
}

/* ===== WEB SERVER FUNCTIONS ===== */
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Industrial Monitor</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;margin:0;padding:20px;}";
  html += ".card{background:#f8f9fa;border-radius:10px;padding:15px;margin:10px;display:inline-block;width:120px;}";
  html += ".alert{color:red;font-weight:bold;}";
  html += "button{padding:10px 20px;background:#007bff;color:white;border:none;border-radius:5px;margin:10px;}";
  html += "</style></head><body>";
  html += "<h1>Industrial Monitoring System</h1>";
  
  html += "<div class='card'><h3>Temperature</h3><p>" + String(slaveData.temperature) + " °C</p></div>";
  html += "<div class='card'><h3>Humidity</h3><p>" + String(slaveData.humidity) + " %</p></div>";
  html += "<div class='card'><h3>Gas Level</h3><p>" + String(slaveData.gas) + "</p></div>";
  html += "<div class='card'><h3>Relay</h3><p>" + slaveData.relayState + "</p></div>";
  html += String("<div class='card'><h3>Status</h3><p class='") + 
        (slaveData.lastAlert != "NORMAL" ? "alert" : "") + 
        "'>" + slaveData.lastAlert + "</p></div>";
  
  html += "<form action='/toggle' method='POST'>";
  html += "<button type='submit'>Toggle Relay</button>";
  html += "</form>";
  
  html += "<p>Last update: " + String((millis() - slaveData.lastUpdate)/1000) + "s ago</p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}


void handleToggle() {
  if (slaveData.relayState == "ON") {
    sendControlCommand("RELAY_OFF");
  } else {
    sendControlCommand("RELAY_ON");
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

/* ===== DASHBOARD UPDATE ===== */
void updateDashboard() {
  // Update Blynk
  Blynk.virtualWrite(VPIN_TEMP, slaveData.temperature);
  Blynk.virtualWrite(VPIN_HUMID, slaveData.humidity);
  Blynk.virtualWrite(VPIN_GAS, slaveData.gas);
  Blynk.virtualWrite(VPIN_RELAY, slaveData.relayState == "ON" ? 1 : 0);
  
  if (slaveData.lastAlert != "NORMAL") {
    Blynk.virtualWrite(VPIN_ALERT, slaveData.lastAlert);
  }
}