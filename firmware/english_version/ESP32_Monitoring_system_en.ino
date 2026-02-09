#include <WiFi.h>
#include <WebServer.h>
#include <time.h>  

// Function declarations
void handleRoot();
void handleApiData();
void handleApiHistory();
void handleApiLogs(); 
void handleLogs();

// Wi-Fi settings - CHANGE THESE FOR YOUR NETWORK
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Web server setup
WebServer server(80);

// SCT-013-000 sensor settings (without EmonLib, using analogRead)
#define CURRENT_INPUT YOUR_ADC_PIN  // ADC pin for ESP32-C3 
const float SENSITIVITY = 0.055;   // Sensitivity factor for SCT-013-000
const int SAMPLES = 1480;           // Number of samples for RMS calculation
const float MIDPOINT = 2048.0;      // ADC midpoint (4096/2)
const float VREF = 3.3;             // Reference voltage
const float fixedVoltage = 220.0;   // Fixed grid voltage

#define WATCH_CALIBRATION true     // Enable calibration monitoring

// Power threshold settings
float offThreshold = 5.0;           // Below this power = off
float lowPower = 50.0, highLow = 500.0;     // Idle range
float mediumPower = 500.0, highMedium = 3000.0;  // Normal work range
float highPower = 3000.0;           // High load threshold

// State tracking variables
String state = "idle";
unsigned long totalTime = 0;
unsigned long idleTime = 0;
unsigned long workTime = 0;
unsigned long intenseTime = 0;
unsigned long overTime = 0;
float dailyPeakPower = 0.0;
float efficiency = 0.0;
float currentIrms = 0.0;
float currentRealPower = 0.0;

// Session tracking
time_t sessionStart = 0;
float totalEnergyWh = 0.0;
float avgPower = 0.0;
unsigned long totalActiveTime = 0;
String currentStatus = "Off";
bool wasActive = false;

// Logging system
struct LogEntry {
  time_t timestamp;
  float power;
  String state;
};
#define LOG_SIZE 100
LogEntry logs[LOG_SIZE];
int logIndex = 0;

// Timing variables
time_t startTime = 0;  // Real time for daily reset
unsigned long lastMeasurement = 0;
unsigned long lastLog = 0;
const unsigned long interval = 1000;    // Measurement interval (ms)
const unsigned long logInterval = 5000; // Logging interval (ms)

// State definitions
const String states[5] = {"Off", "Idle", "Normal work", "Intense work", "Overload"};

/**
 * Format time in seconds to readable string
 * @param seconds time in seconds
 * @return formatted time string
 */
String formatTime(unsigned long seconds) {
  unsigned long days = seconds / 86400;
  unsigned long hours = (seconds % 86400) / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  unsigned long secs = seconds % 60;

  String result = "";
  if (days > 0) result += String(days) + " days ";
  if (hours > 0 || days > 0) result += String(hours) + " h ";
  if (minutes > 0 || hours > 0 || days > 0) result += String(minutes) + " min ";
  result += String(secs) + " sec";

  result.trim(); 
  return result;
}

/**
 * Calculate RMS current from sensor readings
 * @param samples number of samples to take
 * @return RMS current in Amperes
 */
float calcIrms(unsigned int samples) {
  float sumI = 0;
  long start = millis();
  while ((millis() - start) < (samples * (1000 / 50))) {  // Approximately samples cycles at 50Hz
    int sensorValue = analogRead(CURRENT_INPUT);
    float rawVoltage = (sensorValue - MIDPOINT) * (VREF / 4095.0);  // Centered signal
    sumI += rawVoltage * rawVoltage;
    delayMicroseconds(20);  // ~50Hz sampling
  }
  float Iratio = sqrt(sumI / samples);  // RMS calculation
  return Iratio / SENSITIVITY;  // Current in Amperes
}

bool ntpSynced = false;

void setup() {
  Serial.begin(115200);
  pinMode(CURRENT_INPUT, INPUT);  // Set ADC pin as input
  currentIrms = 0.0;
  currentRealPower = 0.0;

  Serial.println("ESP32-C3 Power Monitor (SCT-013-000, no EmonLib) starting...");

  if (WATCH_CALIBRATION) {
    Serial.println("CALIBRATION WATCH: ON. Adjust SENSITIVITY based on known load.");
  }

  // Wi-Fi connection
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(100);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("Wi-Fi connected! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("Wi-Fi connection failed!");
    return;
  }

  // NTP synchronization (UTC+3)
  configTime(3 * 3600, 0, "pool.ntp.org");
  Serial.println("NTP config done. Waiting for sync...");
  delay(5000);  // Wait for synchronization
  time_t now;
  time(&now);
  Serial.print("Current time (UTC+3): ");
  Serial.println(ctime(&now));
  startTime = now;  // Start of day

  // Web server routes
  server.on("/", handleRoot);
  server.on("/api/data", handleApiData);
  server.on("/api/history", handleApiHistory);
  server.on("/api/logs", handleApiLogs);
  server.on("/logs", handleLogs);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  if (millis() - lastMeasurement >= interval) {
    lastMeasurement = millis();
    totalTime++;

    // Current measurement (replacement for emon1.calcIrms(1480))
    currentIrms = calcIrms(SAMPLES);
    if (currentIrms < 0.01) currentIrms = 0.0;

    // Power calculation
    currentRealPower = fixedVoltage * currentIrms;
    if (currentRealPower < offThreshold) currentRealPower = 0.0;

    // Calibration monitoring
    if (WATCH_CALIBRATION) {
      Serial.print("Current: "); Serial.print(currentIrms, 3); Serial.print("A, Power: "); Serial.print(currentRealPower, 1); Serial.println("W");
    }

    // State determination
    String prevState = currentStatus;
    int stateIdx = 0;
    if (currentRealPower < offThreshold) {
      currentStatus = states[0]; stateIdx = 0; idleTime++;
    } else if (currentRealPower >= lowPower && currentRealPower < highLow) {
      currentStatus = states[1]; stateIdx = 1; workTime++;
    } else if (currentRealPower >= mediumPower && currentRealPower < highMedium) {
      currentStatus = states[2]; stateIdx = 2; intenseTime++;
    } else if (currentRealPower >= highPower && currentRealPower < 6000.0) {
      currentStatus = states[3]; stateIdx = 3; overTime++;
    } else if (currentRealPower >= 6000.0) {
      currentStatus = states[4]; stateIdx = 4; overTime++;
    } else {
      currentStatus = states[0]; stateIdx = 0; idleTime++;
    }
    state = currentStatus;
    if (currentStatus != prevState) {
      Serial.println("State: " + currentStatus);
    }

    // Peak power tracking
    if (currentRealPower > dailyPeakPower) {
      dailyPeakPower = currentRealPower;
    }

    // Session tracking
    bool isActive = (currentRealPower > offThreshold);
    time_t now;
    time(&now);
    if (isActive && !wasActive) {
      sessionStart = now;
      totalEnergyWh = 0.0;
      avgPower = 0.0;
      totalActiveTime = 0;
    }
    wasActive = isActive;

    // Energy calculation
    if (isActive) {
      float energyInterval = currentRealPower * (interval / 3600000.0);  // Wh
      totalEnergyWh += energyInterval;
      avgPower = (avgPower * totalActiveTime + currentRealPower) / (totalActiveTime + 1);
      totalActiveTime += interval / 1000;
    }

    // Efficiency calculation
    if (totalTime > 0) {
      efficiency = (float)(workTime + intenseTime + overTime) / totalTime * 100.0;
    }

    // Logging
    if (millis() - lastLog >= logInterval) {
      lastLog = millis();
      logs[logIndex].timestamp = now;
      logs[logIndex].power = currentRealPower;
      logs[logIndex].state = currentStatus;
      logIndex = (logIndex + 1) % LOG_SIZE;
    }

    // Daily reset
    if (now - startTime >= 86400) {
      idleTime = workTime = intenseTime = overTime = 0;
      dailyPeakPower = 0.0; totalTime = 0; efficiency = 0.0;
      totalEnergyWh = 0.0; totalActiveTime = 0; avgPower = 0.0; logIndex = 0;
      startTime = now;
      Serial.println("Daily reset performed.");
    }
  }
}

// Web interface handler
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Power Consumption Monitor</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
    <link href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.10.0/font/bootstrap-icons.css" rel="stylesheet">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { background-color: #f8f9fa; color: #212529; }
        .navbar { background-color: #343a40; color: #ffffff; border-bottom: 1px solid #dee2e6; }
        .navbar-brand, .navbar-text { color: #ffffff !important; }
        .card { background-color: #ffffff; border: 1px solid #dee2e6; box-shadow: 0 0.125rem 0.25rem rgba(0, 0, 0, 0.075); }
        .card-header { background-color: #f8f9fa; color: #212529; border-bottom: 1px solid #dee2e6; }
        .btn-secondary { background-color: #6c757d; border-color: #6c757d; }
        .btn-secondary:hover { background-color: #5a6268; border-color: #545b62; }
        .chart-container { position: relative; height: 300px; }
        #state { font-size: 1.5rem; font-weight: bold; }
        .status-off { color: #6c757d; }
        .status-idle { color: #28a745; }
        .status-normalwork { color: #ffc107; }
        .status-intensework { color: #fd7e14; }
        .status-overload { color: #dc3545; }
    </style>
</head>
<body>
    <nav class="navbar navbar-light">
        <div class="container-fluid">
            <span class="navbar-brand mb-0 h1">Power Consumption Monitor</span>
            <span class="navbar-text"></span>
        </div>
    </nav>
    <div class="container mt-4">
        <div class="row">
            <div class="col-md-3">
                <div class="card h-100 text-center">
                    <div class="card-body">
                        <i class="bi bi-lightning-charge-fill fs-1 text-warning"></i>
                        <h5 class="card-title">Status</h5>
                        <p class="card-text" id="state">Off</p>
                    </div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="card h-100 text-center">
                    <div class="card-body">
                        <i class="bi bi-speedometer2 fs-1 text-primary"></i>
                        <h5 class="card-title">Power</h5>
                        <p class="card-text fs-3" id="power">0 W</p>
                    </div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="card h-100 text-center">
                    <div class="card-body">
                        <i class="bi bi-battery-charging fs-1 text-info"></i>
                        <h5 class="card-title">Current</h5>
                        <p class="card-text fs-3" id="current">0 A</p>
                    </div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="card h-100 text-center">
                    <div class="card-body">
                        <i class="bi bi-graph-up fs-1 text-success"></i>
                        <h5 class="card-title">Efficiency</h5>
                        <p class="card-text fs-3" id="efficiency">0 %</p>
                    </div>
                </div>
            </div>
        </div>

        <div class="row mt-4">
            <div class="col-md-7">
                <div class="card">
                    <div class="card-header">Power History</div>
                    <div class="card-body">
                        <div class="chart-container">
                            <canvas id="powerChart"></canvas>
                        </div>
                    </div>
                </div>
            </div>
            <div class="col-md-5">
                <div class="card">
                    <div class="card-header">Status Distribution</div>
                    <div class="card-body">
                        <div class="chart-container">
                            <canvas id="stateChart"></canvas>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <div class="row mt-4">
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header">Session</div>
                    <div class="card-body">
                        <p><strong>Working time:</strong> <span id="sessionTime">0 sec</span></p>
                        <p><strong>Average power:</strong> <span id="avgPower">0 W</span></p>
                        <p><strong>Energy:</strong> <span id="totalEnergy">0 Wh</span></p>
                    </div>
                </div>
            </div>
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header">Statistics</div>
                    <div class="card-body">
                        <p><strong>Peak power:</strong> <span id="peakPower">0 W</span></p>
                        <ul class="list-unstyled">
                            <li>Off: <span id="idle">0 sec</span></li>
                            <li>Idle: <span id="work">0 sec</span></li>
                            <li>Normal work: <span id="intense">0 sec</span></li>
                            <li>Intense work: <span id="over">0 sec</span></li>
                            <li>Overload: <span id="overHigh">0 sec</span></li>
                        </ul>
                    </div>
                </div>
            </div>
        </div>

        <div class="text-center mt-3">
            <a href="/logs" class="btn btn-secondary me-2"><i class="bi bi-journal-text"></i> Logs</a>
        </div>
    </div>

    <script>
        let powerChart, stateChart;
        const stateColors = ['#6c757d', '#28a745', '#ffc107', '#fd7e14', '#dc3545'];

        function formatLocalTime(ts) {
            return new Date(ts * 1000).toLocaleString('en-US', {hour: '2-digit', minute: '2-digit'});
        }

        function formatLocalDateTime(ts) {
            return new Date(ts * 1000).toLocaleString('en-US');
        }

        function formatTime(seconds) {
            let days = Math.floor(seconds / 86400);
            let hours = Math.floor((seconds % 86400) / 3600);
            let minutes = Math.floor((seconds % 3600) / 60);
            let secs = seconds % 60;

            let result = '';
            if (days > 0) result += days + ' days ';
            if (hours > 0 || days > 0) result += hours + ' h ';
            if (minutes > 0 || hours > 0 || days > 0) result += minutes + ' min ';
            result += secs + ' sec';
            return result.trim();
        }

        function initCharts() {
            const ctxPower = document.getElementById('powerChart').getContext('2d');
            powerChart = new Chart(ctxPower, {
                type: 'line',
                data: { labels: [], datasets: [{ label: 'Power (W)', data: [], borderColor: '#0d6efd', backgroundColor: 'rgba(13,110,253,0.1)', tension: 0.1 }] },
                options: { responsive: true, maintainAspectRatio: false, scales: { y: { beginAtZero: true } } }
            });

            const ctxState = document.getElementById('stateChart').getContext('2d');
            stateChart = new Chart(ctxState, {
                type: 'doughnut',
                data: { labels: ['Off', 'Idle', 'Normal work', 'Intense work', 'Overload'], datasets: [{ data: [0,0,0,0,0], backgroundColor: stateColors }] },
                options: { responsive: true, maintainAspectRatio: false }
            });
        }

        function updateData() {
            fetch('/api/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('state').textContent = data.state;
                    let stateClass = data.state.toLowerCase().replace(/ /g, '');
                    document.getElementById('state').className = 'status-' + stateClass;
                    document.getElementById('power').textContent = Math.round(data.power) + ' W';
                    document.getElementById('current').textContent = data.current.toFixed(2) + ' A';
                    document.getElementById('efficiency').textContent = data.efficiency.toFixed(1) + ' %';
                    document.getElementById('avgPower').textContent = Math.round(data.avgPower) + ' W';
                    document.getElementById('totalEnergy').textContent = data.totalEnergy.toFixed(2) + ' Wh';
                    document.getElementById('peakPower').textContent = Math.round(data.peak) + ' W';
                    document.getElementById('idle').textContent = formatTime(data.idle);
                    document.getElementById('work').textContent = formatTime(data.work);
                    document.getElementById('intense').textContent = formatTime(data.intense);
                    document.getElementById('over').textContent = formatTime(data.over);
                    document.getElementById('overHigh').textContent = formatTime(data.overHigh);
                    document.getElementById('sessionTime').textContent = formatTime(data.totalActiveTime);

                    stateChart.data.datasets[0].data = [data.idle, data.work, data.intense, data.over, data.overHigh];
                    stateChart.update();
                });

            fetch('/api/history')
                .then(response => response.json())
                .then(history => {
                    const labels = history.map(h => formatLocalTime(h.timestamp));
                    const powers = history.map(h => h.power);
                    powerChart.data.labels = labels.reverse();
                    powerChart.data.datasets[0].data = powers.reverse();
                    powerChart.update();
                });
        }

        initCharts();
        updateData();
        setInterval(updateData, 200);
    </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

// API endpoint for current data
void handleApiData() {
  String json = "{";
  json += "\"state\":\"" + state + "\",";
  json += "\"current\":" + String(currentIrms, 3) + ",";
  json += "\"power\":" + String(currentRealPower, 1) + ",";
  json += "\"peak\":" + String(dailyPeakPower, 1) + ",";
  json += "\"efficiency\":" + String(efficiency, 1) + ",";
  json += "\"idle\":" + String(idleTime) + ",";
  json += "\"work\":" + String(workTime) + ",";
  json += "\"intense\":" + String(intenseTime) + ",";
  json += "\"over\":" + String(overTime) + ",";
  json += "\"overHigh\":" + String(overTime) + ",";
  json += "\"avgPower\":" + String(avgPower, 1) + ",";
  json += "\"totalEnergy\":" + String(totalEnergyWh, 2) + ",";
  json += "\"totalActiveTime\":" + String(totalActiveTime);
  json += "}";
  server.send(200, "application/json", json);
}

// API endpoint for historical data
void handleApiHistory() {
  String json = "[";
  int count = 0;
  for (int i = 1; i <= LOG_SIZE; i++) {
    int idx = (logIndex - i + LOG_SIZE) % LOG_SIZE;
    if (logs[idx].timestamp > 0) {
      if (count > 0) json += ",";
      json += "{\"timestamp\":" + String(logs[idx].timestamp) + ",";
      json += "\"power\":" + String(logs[idx].power, 1) + ",";
      json += "\"state\":\"" + logs[idx].state + "\"}";
      count++;
    }
  }
  json += "]";
  server.send(200, "application/json", json);
}

// API endpoint for logs
void handleApiLogs() {
  String json = "[";
  int count = 0;
  for (int i = 1; i <= LOG_SIZE; i++) {
    int idx = (logIndex - i + LOG_SIZE) % LOG_SIZE;
    if (logs[idx].timestamp > 0) {
      if (count > 0) json += ",";
      json += "{\"timestamp\":" + String(logs[idx].timestamp) + ",";
      json += "\"power\":" + String(logs[idx].power, 1) + ",";
      json += "\"state\":\"" + logs[idx].state + "\"}";
      count++;
    }
  }
  json += "]";
  server.send(200, "application/json", json);
}

// Logs page handler
void handleLogs() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Logs</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
    <style>
        body { background-color: #f8f9fa; color: #212529; }
        .table { color: #212529; background-color: #ffffff; border: 1px solid #dee2e6; }
        .table-striped > tbody > tr:nth-of-type(odd) > td { background-color: rgba(0, 0, 0, 0.05); }
        .card { background-color: #ffffff; border: 1px solid #dee2e6; box-shadow: 0 0.125rem 0.25rem rgba(0, 0, 0, 0.075); }
    </style>
</head>
<body>
    <div class="container mt-4">
        <h1 class="text-center">Logs</h1>
        <div class="card">
            <div class="card-body">
                <div class="table-responsive">
                    <table class="table table-striped" id="logsTable">
                        <thead class="table-dark">
                            <tr><th>Time</th><th>Power</th><th>Status</th></tr>
                        </thead>
                        <tbody id="logsBody"></tbody>
                    </table>
                </div>
                <a href="/" class="btn btn-secondary">Back</a>
            </div>
        </div>
    </div>
    <script>
        function formatLocalDateTime(ts) {
            return new Date(ts * 1000).toLocaleString('en-US');
        }

        fetch('/api/logs')
            .then(response => response.json())
            .then(logs => {
                const tbody = document.getElementById('logsBody');
                logs.reverse().forEach(log => {
                    const row = tbody.insertRow();
                    row.innerHTML = `<td>${formatLocalDateTime(log.timestamp)}</td><td>${log.power} W</td><td>${log.state}</td>`;
                });
            });
    </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}