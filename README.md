# ⚡ ESP32 Power Monitor - Real-Time Energy Monitoring System

[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-00979D?logo=espressif&logoColor=white)](https://www.espressif.com/)
[![Framework: Arduino](https://img.shields.io/badge/Framework-Arduino-00979D?logo=arduino&logoColor=white)](https://www.arduino.cc/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Language: English](https://img.shields.io/badge/Language-English-blue.svg)](README.md)
[![Language: Русский](https://img.shields.io/badge/Language-Русский-red.svg)](docs/README_RU.md)

**Professional-grade energy monitoring solution** that transforms your ESP32 into a powerful, standalone power monitoring station with beautiful web interface. Monitor any appliance's energy consumption in real-time with industrial accuracy.

---

## 🎥 See It in Action!

### Real-Time Web Interface Demo
![ESP32 Power Monitor Web Interface](images/web-interface-demo.gif)
*Live data updates every 2 seconds - power consumption, device states, and historical analytics*

### Hardware Setup
![ESP32 with Current Sensor](images/hardware-setup.jpg)
*Clean setup with SCT-013-000 sensor and ESP32 Dev Board*

---

## 🚀 Why This Project?

Tired of expensive energy monitors with limited features? This open-source solution gives you:

- **💰 Cost-effective** - Under $15 in components
- **🔧 Open-source** - Fully customizable to your needs
- **🌐 Self-hosted** - No cloud dependency, your data stays local
- **📊 Professional analytics** - True RMS calculations, state detection, efficiency metrics
- **⚡ Real-time updates** - Instant feedback on power consumption changes

---

## ✨ Key Features

| Feature | Description | Benefit |
|---------|-------------|---------|
| **🔍 True RMS Measurements** | Accurate power calculations using root mean square | Industrial-grade accuracy for any load type |
| **🤖 Smart State Detection** | Automatically detects 5 operating states | Understand appliance behavior patterns |
| **🌐 Built-in Web Server** | Responsive interface works on any device | No app installation required |
| **📊 Live Analytics** | Real-time charts, efficiency metrics, peak tracking | Make data-driven energy decisions |
| **🔧 Easy Calibration** | Simple calibration process with reference loads | Perfect accuracy for your specific setup |
| **⚙️ REST API** | Full API access for home automation integration | Works with Home Assistant, Node-RED, etc. |

---

## 🏗️ How It Works

```
Physical Layer → Processing Layer → Web Interface
     ↓                ↓                 ↓
Current Sensor → ESP32 (ADC + Logic) → Your Browser
```

1. **Current Sensing**: SCT-013-000 sensor clips around wire, measures magnetic field
2. **Signal Processing**: ESP32 reads analog signal, calculates True RMS values
3. **State Analysis**: Smart algorithms detect device operating states
4. **Web Serving**: Built-in server provides interface and API
5. **Real-time Updates**: Web interface auto-refreshes with live data

---

## 🛠️ Quick Start

### What You'll Need
- ✅ ESP32 Board ($5-10)
- ✅ SCT-013-000 Current Sensor ($5)
- ✅ 2 Resistors (10kΩ + 1kΩ) ($0.10)
- ✅ Breadboard & Jumper Wires ($2)
- ✅ USB Cable (you probably have one!)

### 5-Minute Setup

1. **Wire the sensor** (safe voltage divider circuit):
   ```
   Sensor → 10kΩ → 3.3V
          → 1kΩ → GND
          → GPIO34 (ADC)
   ```

2. **Flash the firmware** (choose one method):
   - **Arduino IDE**: Open `firmware/english_version/ESP32_Monitoring_system_en.ino`
   - **PlatformIO** (recommended): Open project in VS Code

3. **Configure WiFi** in the code:
   ```cpp
   const char* ssid = "Your_WiFi_Name";
   const char* password = "Your_WiFi_Password";
   ```

4. **Access the interface**:
   - ESP32 will show IP address in Serial Monitor
   - Open `http://[ESP32-IP]` in your browser
   - **That's it!** You're monitoring power consumption

---

## 💡 Perfect For...

### 🏠 Home Use
- Monitor refrigerator, washing machine, AC units
- Detect when appliances are left on unnecessarily
- Calculate energy costs of your devices

### 💻 IT & Office
- Server rack power monitoring
- Workstation energy usage
- Identify energy-efficient hardware

### 🔬 Education & DIY
- Learn about energy monitoring
- Electronics and IoT projects
- Data visualization and analytics

### 🏭 Industrial
- Machine power consumption tracking
- Preventive maintenance (detect abnormal usage)
- Energy efficiency optimization

---

## 🔧 Advanced Features

### Smart State Detection
The system automatically categorizes device behavior:
- **Off** (0W) - Device completely off
- **Standby** (1-10W) - Low-power sleep mode
- **Operation** (normal range) - Regular operation
- **Load** (high range) - Heavy usage mode
- **Overload** (critical) - Potential issue detection

### Data Analytics
- **Session tracking**: Energy per usage session
- **Efficiency metrics**: Power factor calculations
- **Peak detection**: Identify maximum power draws
- **Historical data**: Last 100 measurements with timestamps

### REST API Endpoints
Integrate with your smart home system:
```bash
GET /api/data          # JSON with all current data
GET /api/history       # Last 100 measurements
GET /api/statistics    # Session stats and totals
```

---

## 🎯 Accuracy & Calibration

**Out-of-the-box accuracy**: ±5%  
**After calibration**: ±2% or better

### Simple Calibration Process:
1. Connect a known load (e.g., 100W bulb)
2. Note the measured power in web interface
3. Calculate: `calibration_factor = 100 / measured_power`
4. Update factor in code and restart

---

## 🌍 Choose Your Language

- **[English Documentation](README.md)** (you are here)
- **[Русская документация](docs/README_RU.md)** - Полная версия на русском языке

Both versions contain complete setup instructions, troubleshooting, and technical details.

---

## 🤝 Contributing

**Love this project? Here's how you can help:**

1. **Report Bugs** - Found an issue? [Open a GitHub issue](issues)
2. **Suggest Features** - Have an idea? Let's discuss it!
3. **Improve Documentation** - Help make setup easier for others
4. **Share Your Setup** - Post pictures of your implementation

**Quick contribution guide:**
```bash
git clone https://github.com/yourusername/esp32-power-monitor.git
cd esp32-power-monitor
# Make your changes and submit a pull request!
```

---

## 📜 License

This project is **open-source** under the [MIT License](LICENSE) - meaning you can use it for personal or commercial projects, modify it, and distribute it. We only ask that you give credit where appropriate.

---

## ❓ Frequently Asked Questions

**Q: Is this safe to use with mains electricity?**  
A: The sensor is non-invasive and safe when properly installed. However, always exercise caution when working with electrical systems.

**Q: Do I need programming experience?**  
A: Basic Arduino knowledge helps, but step-by-step instructions are provided for beginners.

**Q: Can I monitor multiple devices?**  
A: Yes! You can deploy multiple ESP32 monitors or modify the code for multiple sensors.

**Q: What's the maximum current it can measure?**  
A: SCT-013-000 handles up to 100A, suitable for most household and industrial applications.

---

## 🔗 Useful Links

- [📖 Full Documentation](docs/) - Detailed technical documentation
- [🐛 Report Issues](issues) - Found a bug? Let us know!
- [💬 Discussions](discussions) - Get help and share ideas
- [⭐ Star this Repository] - Show your support!

---

**Ready to start monitoring?** Check out the [Russian version](docs/README_RU.md) for native language instructions or jump right into the [firmware directory](firmware/) to get started!

---
*This project is maintained by the open-source community. Your contributions are welcome!*
