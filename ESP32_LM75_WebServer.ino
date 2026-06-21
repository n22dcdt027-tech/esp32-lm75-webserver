/*
  ĐỀ TÀI:
  Hệ thống IoT giám sát và cảnh báo nhiệt độ sử dụng
  ESP32 NodeMCU LuaNode32 + LM75 + WebServer.

  PHẦN CỨNG:
  - LM75: VCC -> 3V3, GND -> GND, SDA -> GPIO21, SCL -> GPIO22
  - LED đỏ: GPIO25 -> điện trở 220R -> LED -> GND
  - LED xanh: GPIO26 -> điện trở 220R -> LED -> GND
  - Buzzer module: IN -> GPIO27, VCC -> 3V3, GND -> GND

  THƯ VIỆN:
  Các thư viện đều có sẵn trong bộ ESP32 Arduino Core:
  WiFi.h, WebServer.h, Wire.h, Preferences.h
*/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Preferences.h>
#include <math.h>

// ============================================================
// 1. CẤU HÌNH NGƯỜI DÙNG
// ============================================================

const char* WIFI_SSID = "TEN_WIFI_CUA_BAN";
const char* WIFI_PASSWORD = "MAT_KHAU_WIFI";

const char* AP_SSID = "ESP32-LM75";
const char* AP_PASSWORD = "12345678";

constexpr uint8_t I2C_SDA_PIN = 21;
constexpr uint8_t I2C_SCL_PIN = 22;
constexpr uint8_t LED_RED_PIN = 25;
constexpr uint8_t LED_GREEN_PIN = 26;
constexpr uint8_t BUZZER_PIN = 27;

// Nếu còi hoạt động ngược, đổi HIGH thành LOW.
constexpr uint8_t BUZZER_ON_LEVEL = LOW;
constexpr uint8_t BUZZER_OFF_LEVEL =
    (BUZZER_ON_LEVEL == HIGH) ? LOW : HIGH;

constexpr unsigned long SENSOR_INTERVAL_MS = 1000;
constexpr unsigned long BUZZER_INTERVAL_MS = 300;
constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;
constexpr unsigned long SENSOR_RESCAN_INTERVAL_MS = 5000;

constexpr float DEFAULT_ALARM_ON_C = 37.0F;
constexpr float DEFAULT_ALARM_OFF_C = 33.0F;

// ============================================================
// 2. ĐỐI TƯỢNG VÀ BIẾN HỆ THỐNG
// ============================================================

WebServer server(80);
Preferences preferences;

uint8_t lm75Address = 0x48;
bool lm75Detected = false;
bool sensorHealthy = false;
bool alarmActive = false;
bool buzzerOutputState = false;

float temperatureC = NAN;
float alarmOnC = DEFAULT_ALARM_ON_C;
float alarmOffC = DEFAULT_ALARM_OFF_C;

unsigned long lastSensorReadMs = 0;
unsigned long lastBuzzerToggleMs = 0;
unsigned long lastWiFiReconnectMs = 0;
unsigned long lastSensorRescanMs = 0;

// ============================================================
// 3. GIAO DIỆN WEB NHÚNG
// ============================================================

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Giám sát nhiệt độ ESP32 - LM75</title>
  <style>
    :root {
      --bg: #f3f5f7;
      --card: #ffffff;
      --text: #17202a;
      --muted: #667085;
      --ok: #16803c;
      --danger: #c62828;
      --warning: #b26a00;
      --border: #d8dde3;
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      font-family: Arial, Helvetica, sans-serif;
      background: var(--bg);
      color: var(--text);
    }

    .container {
      width: min(1050px, 94%);
      margin: 26px auto 40px;
    }

    h1 {
      margin: 0;
      text-align: center;
      font-size: clamp(24px, 4vw, 36px);
    }

    .subtitle {
      text-align: center;
      color: var(--muted);
      margin: 8px 0 24px;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 14px;
    }

    .card {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 14px;
      padding: 20px;
      box-shadow: 0 5px 18px rgba(0, 0, 0, 0.06);
    }

    .label {
      color: var(--muted);
      font-size: 14px;
      margin-bottom: 10px;
    }

    .value {
      font-size: 29px;
      font-weight: 700;
      word-break: break-word;
    }

    .small-value {
      font-size: 20px;
      font-weight: 700;
      word-break: break-word;
    }

    .ok { color: var(--ok); }
    .danger { color: var(--danger); }
    .warning { color: var(--warning); }

    .wide { grid-column: 1 / -1; }

    canvas {
      width: 100%;
      height: 260px;
      border: 1px solid var(--border);
      border-radius: 10px;
      background: #fff;
    }

    .form-row {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
      gap: 12px;
      align-items: end;
    }

    label {
      display: block;
      color: var(--muted);
      font-size: 14px;
      margin-bottom: 6px;
    }

    input {
      width: 100%;
      padding: 10px 12px;
      border: 1px solid var(--border);
      border-radius: 8px;
      font-size: 16px;
    }

    button {
      width: 100%;
      padding: 11px 14px;
      border: 0;
      border-radius: 8px;
      background: #1f5fae;
      color: #fff;
      font-size: 16px;
      font-weight: 700;
      cursor: pointer;
    }

    .message {
      min-height: 22px;
      margin-top: 10px;
      font-size: 14px;
    }

    .footer {
      text-align: center;
      color: var(--muted);
      font-size: 13px;
      margin-top: 20px;
    }
  </style>
</head>
<body>
  <main class="container">
    <h1>HỆ THỐNG GIÁM SÁT NHIỆT ĐỘ</h1>
    <div class="subtitle">
      ESP32 NodeMCU LuaNode32 · Cảm biến LM75 · Máy chủ web nhúng
    </div>

    <section class="grid">
      <article class="card">
        <div class="label">Nhiệt độ hiện tại</div>
        <div id="temperature" class="value">-- °C</div>
      </article>

      <article class="card">
        <div class="label">Trạng thái hệ thống</div>
        <div id="status" class="small-value">Đang tải...</div>
      </article>

      <article class="card">
        <div class="label">Cảm biến LM75</div>
        <div id="sensor" class="small-value">--</div>
      </article>

      <article class="card">
        <div class="label">Địa chỉ cảm biến</div>
        <div id="sensorAddress" class="small-value">--</div>
      </article>

      <article class="card">
        <div class="label">Kết nối mạng</div>
        <div id="network" class="small-value">--</div>
      </article>

      <article class="card">
        <div class="label">Địa chỉ IP Wi-Fi</div>
        <div id="staIp" class="small-value">--</div>
      </article>

      <article class="card">
        <div class="label">Địa chỉ IP điểm truy cập</div>
        <div id="apIp" class="small-value">--</div>
      </article>

      <article class="card">
        <div class="label">Cường độ Wi-Fi</div>
        <div id="rssi" class="small-value">--</div>
      </article>

      <article class="card">
        <div class="label">Thời gian hoạt động</div>
        <div id="uptime" class="small-value">--</div>
      </article>

      <article class="card wide">
        <div class="label">Biểu đồ nhiệt độ gần nhất</div>
        <canvas id="chart" width="960" height="260"></canvas>
      </article>

      <article class="card wide">
        <div class="label">Cấu hình ngưỡng cảnh báo</div>
        <div class="form-row">
          <div>
            <label for="alarmOn">Bật cảnh báo từ (°C)</label>
            <input id="alarmOn" type="number" min="-40" max="125" step="0.1">
          </div>
          <div>
            <label for="alarmOff">Tắt cảnh báo khi dưới (°C)</label>
            <input id="alarmOff" type="number" min="-40" max="124.9" step="0.1">
          </div>
          <div>
            <button onclick="saveConfig()">Lưu ngưỡng</button>
          </div>
        </div>
        <div id="configMessage" class="message"></div>
      </article>
    </section>

    <div class="footer">
      Trang tự cập nhật mỗi giây. Có thể truy cập bằng Wi-Fi nội bộ hoặc
      mạng dự phòng “ESP32-LM75”.
    </div>
  </main>

  <script>
    const values = [];
    const maxPoints = 60;
    let configLoaded = false;

    function formatUptime(totalSeconds) {
      const days = Math.floor(totalSeconds / 86400);
      const hours = Math.floor((totalSeconds % 86400) / 3600);
      const minutes = Math.floor((totalSeconds % 3600) / 60);
      const seconds = totalSeconds % 60;

      let text = "";
      if (days > 0) text += days + " ngày ";
      text += hours + " giờ " + minutes + " phút " + seconds + " giây";
      return text;
    }

    function drawChart() {
      const canvas = document.getElementById("chart");
      const ctx = canvas.getContext("2d");
      const w = canvas.width;
      const h = canvas.height;

      ctx.clearRect(0, 0, w, h);
      ctx.fillStyle = "#ffffff";
      ctx.fillRect(0, 0, w, h);

      ctx.strokeStyle = "#e1e5ea";
      ctx.lineWidth = 1;

      for (let i = 1; i < 5; i++) {
        const y = (h / 5) * i;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      }

      if (values.length < 2) return;

      let min = Math.min(...values);
      let max = Math.max(...values);

      if (max - min < 2) {
        max += 1;
        min -= 1;
      }

      const padding = 28;
      const plotW = w - padding * 2;
      const plotH = h - padding * 2;

      ctx.strokeStyle = "#1f5fae";
      ctx.lineWidth = 3;
      ctx.beginPath();

      values.forEach((value, index) => {
        const x = padding + (index / (maxPoints - 1)) * plotW;
        const y = padding + (max - value) / (max - min) * plotH;

        if (index === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });

      ctx.stroke();

      ctx.fillStyle = "#667085";
      ctx.font = "14px Arial";
      ctx.fillText(max.toFixed(1) + " °C", 6, 18);
      ctx.fillText(min.toFixed(1) + " °C", 6, h - 8);
    }

    function setText(id, text, cssClass) {
      const element = document.getElementById(id);
      element.textContent = text;

      if (cssClass) {
        element.className =
          element.classList.contains("value")
            ? "value " + cssClass
            : "small-value " + cssClass;
      }
    }

    async function updateData() {
      try {
        const response = await fetch("/api/data", { cache: "no-store" });
        if (!response.ok) throw new Error("HTTP " + response.status);

        const data = await response.json();

        if (data.sensor_ok) {
          setText("temperature", data.temperature.toFixed(2) + " °C",
                  data.alarm ? "danger" : "ok");
          setText("sensor", "HOẠT ĐỘNG", "ok");
          setText("sensorAddress", data.sensor_address, "ok");

          values.push(data.temperature);
          if (values.length > maxPoints) values.shift();
          drawChart();
        } else {
          setText("temperature", "-- °C", "warning");
          setText("sensor", "LỖI / KHÔNG TÌM THẤY", "warning");
          setText("sensorAddress", data.sensor_address, "warning");
        }

        if (!data.sensor_ok) {
          setText("status", "LỖI CẢM BIẾN", "warning");
        } else if (data.alarm) {
          setText("status", "CẢNH BÁO QUÁ NHIỆT", "danger");
        } else {
          setText("status", "BÌNH THƯỜNG", "ok");
        }

        setText("network", data.network, data.wifi_connected ? "ok" : "warning");
        setText("staIp", data.sta_ip, data.wifi_connected ? "ok" : "warning");
        setText("apIp", data.ap_ip, "ok");

        if (data.wifi_connected) {
          setText("rssi", data.rssi + " dBm", "ok");
        } else {
          setText("rssi", "Không kết nối bộ phát", "warning");
        }

        setText("uptime", formatUptime(data.uptime), "ok");

        if (!configLoaded) {
          document.getElementById("alarmOn").value = data.alarm_on.toFixed(1);
          document.getElementById("alarmOff").value = data.alarm_off.toFixed(1);
          configLoaded = true;
        }
      } catch (error) {
        setText("status", "MẤT KẾT NỐI VỚI ESP32", "danger");
      }
    }

    async function saveConfig() {
      const alarmOn = document.getElementById("alarmOn").value;
      const alarmOff = document.getElementById("alarmOff").value;
      const message = document.getElementById("configMessage");

      message.textContent = "Đang lưu...";

      try {
        const body = new URLSearchParams();
        body.append("alarm_on", alarmOn);
        body.append("alarm_off", alarmOff);

        const response = await fetch("/api/config", {
          method: "POST",
          headers: {
            "Content-Type": "application/x-www-form-urlencoded"
          },
          body
        });

        const result = await response.json();

        if (!response.ok || !result.ok) {
          throw new Error(result.message || "Không thể lưu cấu hình");
        }

        message.textContent = result.message;
        message.className = "message ok";
        configLoaded = false;
        await updateData();
      } catch (error) {
        message.textContent = error.message;
        message.className = "message danger";
      }
    }

    updateData();
    setInterval(updateData, 1000);
  </script>
</body>
</html>
)HTML";

// ============================================================
// 4. HÀM TIỆN ÍCH
// ============================================================

void setBuzzer(bool on) {
  buzzerOutputState = on;
  digitalWrite(BUZZER_PIN, on ? BUZZER_ON_LEVEL : BUZZER_OFF_LEVEL);
}

String boolToJson(bool value) {
  return value ? "true" : "false";
}

String sensorAddressToString() {
  if (!lm75Detected) {
    return "Chưa xác định";
  }

  char buffer[8];
  snprintf(buffer, sizeof(buffer), "0x%02X", lm75Address);
  return String(buffer);
}

// ============================================================
// 5. GIAO TIẾP LM75
// ============================================================

bool probeI2CAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool detectLM75() {
  for (uint8_t address = 0x48; address <= 0x4F; ++address) {
    if (probeI2CAddress(address)) {
      lm75Address = address;
      lm75Detected = true;

      Serial.print("Tim thay LM75 tai dia chi 0x");
      if (address < 0x10) Serial.print('0');
      Serial.println(address, HEX);
      return true;
    }
  }

  lm75Detected = false;
  return false;
}

float readLM75Temperature() {
  if (!lm75Detected) {
    return NAN;
  }

  Wire.beginTransmission(lm75Address);
  Wire.write(0x00);

  if (Wire.endTransmission(false) != 0) {
    return NAN;
  }

  const uint8_t bytesRead =
      Wire.requestFrom(lm75Address, static_cast<uint8_t>(2));

  if (bytesRead != 2) {
    return NAN;
  }

  const uint8_t msb = Wire.read();
  const uint8_t lsb = Wire.read();

  const int16_t raw =
      static_cast<int16_t>(
          (static_cast<uint16_t>(msb) << 8) |
          static_cast<uint16_t>(lsb));

  const float value = raw / 256.0F;

  if (value < -55.0F || value > 125.0F) {
    return NAN;
  }

  return value;
}

// ============================================================
// 6. XỬ LÝ CẢNH BÁO VÀ ĐẦU RA
// ============================================================

void updateAlarmState() {
  if (!sensorHealthy) {
    alarmActive = false;
    digitalWrite(LED_RED_PIN, LOW);
    digitalWrite(LED_GREEN_PIN, LOW);
    setBuzzer(false);
    return;
  }

  if (!alarmActive && temperatureC >= alarmOnC) {
    alarmActive = true;
  } else if (alarmActive && temperatureC <= alarmOffC) {
    alarmActive = false;
  }

  digitalWrite(LED_RED_PIN, alarmActive ? HIGH : LOW);
  digitalWrite(LED_GREEN_PIN, alarmActive ? LOW : HIGH);

  if (!alarmActive) {
    setBuzzer(false);
  }
}

void updateBuzzer() {
  if (!alarmActive || !sensorHealthy) {
    setBuzzer(false);
    return;
  }

  const unsigned long now = millis();

  if (now - lastBuzzerToggleMs >= BUZZER_INTERVAL_MS) {
    lastBuzzerToggleMs = now;
    setBuzzer(!buzzerOutputState);
  }
}

// ============================================================
// 7. MẠNG WI-FI
// ============================================================

void startNetwork() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_AP_STA);

  const bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);

  if (apStarted) {
    Serial.print("Diem truy cap da khoi dong. IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Khoi dong diem truy cap that bai.");
  }

  if (strlen(WIFI_SSID) == 0 ||
      String(WIFI_SSID) == "TEN_WIFI_CUA_BAN") {
    Serial.println(
        "Chua cau hinh Wi-Fi. Su dung diem truy cap ESP32-LM75.");
    return;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Dang ket noi Wi-Fi");

  const unsigned long startMs = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startMs < 15000UL) {
    delay(500);
    Serial.print('.');
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Da ket noi Wi-Fi.");
    Serial.print("Dia chi IP Wi-Fi: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(
        "Khong ket noi duoc Wi-Fi. "
        "Van co the truy cap diem ESP32-LM75 tai 192.168.4.1.");
  }
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (strlen(WIFI_SSID) == 0 ||
      String(WIFI_SSID) == "TEN_WIFI_CUA_BAN") {
    return;
  }

  const unsigned long now = millis();

  if (now - lastWiFiReconnectMs >= WIFI_RECONNECT_INTERVAL_MS) {
    lastWiFiReconnectMs = now;
    Serial.println("Thu ket noi lai Wi-Fi...");
    WiFi.reconnect();
  }
}

// ============================================================
// 8. XỬ LÝ YÊU CẦU WEB
// ============================================================

void handleRoot() {
  server.send_P(
      200,
      "text/html; charset=utf-8",
      INDEX_HTML);
}

void handleApiData() {
  const bool wifiConnected = WiFi.status() == WL_CONNECTED;

  String json;
  json.reserve(420);

  json += "{";

  json += "\"temperature\":";
  if (sensorHealthy) {
    json += String(temperatureC, 2);
  } else {
    json += "null";
  }
  json += ",";

  json += "\"sensor_ok\":";
  json += boolToJson(sensorHealthy);
  json += ",";

  json += "\"alarm\":";
  json += boolToJson(alarmActive);
  json += ",";

  json += "\"alarm_on\":";
  json += String(alarmOnC, 1);
  json += ",";

  json += "\"alarm_off\":";
  json += String(alarmOffC, 1);
  json += ",";

  json += "\"sensor_address\":\"";
  json += sensorAddressToString();
  json += "\",";

  json += "\"wifi_connected\":";
  json += boolToJson(wifiConnected);
  json += ",";

  json += "\"network\":\"";
  json += wifiConnected
              ? "Wi-Fi + điểm truy cập dự phòng"
              : "Chỉ có điểm truy cập dự phòng";
  json += "\",";

  json += "\"sta_ip\":\"";
  json += wifiConnected
              ? WiFi.localIP().toString()
              : String("Chưa kết nối");
  json += "\",";

  json += "\"ap_ip\":\"";
  json += WiFi.softAPIP().toString();
  json += "\",";

  json += "\"rssi\":";
  json += wifiConnected ? String(WiFi.RSSI()) : String(-127);
  json += ",";

  json += "\"uptime\":";
  json += String(millis() / 1000UL);

  json += "}";

  server.send(
      200,
      "application/json; charset=utf-8",
      json);
}

void handleApiConfig() {
  if (!server.hasArg("alarm_on") ||
      !server.hasArg("alarm_off")) {
    server.send(
        400,
        "application/json; charset=utf-8",
        "{\"ok\":false,"
        "\"message\":\"Thiếu giá trị ngưỡng.\"}");
    return;
  }

  const float newAlarmOn = server.arg("alarm_on").toFloat();
  const float newAlarmOff = server.arg("alarm_off").toFloat();

  const bool validRange =
      newAlarmOn >= -40.0F &&
      newAlarmOn <= 125.0F &&
      newAlarmOff >= -40.0F &&
      newAlarmOff < newAlarmOn;

  if (!validRange) {
    server.send(
        400,
        "application/json; charset=utf-8",
        "{\"ok\":false,"
        "\"message\":\"Ngưỡng tắt phải nhỏ hơn ngưỡng bật và "
        "các giá trị phải nằm trong khoảng -40 đến 125 °C.\"}");
    return;
  }

  alarmOnC = newAlarmOn;
  alarmOffC = newAlarmOff;

  preferences.putFloat("alarmOn", alarmOnC);
  preferences.putFloat("alarmOff", alarmOffC);

  updateAlarmState();

  server.send(
      200,
      "application/json; charset=utf-8",
      "{\"ok\":true,"
      "\"message\":\"Đã lưu ngưỡng cảnh báo.\"}");
}

void handleNotFound() {
  server.send(
      404,
      "application/json; charset=utf-8",
      "{\"ok\":false,"
      "\"message\":\"Không tìm thấy tài nguyên.\"}");
}

void startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.on("/api/config", HTTP_POST, handleApiConfig);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("May chu web da khoi dong cong 80.");
}

// ============================================================
// 9. CẤU HÌNH LƯU TRỮ
// ============================================================

void loadConfiguration() {
  preferences.begin("lm75cfg", false);

  alarmOnC =
      preferences.getFloat("alarmOn", DEFAULT_ALARM_ON_C);
  alarmOffC =
      preferences.getFloat("alarmOff", DEFAULT_ALARM_OFF_C);

  if (alarmOnC < -40.0F ||
      alarmOnC > 125.0F ||
      alarmOffC < -40.0F ||
      alarmOffC >= alarmOnC) {
    alarmOnC = DEFAULT_ALARM_ON_C;
    alarmOffC = DEFAULT_ALARM_OFF_C;

    preferences.putFloat("alarmOn", alarmOnC);
    preferences.putFloat("alarmOff", alarmOffC);
  }

  Serial.print("Nguong bat canh bao: ");
  Serial.print(alarmOnC, 1);
  Serial.println(" C");

  Serial.print("Nguong tat canh bao: ");
  Serial.print(alarmOffC, 1);
  Serial.println(" C");
}

// ============================================================
// 10. SETUP VÀ LOOP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);
  setBuzzer(false);

  loadConfiguration();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);

  if (!detectLM75()) {
    Serial.println(
        "Khong tim thay LM75 trong dai dia chi 0x48-0x4F.");
  }

  startNetwork();
  startWebServer();

  Serial.println();
  Serial.println("===== DIA CHI TRUY CAP =====");
  Serial.print("Diem truy cap: http://");
  Serial.println(WiFi.softAPIP());

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi noi bo: http://");
    Serial.println(WiFi.localIP());
  }

  Serial.println("============================");
}

void loop() {
  const unsigned long now = millis();

  server.handleClient();
  maintainWiFi();

  if (!lm75Detected &&
      now - lastSensorRescanMs >= SENSOR_RESCAN_INTERVAL_MS) {
    lastSensorRescanMs = now;
    detectLM75();
  }

  if (now - lastSensorReadMs >= SENSOR_INTERVAL_MS) {
    lastSensorReadMs = now;

    const float newTemperature = readLM75Temperature();

    sensorHealthy = !isnan(newTemperature);

    if (sensorHealthy) {
      temperatureC = newTemperature;
    } else {
      lm75Detected = false;
      temperatureC = NAN;
    }

    updateAlarmState();

    if (sensorHealthy) {
      Serial.print("Nhiet do: ");
      Serial.print(temperatureC, 2);
      Serial.print(" C | Trang thai: ");
      Serial.println(alarmActive ? "CANH BAO" : "BINH THUONG");
    } else {
      Serial.println("Loi doc LM75. Se quet lai cam bien.");
    }
  }

  updateBuzzer();
  delay(2);
}
