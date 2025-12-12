# 🟦 **ESP32 Industrial Monitoring & Control System (Master–Slave MQTT Architecture)**

A robust industrial IoT system using **two ESP32 boards** communicating over **MQTT**, with **real-time dashboards**, **remote relay control**, and **safety-critical alerts** delivered via **Telegram**, **Web UI**, and **Blynk IoT**.

---

# 📌 **Project Overview**

This project implements a complete **wireless industrial monitoring and control architecture** using:

### 🔵 **Slave ESP32 Node**

Handles sensing and safety:

* Reads **Temperature (DHT11)**
* Reads **Humidity (DHT11)**
* Reads **Gas concentration (MQ2)**
* Publishes:

  * `DATA` packets (sensor readings)
  * `ALERT` packets (WARNING / CRITICAL)
  * `STATUS` packets (relay state, uptime)
* Executes **relay commands** from the Master
* Performs **local emergency shutdown**
* Updates its **Blynk dashboard**

### 🟣 **Master ESP32 Node**

Acts as the supervisory controller:

* Subscribes to all MQTT packets from Slave
* Displays data on:

  * **Blynk dashboard**
  * **Local Web Dashboard**
  * **Serial Monitor**
* Sends relay control commands (`RELAY_ON`, `RELAY_OFF`)
* Handles **Telegram alerts** for remote safety notifications
* Implements real-time **alert processing logic**
* Hosts web relay control interface

---

# 🏗 **System Architecture**

```
                 ┌──────────────────────────────┐
                 │         MASTER ESP32         │
                 │  - MQTT Subscriber           │
                 │  - Relay Control (MQTT)      │
                 │  - Telegram Alerts           │
                 │  - Web Dashboard             │
                 │  - Blynk Cloud Dashboard     │
                 └──────────────────────────────┘
                           ▲          │
                           │          │ MQTT
                           ▼          │
                 ┌──────────────────────────────┐
                 │          SLAVE ESP32         │
                 │  - DHT11 (Temp/Humidity)     │
                 │  - MQ2 Gas Sensor            │
                 │  - Relay Control Module      │
                 │  - MQTT Publisher            │
                 │  - Blynk Cloud Updates       │
                 └──────────────────────────────┘
```

---

# 🛠 **Hardware Components**

### **Slave Node**

| Component        | Description                |
| ---------------- | -------------------------- |
| ESP32 Dev Module | Sensor & control unit      |
| DHT11            | Temperature & humidity     |
| MQ2 Sensor       | Gas/smoke measurements     |
| Relay Module     | Equipment shutdown control |

### **Master Node**

| Component        | Description                |
| ---------------- | -------------------------- |
| ESP32 Dev Module | Supervisory control system |
| WiFi Router      | MQTT communication medium  |

---

# 🔌 **Wiring Summary (Slave Node)**

| Component      | ESP32 Pin |
| -------------- | --------- |
| DHT11 Data     | GPIO 4    |
| MQ2 Analog Out | GPIO 34   |
| Relay Input    | GPIO 25   |
| Power          | 3.3V / 5V |
| Ground         | GND       |

---

# 🌐 **MQTT Topic Mapping**

| Topic                      | Direction      | Description               |
| -------------------------- | -------------- | ------------------------- |
| `industrial/slave/data`    | Slave → Master | Sensor data (JSON)        |
| `industrial/slave/alert`   | Slave → Master | WARNING / CRITICAL alerts |
| `industrial/slave/status`  | Slave → Master | Relay state + uptime      |
| `industrial/slave/control` | Master → Slave | `RELAY_ON` / `RELAY_OFF`  |

These topics implement a **full-duplex command–data exchange** between nodes.

---

# 📱 **Dashboards & Interfaces**

## 1️⃣ **Blynk Cloud Dashboard**

Displayed:

* Temperature
* Humidity
* Gas level
* Relay state
* Alert level

User can **toggle relay** remotely through the dashboard.
Both Master and Slave perform updates.

---

## 2️⃣ **Web Dashboard (Master Node)**

Accessible using:

```
http://192.168.218.17/
```

Displays:

* Temperature
* Humidity
* Gas reading
* Relay ON/OFF state
* Alert level
* Time since last update

Includes a **relay toggle button**, synced with the Slave via MQTT.

---

## 3️⃣ **Telegram Alerting System**

Master sends:

* ⚠ **WARNING Alerts**
* 🚨 **CRITICAL Alerts**
* 🔌 “Relay forced OFF” messages
* Real-time emergency notifications

Alerts are triggered precisely based on **Slave’s threshold analysis**.

---

# 🧪 **Operational Workflow**

### 1️⃣ Slave Node:

* Reads environmental sensors every **10 seconds**
* Publishes a DATA JSON packet over MQTT
* Evaluates thresholds:

  * Gas > 300 → WARNING
  * Gas > 500 OR Temp > 40°C OR Humidity > 80% → CRITICAL
* If CRITICAL → relay is turned OFF locally
* Publishes:

  * ALERT packet (WARNING/CRITICAL)
  * STATUS packet (relay state, uptime)
* Updates Blynk virtual pins

### 2️⃣ Master Node:

* Receives DATA, ALERT, STATUS packets
* Updates:

  * Blynk dashboard widgets
  * Web dashboard HTML
* On WARNING / CRITICAL:

  * Sends Telegram alerts
  * Logs Blynk events
  * Sends relay control if necessary
* Provides relay control UI via:

  * Blynk App
  * Web dashboard

---

# 📂 **Project Structure**

```
/
├── master/
│   └── master_code.ino       # MQTT supervisor node
│
└── slave/
    └── slave_code.ino        # Sensor + safety + relay node

README.md                     # Documentation
```

---

# 📦 **Example JSON Packets**

### **Sensor Packet (DATA)**

```json
{
  "node": "slave_1",
  "type": "data",
  "temperature": 29.4,
  "humidity": 61.3,
  "gas": 185,
  "timestamp": 12345678
}
```

### **Alert Packet**

```json
{
  "node": "slave_1",
  "type": "alert",
  "level": "CRITICAL",
  "timestamp": 12345678
}
```

### **Status Packet**

```json
{
  "node": "slave_1",
  "type": "status",
  "relay": "OFF",
  "uptime": 320
}
```

---

# 🛡 **Safety Logic (as per code)**

The Slave ESP32 enforces emergency shutdown when:

| Condition          | Action                    |
| ------------------ | ------------------------- |
| Gas > 500          | Relay OFF, CRITICAL alert |
| Temperature > 40°C | Relay OFF, CRITICAL alert |
| Humidity > 80%     | Relay OFF, CRITICAL alert |
| Gas > 300          | WARNING alert             |

On CRITICAL → Master triggers:

* Telegram alert
* Blynk event
* Relay-off confirmation via MQTT

---

# 🧠 **Key Features**

### ✔ True two-way MQTT communication (Master ↔ Slave)

### ✔ JSON-based structured messaging

### ✔ Local emergency shutdown logic (Slave)

### ✔ Multi-interface user control: Web + Blynk

### ✔ Real-time dashboards with auto-refresh

### ✔ Remote Telegram alerting for industrial safety

### ✔ PubSubClient reconnection handling

### ✔ Retained STATUS messages

---

# 🚀 **Future Enhancements**

* Add multi-slave support
* TLS-secured MQTT
* OLED/LCD display on Slave
* InfluxDB + Grafana visualization
* AI-based environmental anomaly detection

---


Just say: **“Generate diagrams”** or **“Create report”**.
