#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>          // 热点网页配网
#include <WebSocketsClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ========== 外设 GPIO 对应分配 ==========
#define DHTPIN 4
#define DHTTYPE DHT11

// 3 个 LED 灯泡引脚
const int LED_PINS[3] = {16, 17, 18}; 
// 4 路继电器引脚
const int RELAY_PINS[4] = {19, 21, 22, 23}; 

// 自定义配置变量 (通过 WiFiManager 动态输入，不硬编码)
char cf_ws_host[64] = "your-worker.your-subdomain.workers.dev";
char cf_auth_key[32] = "";

DHT dht(DHTPIN, DHTTYPE);
WebSocketsClient webSocket;

// 状态变量
bool autoControlMode = true; // 默认开启温湿度自动控制
unsigned long lastSensorReport = 0;

// 自动控制阈值定义
const float TEMP_HIGH_THRESHOLD = 30.0; // 温度 > 30°C 自动开启继电器 1 (降温/风扇) 和 LED 1
const float HUMI_HIGH_THRESHOLD = 70.0; // 湿度 > 70% 自动开启继电器 2 (除湿)

// 继电器输出封装 (多数继电器板为低电平触发 LOW 为开)
void setRelay(int index, bool turnOn) {
  digitalWrite(RELAY_PINS[index], turnOn ? LOW : HIGH);
}

// LED 输出封装 (高电平触发 HIGH 为开)
void setLED(int index, bool turnOn) {
  digitalWrite(LED_PINS[index], turnOn ? HIGH : LOW);
}

// 自动温湿度控制逻辑 (本地运行)
void handleAutoLogic(float temp, float humi) {
  if (!autoControlMode) return;

  // 温度控制示例：温度高时开启继电器1与LED1
  if (temp > TEMP_HIGH_THRESHOLD) {
    setRelay(0, true);
    setLED(0, true);
  } else {
    setRelay(0, false);
    setLED(0, false);
  }

  // 湿度控制示例：湿度高时开启继电器2
  if (humi > HUMI_HIGH_THRESHOLD) {
    setRelay(1, true);
  } else {
    setRelay(1, false);
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) return;

    const char* action = doc["action"];

    // 自动化开关
    if (strcmp(action, "AUTO_ON") == 0) autoControlMode = true;
    if (strcmp(action, "AUTO_OFF") == 0) autoControlMode = false;

    // 手动控制开关 LED
    if (strcmp(action, "LED1_ON") == 0) setLED(0, true);
    if (strcmp(action, "LED1_OFF") == 0) setLED(0, false);
    if (strcmp(action, "LED2_ON") == 0) setLED(1, true);
    if (strcmp(action, "LED2_OFF") == 0) setLED(1, false);
    if (strcmp(action, "LED3_ON") == 0) setLED(2, true);
    if (strcmp(action, "LED3_OFF") == 0) setLED(2, false);

    // 手动控制 4 路继电器
    if (strcmp(action, "R1_ON") == 0) setRelay(0, true);
    if (strcmp(action, "R1_OFF") == 0) setRelay(0, false);
    if (strcmp(action, "R2_ON") == 0) setRelay(1, true);
    if (strcmp(action, "R2_OFF") == 0) setRelay(1, false);
    if (strcmp(action, "R3_ON") == 0) setRelay(2, true);
    if (strcmp(action, "R3_OFF") == 0) setRelay(2, false);
    if (strcmp(action, "R4_ON") == 0) setRelay(3, true);
    if (strcmp(action, "R4_OFF") == 0) setRelay(3, false);
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  // 初始化外设引脚
  for (int i = 0; i < 3; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    setLED(i, false);
  }
  for (int i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    setRelay(i, false); // 默认关闭继电器
  }

  // WiFiManager 初始化：添加自定义网页配置参数
  WiFiManager wm;
  WiFiManagerParameter custom_ws_host("host", "CF Worker 域名", cf_ws_host, 64);
  WiFiManagerParameter custom_auth_key("key", "Auth Secret 密钥", cf_auth_key, 32);
  wm.addParameter(&custom_ws_host);
  wm.addParameter(&custom_auth_key);

  // 如果连不上已知 Wi-Fi，则开启名为 ESP32-AP-Config 的配网热点
  if (!wm.autoConnect("ESP32-AP-Config")) {
    ESP.restart();
  }

  // 读取配网网页保存的参数
  strcpy(cf_ws_host, custom_ws_host.getValue());
  strcpy(cf_auth_key, custom_auth_key.getValue());

  // 构建带 auth 参数的 WebSocket 路径
  String path = "/ws?auth=" + String(cf_auth_key);

  // 建立连接
  webSocket.beginSSL(cf_ws_host, 443, path.c_str());
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();

  // 每 3 秒读取一次传感器并上传数据、触发自动化
  if (millis() - lastSensorReport > 3000) {
    lastSensorReport = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      // 1. 本地自动化处理
      handleAutoLogic(t, h);

      // 2. 将数据实时推送到 Worker，并在控制面板上更新
      StaticJsonDocument<128> doc;
      doc["temp"] = t;
      doc["humi"] = h;
      doc["autoMode"] = autoControlMode;
      String jsonString;
      serializeJson(doc, jsonString);
      webSocket.sendTXT(jsonString);
    }
  }
}
