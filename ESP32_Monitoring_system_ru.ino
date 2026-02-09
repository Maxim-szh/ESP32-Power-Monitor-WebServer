#include <WiFi.h>
#include <WebServer.h>
#include <time.h>  

// Прототипы функций
void handleRoot();
void handleApiData();
void handleApiHistory();
void handleApiLogs(); 
void handleLogs();

// НАСТРОЙКИ: ИЗМЕНИТЕ ДЛЯ СВОЕЙ СЕТИ
const char* ssid = "Login";           // SSID вашей Wi-Fi сети
const char* password = "Password";    // Пароль от Wi-Fi

WebServer server(80);

// НАСТРОЙКИ ДАТЧИКА SCT-013-000
#define CURRENT_INPUT A0              // Укажите пин ADC для вашей платы ESP32
const float SENSITIVITY = 0.055;     // Коэффициент чувствительности (калибровка)
const int SAMPLES = 1480;             // Количество измерений для RMS расчета
const float MIDPOINT = 2048.0;       // Середина ADC (4096/2)
const float VREF = 3.3;               // Опорное напряжение
const float fixedVoltage = 220.0;     // Сетевое напряжение

#define WATCH_CALIBRATION true        // Включить вывод данных для калибровки

// ПОРОГИ МОЩНОСТИ ДЛЯ ОПРЕДЕЛЕНИЯ СОСТОЯНИЙ
float offThreshold = 5.0;             // Ниже этого значения - устройство выключено
float lowPower = 50.0, highLow = 500.0;      // Диапазон простоя
float mediumPower = 500.0, highMedium = 3000.0; // Диапазон нормальной работы
float highPower = 3000.0;              // Порог высокой нагрузки

// ПЕРЕМЕННЫЕ ДЛЯ СТАТИСТИКИ
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

// ОТСЛЕЖИВАНИЕ СЕССИИ РАБОТЫ
time_t sessionStart = 0;
float totalEnergyWh = 0.0;
float avgPower = 0.0;
unsigned long totalActiveTime = 0;
String currentStatus = "Выключен";
bool wasActive = false;

// СИСТЕМА ЛОГИРОВАНИЯ
struct LogEntry {
  time_t timestamp;
  float power;
  String state;
};
#define LOG_SIZE 100
LogEntry logs[LOG_SIZE];
int logIndex = 0;

// ПЕРЕМЕННЫЕ ВРЕМЕНИ
time_t startTime = 0;
unsigned long lastMeasurement = 0;
unsigned long lastLog = 0;
const unsigned long interval = 1000;      // Интервал измерений (мс)
const unsigned long logInterval = 5000;   // Интервал логирования (мс)

// ОПРЕДЕЛЕНИЯ СОСТОЯНИЙ УСТРОЙСТВА
const String states[5] = {"Выключен", "Простой", "Работает нормально", "Работает интенсивно", "Работает при повышенной нагрузке"};

/**
 * Форматирование времени в читаемый вид
 */
String formatTime(unsigned long seconds) {
  unsigned long days = seconds / 86400;
  unsigned long hours = (seconds % 86400) / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  unsigned long secs = seconds % 60;

  String result = "";
  if (days > 0) result += String(days) + " д ";
  if (hours > 0 || days > 0) result += String(hours) + " ч ";
  if (minutes > 0 || hours > 0 || days > 0) result += String(minutes) + " мин ";
  result += String(secs) + " сек";

  result.trim(); 
  return result;
}

/**
 * Расчет RMS тока из показаний датчика
 */
float calcIrms(unsigned int samples) {
  float sumI = 0;
  long start = millis();
  while ((millis() - start) < (samples * (1000 / 50))) {  // Примерно samples циклов при 50Hz
    int sensorValue = analogRead(CURRENT_INPUT);
    float rawVoltage = (sensorValue - MIDPOINT) * (VREF / 4095.0);  // Отцентрированный сигнал
    sumI += rawVoltage * rawVoltage;
    delayMicroseconds(20);  // ~50Hz sampling
  }
  float Iratio = sqrt(sumI / samples);  // Среднеквадратичное
  return Iratio / SENSITIVITY;  // Ток в Амперах
}

bool ntpSynced = false;

/**
 * ИНИЦИАЛИЗАЦИЯ СИСТЕМЫ
 */
void setup() {
  Serial.begin(115200);
  pinMode(CURRENT_INPUT, INPUT);  // Устанавливаем ADC-пин как вход
  currentIrms = 0.0;
  currentRealPower = 0.0;

  Serial.println("ESP32-C3 Power Monitor (SCT-013-000, no EmonLib) starting...");

  if (WATCH_CALIBRATION) {
    Serial.println("CALIBRATION WATCH: ON. Adjust SENSITIVITY based on known load.");
  }

  // ПОДКЛЮЧЕНИЕ К WI-FI
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

  // СИНХРОНИЗАЦИЯ ВРЕМЕНИ ЧЕРЕЗ NTP (Московское время UTC+3)
  configTime(3 * 3600, 0, "pool.ntp.org");
  Serial.println("NTP config done. Waiting for sync...");
  delay(5000);  // Ждём синхронизации
  time_t now;
  time(&now);
  Serial.print("Current time (UTC+3): ");
  Serial.println(ctime(&now));
  startTime = now;  // Начало дня

  // НАСТРОЙКА WEB-СЕРВЕРА И API ЭНДПОИНТОВ
  server.on("/", handleRoot);
  server.on("/api/data", handleApiData);
  server.on("/api/history", handleApiHistory);
  server.on("/api/logs", handleApiLogs);
  server.on("/logs", handleLogs);
  server.begin();
  Serial.println("HTTP server started");
}

/**
 * ОСНОВНОЙ ЦИКЛ РАБОТЫ
 */
void loop() {
  server.handleClient();  // Обработка HTTP запросов

  if (millis() - lastMeasurement >= interval) {
    lastMeasurement = millis();
    totalTime++;

    // ОСНОВНОЕ ИЗМЕРЕНИЕ ТОКА И МОЩНОСТИ
    currentIrms = calcIrms(SAMPLES);
    if (currentIrms < 0.01) currentIrms = 0.0;

    currentRealPower = fixedVoltage * currentIrms;
    if (currentRealPower < offThreshold) currentRealPower = 0.0;

    // ВЫВОД ДЛЯ КАЛИБРОВКИ
    if (WATCH_CALIBRATION) {
      Serial.print("Current: "); Serial.print(currentIrms, 3); Serial.print("A, Power: "); Serial.print(currentRealPower, 1); Serial.println("W");
    }

    // ОПРЕДЕЛЕНИЕ ТЕКУЩЕГО СОСТОЯНИЯ УСТРОЙСТВА
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

    // ОБНОВЛЕНИЕ ПИКОВОЙ МОЩНОСТИ
    if (currentRealPower > dailyPeakPower) {
      dailyPeakPower = currentRealPower;
    }

    // ОТСЛЕЖИВАНИЕ СЕССИИ РАБОТЫ И РАСЧЕТ ЭНЕРГИИ
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

    if (isActive) {
      float energyInterval = currentRealPower * (interval / 3600000.0);  // Wh
      totalEnergyWh += energyInterval;
      avgPower = (avgPower * totalActiveTime + currentRealPower) / (totalActiveTime + 1);
      totalActiveTime += interval / 1000;
    }

    // РАСЧЕТ КПД СИСТЕМЫ
    if (totalTime > 0) {
      efficiency = (float)(workTime + intenseTime + overTime) / totalTime * 100.0;
    }

    // ЛОГИРОВАНИЕ ДАННЫХ
    if (millis() - lastLog >= logInterval) {
      lastLog = millis();
      logs[logIndex].timestamp = now;
      logs[logIndex].power = currentRealPower;
      logs[logIndex].state = currentStatus;
      logIndex = (logIndex + 1) % LOG_SIZE;
    }

    // ЕЖЕДНЕВНЫЙ СБРОС СТАТИСТИКИ
    if (now - startTime >= 86400) {
      idleTime = workTime = intenseTime = overTime = 0;
      dailyPeakPower = 0.0; totalTime = 0; efficiency = 0.0;
      totalEnergyWh = 0.0; totalActiveTime = 0; avgPower = 0.0; logIndex = 0;
      startTime = now;
      Serial.println("Daily reset performed.");
    }
  }
}

/**
 * ВЕБ-ИНТЕРФЕЙС - ГЛАВНАЯ СТРАНИЦА
 */
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Монитор мощности ESP32</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        .card { background: #f8f9fa; border-left: 4px solid #007bff; padding: 15px; margin: 10px 0; border-radius: 5px; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; }
        .value { font-size: 1.2em; font-weight: bold; color: #007bff; }
        .state { padding: 5px 10px; border-radius: 15px; color: white; font-weight: bold; }
        .state-off { background: #6c757d; }
        .state-idle { background: #28a745; }
        .state-normal { background: #007bff; }
        .state-intense { background: #ffc107; color: black; }
        .state-over { background: #dc3545; }
        .chart-container { height: 300px; margin: 20px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📊 Монитор мощности устройства</h1>
        
        <div class="grid">
            <div class="card">
                <h3>⚡ Текущая мощность</h3>
                <div class="value" id="power">0 Вт</div>
                <div id="currentState" class="state state-off">Выключен</div>
            </div>
            
            <div class="card">
                <h3>🔌 Ток</h3>
                <div class="value" id="current">0 A</div>
            </div>
            
            <div class="card">
                <h3>📈 Пиковая мощность сегодня</h3>
                <div class="value" id="peakPower">0 Вт</div>
            </div>
            
            <div class="card">
                <h3>⏱️ Статистика работы</h3>
                <div>Активно: <span id="activeTime">0 сек</span></div>
                <div>Простой: <span id="idleTime">0 сек</span></div>
                <div>КПД: <span id="efficiency">0%</span></div>
            </div>
        </div>

        <div class="card">
            <h3>🔋 Сессия работы</h3>
            <div>Энергия: <span id="energy">0 Вт·ч</span></div>
            <div>Средняя мощность: <span id="avgPower">0 Вт</span></div>
            <div>Длительность: <span id="sessionTime">0 сек</span></div>
        </div>

        <div class="card">
            <h3>📊 История состояний</h3>
            <div id="history"></div>
        </div>

        <div style="margin-top: 20px;">
            <a href="/logs" style="background: #007bff; color: white; padding: 10px 15px; text-decoration: none; border-radius: 5px;">📋 Посмотреть логи</a>
            <button onclick="location.reload()" style="background: #28a745; color: white; padding: 10px 15px; border: none; border-radius: 5px;">🔄 Обновить</button>
        </div>
    </div>

    <script>
        function updateData() {
            fetch('/api/data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('power').textContent = data.power + ' Вт';
                    document.getElementById('current').textContent = data.current + ' A';
                    document.getElementById('peakPower').textContent = data.peakPower + ' Вт';
                    document.getElementById('activeTime').textContent = data.activeTime;
                    document.getElementById('idleTime').textContent = data.idleTime;
                    document.getElementById('efficiency').textContent = data.efficiency + '%';
                    document.getElementById('energy').textContent = data.energy + ' Вт·ч';
                    document.getElementById('avgPower').textContent = data.avgPower + ' Вт';
                    document.getElementById('sessionTime').textContent = data.sessionTime;
                    
                    // Обновление состояния
                    const stateElem = document.getElementById('currentState');
                    stateElem.textContent = data.state;
                    stateElem.className = 'state state-' + data.stateClass;
                });
            
            fetch('/api/history')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('history').innerHTML = data.history;
                });
        }
        
        setInterval(updateData, 2000);
        updateData();
    </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

/**
 * API ДЛЯ ПОЛУЧЕНИЯ ТЕКУЩИХ ДАННЫХ
 */
void handleApiData() {
  String stateClass = "off";
  if (currentStatus == states[1]) stateClass = "idle";
  else if (currentStatus == states[2]) stateClass = "normal";
  else if (currentStatus == states[3]) stateClass = "intense";
  else if (currentStatus == states[4]) stateClass = "over";

  String json = "{";
  json += "\"power\":" + String(currentRealPower, 1) + ",";
  json += "\"current\":" + String(currentIrms, 3) + ",";
  json += "\"peakPower\":" + String(dailyPeakPower, 1) + ",";
  json += "\"activeTime\":\"" + formatTime(workTime + intenseTime + overTime) + "\",";
  json += "\"idleTime\":\"" + formatTime(idleTime) + "\",";
  json += "\"efficiency\":" + String(efficiency, 1) + ",";
  json += "\"energy\":" + String(totalEnergyWh, 2) + ",";
  json += "\"avgPower\":" + String(avgPower, 1) + ",";
  json += "\"sessionTime\":\"" + formatTime(totalActiveTime) + "\",";
  json += "\"state\":\"" + currentStatus + "\",";
  json += "\"stateClass\":\"" + stateClass + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

/**
 * API ДЛЯ ПОЛУЧЕНИЯ ИСТОРИИ СОСТОЯНИЙ
 */
void handleApiHistory() {
  String history = "<table style='width:100%; border-collapse: collapse;'>";
  history += "<tr><th style='border:1px solid #ddd; padding:8px;'>Время</th><th style='border:1px solid #ddd; padding:8px;'>Состояние</th><th style='border:1px solid #ddd; padding:8px;'>Мощность</th></tr>";
  
  for (int i = 0; i < 10; i++) {
    int idx = (logIndex - 1 - i + LOG_SIZE) % LOG_SIZE;
    if (logs[idx].timestamp == 0) continue;
    
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&logs[idx].timestamp));
    
    history += "<tr>";
    history += "<td style='border:1px solid #ddd; padding:8px;'>" + String(timeStr) + "</td>";
    history += "<td style='border:1px solid #ddd; padding:8px;'>" + logs[idx].state + "</td>";
    history += "<td style='border:1px solid #ddd; padding:8px;'>" + String(logs[idx].power, 1) + " Вт</td>";
    history += "</tr>";
  }
  history += "</table>";

  String json = "{\"history\":\"" + history + "\"}";
  server.send(200, "application/json", json);
}

/**
 * API ДЛЯ ПОЛУЧЕНИЯ ЛОГОВ В JSON ФОРМАТЕ
 */
void handleApiLogs() {
  String json = "[";
  for (int i = 0; i < LOG_SIZE; i++) {
    int idx = (logIndex - 1 - i + LOG_SIZE) % LOG_SIZE;
    if (logs[idx].timestamp == 0) continue;
    
    if (i > 0) json += ",";
    json += "{";
    json += "\"timestamp\":" + String(logs[idx].timestamp) + ",";
    json += "\"power\":" + String(logs[idx].power, 1) + ",";
    json += "\"state\":\"" + logs[idx].state + "\"";
    json += "}";
  }
  json += "]";

  server.send(200, "application/json", json);
}

/**
 * СТРАНИЦА С ЛОГАМИ
 */
void handleLogs() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Логи мощности</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        table { width: 100%; border-collapse: collapse; margin: 20px 0; }
        th, td { border: 1px solid #ddd; padding: 12px; text-align: left; }
        th { background-color: #007bff; color: white; }
        tr:nth-child(even) { background-color: #f2f2f2; }
        .back-btn { background: #6c757d; color: white; padding: 10px 15px; text-decoration: none; border-radius: 5px; display: inline-block; margin-bottom: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <a href="/" class="back-btn">← Назад к мониторингу</a>
        <h1>📋 Логи измерений мощности</h1>
        <div id="logTable"></div>
    </div>

    <script>
        function loadLogs() {
            fetch('/api/logs')
                .then(response => response.json())
                .then(data => {
                    let tableHtml = '<table><tr><th>Время</th><th>Мощность (Вт)</th><th>Состояние</th></tr>';
                    
                    data.forEach(log => {
                        const date = new Date(log.timestamp * 1000);
                        const timeStr = date.toLocaleString('ru-RU');
                        
                        tableHtml += '<tr>';
                        tableHtml += '<td>' + timeStr + '</td>';
                        tableHtml += '<td>' + log.power + '</td>';
                        tableHtml += '<td>' + log.state + '</td>';
                        tableHtml += '</tr>';
                    });
                    
                    tableHtml += '</table>';
                    document.getElementById('logTable').innerHTML = tableHtml;
                });
        }
        
        loadLogs();
        setInterval(loadLogs, 10000); // Обновление каждые 10 секунд
    </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}