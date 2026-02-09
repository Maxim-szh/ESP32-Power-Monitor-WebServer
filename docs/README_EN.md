# ⚡ ESP32 Power Monitor with Web Interface

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-00979D?logo=espressif&logoColor=white)](https://www.espressif.com/)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

A professional system for accurate real-time energy monitoring of any electrical appliances. Based on ESP32 microcontroller and SCT-013-000 current sensor, providing a convenient web interface for data visualization and analytics.

---

## 📸 Project Visualization

### 🖥️ Web Interface in Action
Below is a GIF demonstrating the web interface: real-time data updates, device state changes, and statistics.
<!--
Replace the link below with your actual GIF!
-->
![Web Interface Demo](images/web-interface-demo.gif)
*Web interface with real-time data: current, power, device state, and historical chart.*

### 🔌 Hardware Setup
This shows the connection of SCT-013-000 current sensor to ESP32 board (or Arduino with WiFi module).
<!--
Replace the link below with your actual photo!
-->
![Sensor to ESP32 Connection](images/hardware-setup.jpg)
*SCT-013-000 sensor connected to ESP32 via voltage divider.*

---

## 🌟 Key Features

- **🔍 Accurate Measurements:** True RMS current calculation for high precision.
- **🤖 Smart State Detection:** Automatic detection of 5 device states: `Off`, `Standby`, `Operation`, `Load`, `Overload`.
- **🌐 Intuitive Web Interface:** Responsive web page running on ESP32 itself. No cloud services required.
- **📊 Deep Analytics:** Efficiency calculation, peak power tracking, total operation time, and energy consumption per session.
- **📈 Measurement History:** Circular buffer storing last 100 measurements with timestamps.
- **🔧 Flexible Calibration:** Precise adjustment for specific sensors and loads.
- **⚙️ REST API:** Easy integration with other systems (Home Assistant, Node-RED, etc.) via API endpoints (`/api/data`).

---

## 🏗️ Project Architecture

The project implements the following multi-layer logic:

1.  **Measurement Layer:** ESP32 ADC reads voltage from current sensor, converted to safe range using voltage divider.
2.  **Processing Layer:** ESP32 program calculates RMS current value, multiplies by grid voltage (e.g., 220V) to get power.
3.  **Logic Layer:** Based on power consumption, determines current device state, maintains statistics and history.
4.  **Presentation Layer:** Built-in web server serves HTML page with data and provides API. Interface auto-updates every 2 seconds.

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│  Current Sensor │ →  │      ESP32       │ →  │   Web Server    │
│   SCT-013-000   │    │  (Processing +    │    │   & API (80)    │
│                 │    │     Logic)        │    │                 │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         ↓                        ↓                       ↓
   Analog Signal           Power Calculation,        Local Website
                          State Detection,           and JSON API
                             Statistics
```

---

## 💡 Use Cases

- **🏠 Smart Home:** Monitor energy consumption of refrigerator, washing machine, water heater.
- **💻 IT Infrastructure:** Control power usage of servers, workstations, network equipment.
- **🏭 Industry:** Monitor operation of machinery and equipment.
- **🔌 Energy Audit:** Analyze appliance efficiency and calculate operational costs.
- **🚨 Security:** Detect abnormal consumption or unauthorized device activation.

---

## 🚀 Getting Started

### Required Equipment

1.  **ESP32 Board** (any model: ESP32 DevKit C, ESP32-S3, etc.).
2.  **Current Sensor** SCT-013-000 (100A) or similar.
3.  **Two Resistors** for voltage divider (e.g., 10kΩ and 1kΩ).
4.  **Breadboard** and jumper wires.
5.  **USB Cable** for programming and powering ESP32.
6.  Computer with **Arduino IDE** or **VS Code with PlatformIO**.

### Hardware Assembly

1.  Build the voltage divider circuit as shown in the diagram below. This is necessary to reduce sensor voltage to ESP32 ADC safe range (0-3.3V).

    **Connection Diagram:**
    ```
    SCT-013-000 Output ────┬───────── 10kΩ Resistor ──── 3.3V (ESP32)
                           │
                          ──── 1kΩ Resistor ──── GND (ESP32)
                           │
                           └───────── GPIO34 (ADC Input, ESP32)
    ```

2.  Clip the SCT-013-000 sensor onto the phase wire of the device you want to monitor. **WARNING: Working with mains voltage is dangerous! If unsure, consult an electrician.**

### Software Setup and Flashing

#### Option 1: Arduino IDE

1.  **Install ESP32 support in Arduino IDE:**
    - Open *File -> Preferences*.
    - In "Additional Boards Manager URLs" add: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
    - Go to *Tools -> Board -> Boards Manager*, search for `esp32` and install.

2.  **Prepare the code:**
    - Download or clone this repository.
    - Open the file `firmware/english_version/ESP32_Monitoring_system_en.ino` in Arduino IDE.

3.  **Configure parameters:**
    - Find the configuration section at the beginning of the code.
    - Replace `"your_SSID"` and `"your_PASSWORD"` with your Wi-Fi credentials.
    - Adjust ADC pin number (default `34`) and calibration factor if needed.

4.  **Upload the code:**
    - Select *ESP32 Dev Module* in *Tools -> Board*.
    - Choose the correct COM port.
    - Click **Upload**.

#### Option 2: PlatformIO (VS Code) - Recommended

1.  **Install VS Code and PlatformIO extension.**
2.  **Open the project folder in VS Code.**
3.  PlatformIO will automatically recognize the project. Configure Wi-Fi in the firmware file.
4.  Click the **Upload** button in PlatformIO taskbar.

### Final Setup and Usage

1.  After successful upload, open **Serial Monitor** at 115200 baud rate.
2.  ESP32 will connect to Wi-Fi and display its IP address, e.g.: `Web server started on IP: 192.168.1.100`.
3.  Open this IP address in a browser on any device in the same network.
4.  You'll see the web interface with data. Turn the monitored device on/off to verify readings change.

---

## 🔧 Calibration

For accurate readings, calibration is necessary. The simplest method:

1.  Connect a reference load with known power consumption (e.g., 100W incandescent bulb).
2.  Check the raw power value in web interface or serial monitor.
3.  Calculate calibration factor: `Factor = (Reference Power) / (Measured Power)`.
4.  Update the `calibration_factor` variable in code and restart ESP32.

---

## 🤝 Contributing

Contributions are welcome! If you have improvement ideas:
1.  Create an [Issue](issues) for discussion.
2.  Fork the repository.
3.  Create a feature branch (`git checkout -b feature/AmazingFeature`).
4.  Commit your changes (`git commit -m 'Add some AmazingFeature'`).
5.  Push the branch (`git push origin feature/AmazingFeature`).
6.  Create a Pull Request.

---

## 📄 License

This project is distributed under MIT License. See [LICENSE](LICENSE) for details.

---

## 🌐 Language Versions
- [Russian Documentation](docs/README_RU.md)
- [Russian Firmware](firmware/russian_version/ESP32_Monitoring_system_ru.ino)
