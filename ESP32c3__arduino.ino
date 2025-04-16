#include <WiFi.h>
#include <WebServer.h>
#include <BleKeyboard.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// WiFi热点参数
const char* ssid = "ESP32-WebControl";
const char* password = "";

// BLE键盘
BleKeyboard bleKeyboard;

// Web服务器
WebServer server(80);

// 循环拍照状态
bool isContinuousShooting = false;
unsigned long lastPhotoTime = 0;
unsigned long photoInterval = 10000; // 默认10秒

// GPIO8 LED
const int ledPin = 8;
bool isLedOn = false;
unsigned long lastLedToggleTime = 0;
unsigned long ledInterval = 1000; // 正常状态下的闪烁间隔（毫秒）

// WiFi设置
String savedWifiName = "";
String savedWifiPassword = "";

// 蓝牙名称
String savedBleName = "ESP32-C3";

// 拍照按键
String selectedKey = "KEY_MEDIA_VOLUME_UP";

// Preferences for storing settings
Preferences preferences;

// 连接WiFi热点
void setupWiFi() {
  WiFi.setSleep(ESP_LIGHT_SLEEP); // 启用WiFi低功耗模式
  WiFi.softAP(ssid, password);
  Serial.println("WiFi热点已创建");
  Serial.print("IP地址: ");
  Serial.println(WiFi.softAPIP());
}

// 初始化LED
void setupLed() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH); // 初始化为关闭状态 (0为亮，1为灭)
  lastLedToggleTime = millis();
}

// 非阻塞LED闪烁逻辑
void updateLed() {
  unsigned long currentTime = millis();
  if (currentTime - lastLedToggleTime >= ledInterval) {
    lastLedToggleTime = currentTime;
    isLedOn = !isLedOn;
    digitalWrite(ledPin, isLedOn ? HIGH : LOW); // 更新LED状态
  }
}

// 初始化Web服务器路由
void setupServer() {
  server.on("/", handleRoot);
  server.on("/take_photo", handleTakePhoto);
  server.on("/start_continuous", handleStartContinuous);
  server.on("/stop_continuous", handleStopContinuous);
  server.on("/save_wifi", handleSaveWifi);
  server.on("/save_bluetooth", handleSaveBleName);
  server.on("/get_settings", handleGetSettings);
  server.on("/heartbeat", handleHeartbeat);
  server.on("/bluetooth_status", handleBluetoothStatus);
  server.begin();
  Serial.println("Web服务器已启动");
}

// 拍照处理
void handleTakePhoto() {
  if (bleKeyboard.isConnected()) {
    sendKeyPress(selectedKey);
    server.send(200, "text/plain", "拍照成功");
  } else {
    server.send(500, "text/plain", "设备未连接");
  }
}

// 开始连拍
void handleStartContinuous() {
  if (server.method() == HTTP_POST && server.hasArg("plain")) {
    String json = server.arg("plain");
    int startIndex = json.indexOf('{');
    int endIndex = json.lastIndexOf('}') + 1;
    if (startIndex != -1 && endIndex != -1 && endIndex > startIndex) {
      json = json.substring(startIndex, endIndex);
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, json);
      if (!error) {
        int interval = doc["interval"].as<int>();
        if (interval > 0) {
          photoInterval = interval * 1000; // 转换为毫秒
          isContinuousShooting = true;
          lastPhotoTime = millis();
          server.send(200, "text/plain", "开始连拍");
        } else {
          server.send(400, "text/plain", "无效的间隔值");
        }
      } else {
        server.send(500, "text/plain", "错误：无法解析JSON");
      }
    } else {
      server.send(400, "text/plain", "错误：无效的JSON格式");
    }
  } else {
    server.send(405, "text/plain", "错误：方法不支持");
  }
}

// 停止连拍
void handleStopContinuous() {
  isContinuousShooting = false;
  server.send(200, "text/plain", "停止连拍");
}

// 保存WiFi设置
void handleSaveWifi() {
  if (server.method() == HTTP_POST && server.hasArg("plain")) {
    String json = server.arg("plain");
    int startIndex = json.indexOf('{');
    int endIndex = json.lastIndexOf('}') + 1;
    if (startIndex != -1 && endIndex != -1 && endIndex > startIndex) {
      json = json.substring(startIndex, endIndex);
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, json);
      if (!error) {
        savedWifiName = doc["name"].as<String>();
        savedWifiPassword = doc["password"].as<String>();
        preferences.putString("wifiName", savedWifiName);
        preferences.putString("wifiPassword", savedWifiPassword);
        server.send(200, "text/plain", "WiFi设置已保存");
      } else {
        server.send(500, "text/plain", "错误：无法解析JSON");
      }
    } else {
      server.send(400, "text/plain", "错误：无效的JSON格式");
    }
  } else {
    server.send(405, "text/plain", "错误：方法不支持");
  }
}

// 保存蓝牙设置
void handleSaveBleName() {
  if (server.method() == HTTP_POST && server.hasArg("plain")) {
    String json = server.arg("plain");
    int startIndex = json.indexOf('{');
    int endIndex = json.lastIndexOf('}') + 1;
    if (startIndex != -1 && endIndex != -1 && endIndex > startIndex) {
      json = json.substring(startIndex, endIndex);
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, json);
      if (!error) {
        savedBleName = doc["name"].as<String>();
        selectedKey = doc["key"].as<String>();
        bleKeyboard.setName(savedBleName.c_str());
        preferences.putString("bleName", savedBleName);
        preferences.putString("selectedKey", selectedKey);
        server.send(200, "text/plain", "蓝牙设置已保存");
      } else {
        server.send(500, "text/plain", "错误：无法解析JSON");
      }
    } else {
      server.send(400, "text/plain", "错误：无效的JSON格式");
    }
  } else {
    server.send(405, "text/plain", "错误：方法不支持");
  }
}

// 获取保存的设置
void handleGetSettings() {
  DynamicJsonDocument doc(1024);
  doc["wifiName"] = preferences.getString("wifiName", "");
  doc["wifiPassword"] = preferences.getString("wifiPassword", "");
  doc["bluetoothName"] = preferences.getString("bleName", "ESP32-C3");
  doc["selectedKey"] = preferences.getString("selectedKey", "KEY_MEDIA_VOLUME_UP");

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// 心跳检测
void handleHeartbeat() {
  server.send(200, "text/plain", "alive");
}

// 蓝牙连接状态
void handleBluetoothStatus() {
  String status = bleKeyboard.isConnected() ? "connected" : "disconnected";
  server.send(200, "text/plain", status);
}

// 发送按键
void sendKeyPress(String key) {
  if (key == "KEY_MEDIA_VOLUME_UP") {
    bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
  } else if (key == "KEY_MEDIA_VOLUME_DOWN") {
    bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
  } else if (key == "KEY_ESC") {
    bleKeyboard.write(KEY_ESC);
  } else if (key == "KEY_UP_ARROW") {
    bleKeyboard.write(KEY_UP_ARROW);
  } else if (key == "KEY_DOWN_ARROW") {
    bleKeyboard.write(KEY_DOWN_ARROW);
  } else if (key == "KEY_LEFT_ARROW") {
    bleKeyboard.write(KEY_LEFT_ARROW);
  } else if (key == "KEY_RIGHT_ARROW") {
    bleKeyboard.write(KEY_RIGHT_ARROW);
  } else if (key == "KEY_PAGE_UP") {
    bleKeyboard.write(KEY_PAGE_UP);
  } else if (key == "KEY_PAGE_DOWN") {
    bleKeyboard.write(KEY_PAGE_DOWN);
  } else if (key == "KEY_TAB") {
    bleKeyboard.write(KEY_TAB);
  } else if (key == "KEY_SHIFT") {
    bleKeyboard.write(KEY_LEFT_SHIFT);
  } else if (key == "KEY_CONTROL") {
    bleKeyboard.write(KEY_LEFT_CTRL);
  } else if (key == "KEY_ALT") {
    bleKeyboard.write(KEY_LEFT_ALT);
  } else if (key == "KEY_F1") {
    bleKeyboard.write(KEY_F1);
  } else if (key == "KEY_F2") {
    bleKeyboard.write(KEY_F2);
  } else if (key == "KEY_F3") {
    bleKeyboard.write(KEY_F3);
  } else if (key == "KEY_F4") {
    bleKeyboard.write(KEY_F4);
  } else if (key == "KEY_F5") {
    bleKeyboard.write(KEY_F5);
  } else if (key == "KEY_F6") {
    bleKeyboard.write(KEY_F6);
  } else if (key == "KEY_F7") {
    bleKeyboard.write(KEY_F7);
  } else if (key == "KEY_F8") {
    bleKeyboard.write(KEY_F8);
  } else if (key == "KEY_F9") {
    bleKeyboard.write(KEY_F9);
  } else if (key == "KEY_F10") {
    bleKeyboard.write(KEY_F10);
  } else if (key == "KEY_F11") {
    bleKeyboard.write(KEY_F11);
  } else if (key == "KEY_F12") {
    bleKeyboard.write(KEY_F12);
  } else {
    bleKeyboard.write(KEY_MEDIA_VOLUME_UP); // 默认使用音量+
  }
}

// 处理根路径请求
void handleRoot() {
  server.send(200, "text/html", getWebPage());
}

// 获取网页内容

// 获取网页内容
String getWebPage() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>ESP32拍照控制</title>
      <style>
          :root {
              --primary-color: #42b983;
              --secondary-color: #58677a;
              --background-color: #f5f7fa;
              --card-color: #ffffff;
              --text-color: #333333;
              --border-radius: 8px;
              --shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
              --pause-color: #e74c3c;
          }

          * {
              margin: 0;
              padding: 0;
              box-sizing: border-box;
              font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          }

          body {
              background-color: var(--background-color);
              color: var(--text-color);
              min-height: 100vh;
              display: flex;
              flex-direction: column;
              align-items: center;
              padding: 20px;
          }

          .container {
              width: 100%;
              max-width: 500px;
          }

          header {
              text-align: center;
              margin-bottom: 30px;
          }

          h1 {
              color: var(--primary-color);
              margin-bottom: 10px;
          }

          .tabs {
              display: flex;
              margin-bottom: 20px;
              background-color: var(--card-color);
              border-radius: var(--border-radius);
              box-shadow: var(--shadow);
              overflow: hidden;
          }

          .tab {
              flex: 1;
              padding: 15px;
              text-align: center;
              cursor: pointer;
              transition: background-color 0.3s;
          }

          .tab.active {
              background-color: var(--primary-color);
              color: white;
          }

          .tab:hover {
              background-color: rgba(66, 185, 131, 0.1);
          }

          .tab-content {
              display: none;
              background-color: var(--card-color);
              border-radius: var(--border-radius);
              box-shadow: var(--shadow);
              padding: 20px;
              margin-bottom: 20px;
          }

          .tab-content.active {
              display: block;
          }

          .card-title {
              font-size: 18px;
              font-weight: 600;
              margin-bottom: 15px;
              color: var(--secondary-color);
          }

          .btn {
              display: block;
              width: 100%;
              padding: 12px;
              background-color: var(--primary-color);
              color: white;
              border: none;
              border-radius: var(--border-radius);
              font-size: 16px;
              font-weight: 500;
              cursor: pointer;
              transition: background-color 0.3s, transform 0.2s;
              text-align: center;
              margin-bottom: 10px;
          }

          .btn:hover {
              background-color: #3a916f;
              transform: translateY(-2px);
          }

          .btn:active {
              transform: translateY(0);
          }

          .btn:disabled {
              background-color: #cccccc;
              cursor: not-allowed;
              transform: none;
          }

          .interval-form {
              display: flex;
              flex-direction: column;
              gap: 10px;
          }

          .form-group {
              display: flex;
              flex-direction: column;
              gap: 5px;
          }

          label {
              margin-bottom: 5px;
              font-weight: 500;
          }

          input {
              padding: 10px;
              border: 1px solid #ddd;
              border-radius: var(--border-radius);
              font-size: 14px;
          }

          .status {
              margin-top: 15px;
              font-size: 14px;
              color: var(--secondary-color);
          }

          .pause-btn {
              background-color: var(--pause-color);
          }

          .pause-btn:hover {
              background-color: #c0392b;
          }

          .wifi-form {
              display: flex;
              flex-direction: column;
              gap: 15px;
          }

          .wifi-btn {
              margin-top: 10px;
          }

          .bluetooth-status {
              position: fixed;
              top: 20px;
              right: 20px;
              width: 20px;
              height: 20px;
              background-color: #e74c3c;
              border-radius: 50%;
              animation: heartbeat 1s infinite;
          }

          .bluetooth-status.connected {
              background-color: var(--primary-color);
          }

          @keyframes heartbeat {
              0% {
                  transform: scale(1);
              }
              50% {
                  transform: scale(1.2);
              }
              100% {
                  transform: scale(1);
              }
          }

          @media (max-width: 480px) {
              .container {
                  padding: 0 10px;
              }
          }

          .key-select {
              margin-top: 10px;
              padding: 10px;
              background-color: #f8f9fa;
              border-radius: var(--border-radius);
              font-size: 14px;
              color: var(--secondary-color);
          }
      </style>
  </head>
  <body>
      <div class="container">
          <header>
              <h1>ESP32拍照控制</h1>
              <p>通过Wi-Fi控制您的设备进行拍照</p>
          </header>

          <div class="tabs">
              <div class="tab active" data-tab="control">控制面板</div>
              <div class="tab" data-tab="settings">WiFi设置</div>
              <div class="tab" data-tab="bluetooth">蓝牙设置</div>
          </div>

          <div id="control" class="tab-content active">
              <div class="card">
                  <div class="card-title">拍照控制</div>
                  <button id="take-photo-btn" class="btn">拍照</button>
              </div>

              <div class="card">
                  <div class="card-title">连拍控制</div>
                  <div class="interval-form">
                      <div class="form-group">
                          <label for="interval-input">拍照间隔（秒）：</label>
                          <input type="number" id="interval-input" min="1" value="10">
                      </div>
                  </div>
                  <button id="start-stop-continuous-btn" class="btn">开始连拍</button>
              </div>

              <div class="card">
                  <div class="card-title">状态</div>
                  <div class="status" id="status">当前状态：空闲</div>
                  <div id="countdown" class="status"></div>
              </div>

              <div class="card">
                  <div class="key-select" id="current-key">当前设置控制按键为：音量+</div>
              </div>
          </div>

          <div id="settings" class="tab-content">
              <div class="card">
                  <div class="card-title">WiFi设置</div>
                  <div class="wifi-form">
                      <div class="form-group">
                          <label for="wifi-name">WiFi名称：</label>
                          <input type="text" id="wifi-name" placeholder="输入WiFi名称">
                      </div>
                      <div class="form-group">
                          <label for="wifi-password">WiFi密码：</label>
                          <input type="password" id="wifi-password" placeholder="输入WiFi密码">
                      </div>
                      <button id="save-wifi-btn" class="btn">保存设置</button>
                  </div>
                  <div class="status" id="wifi-status">WiFi设置未保存</div>
              </div>
          </div>

          <div id="bluetooth" class="tab-content">
              <div class="card">
                  <div class="card-title">蓝牙设置</div>
                  <div class="wifi-form">
                      <div class="form-group">
                          <label for="bluetooth-name">蓝牙名称：</label>
                          <input type="text" id="bluetooth-name" placeholder="输入蓝牙名称">
                      </div>
                      <div class="form-group">
                          <label for="key-select">拍照按键选择：</label>
                          <select id="key-select">
                              <option value="KEY_MEDIA_VOLUME_UP">音量+</option>
                              <option value="KEY_MEDIA_VOLUME_DOWN">音量-</option>
                              <option value="KEY_ESC">ESC</option>
                              <option value="KEY_UP_ARROW">上箭头</option>
                              <option value="KEY_DOWN_ARROW">下箭头</option>
                              <option value="KEY_LEFT_ARROW">左箭头</option>
                              <option value="KEY_RIGHT_ARROW">右箭头</option>
                              <option value="KEY_PAGE_UP">PgUp</option>
                              <option value="KEY_PAGE_DOWN">PgDn</option>
                              <option value="KEY_TAB">Tab</option>
                              <option value="KEY_SHIFT">Shift</option>
                              <option value="KEY_CONTROL">Ctrl</option>
                              <option value="KEY_ALT">Alt</option>
                              <option value="KEY_F1">F1</option>
                              <option value="KEY_F2">F2</option>
                              <option value="KEY_F3">F3</option>
                              <option value="KEY_F4">F4</option>
                              <option value="KEY_F5">F5</option>
                              <option value="KEY_F6">F6</option>
                              <option value="KEY_F7">F7</option>
                              <option value="KEY_F8">F8</option>
                              <option value="KEY_F9">F9</option>
                              <option value="KEY_F10">F10</option>
                              <option value="KEY_F11">F11</option>
                              <option value="KEY_F12">F12</option>
                          </select>
                      </div>
                      <button id="save-bluetooth-btn" class="btn">保存设置</button>
                  </div>
                  <div class="status" id="bluetooth-status">蓝牙设置未保存</div>
              </div>
          </div>
      </div>

      <div class="bluetooth-status" id="bluetooth-indicator"></div>

      <script>
          // Tab切换功能
          const tabs = document.querySelectorAll('.tab');
          const tabContents = document.querySelectorAll('.tab-content');

          tabs.forEach(tab => {
              tab.addEventListener('click', () => {
                  const tabId = tab.getAttribute('data-tab');
                  
                  // 更新tab状态
                  tabs.forEach(t => t.classList.remove('active'));
                  tab.classList.add('active');
                  
                  // 更新内容显示
                  tabContents.forEach(content => content.classList.remove('active'));
                  document.getElementById(tabId).classList.add('active');
              });
          });

          // 更新状态显示
          function updateStatus(message) {
              document.getElementById('status').textContent = `当前状态：${message}`;
          }

          // 更新按钮状态
          function updateButtonState(isConnected) {
              const takePhotoBtn = document.getElementById('take-photo-btn');
              const startStopBtn = document.getElementById('start-stop-continuous-btn');
              
              if (isConnected) {
                  takePhotoBtn.disabled = false;
                  startStopBtn.disabled = false;
              } else {
                  takePhotoBtn.disabled = true;
                  startStopBtn.disabled = true;
                  updateStatus('等待蓝牙连接中...');
              }
          }

          // 拍照按钮事件
          document.getElementById('take-photo-btn').addEventListener('click', () => {
              const takePhotoBtn = document.getElementById('take-photo-btn');
              takePhotoBtn.textContent = '拍照中...';
              
              fetch('/take_photo')
                  .then(response => response.text())
                  .then(data => {
                      takePhotoBtn.textContent = data;
                      setTimeout(() => {
                          takePhotoBtn.textContent = '拍照';
                      }, 1000);
                  })
                  .catch(error => {
                      console.error('Error:', error);
                      takePhotoBtn.textContent = '错误：无法拍照';
                      setTimeout(() => {
                          takePhotoBtn.textContent = '拍照';
                      }, 1000);
                  });
          });

          // 连拍控制按钮事件
          let isContinuousShooting = false;
          let countdownInterval;

          document.getElementById('start-stop-continuous-btn').addEventListener('click', () => {
              const startStopBtn = document.getElementById('start-stop-continuous-btn');
              const interval = document.getElementById('interval-input').value;
              
              if (!isContinuousShooting) {
                  // 开始连拍
                  isContinuousShooting = true;
                  startStopBtn.textContent = '停止连拍';
                  startStopBtn.classList.remove('pause-btn');
                  fetch('/start_continuous', {
                      method: 'POST',
                      headers: {
                          'Content-Type': 'application/json',
                      },
                      body: JSON.stringify({ interval: interval }),
                  })
                      .then(response => response.text())
                      .then(data => {
                          updateStatus(data);
                          startCountdown();
                      })
                      .catch(error => {
                          console.error('Error:', error);
                          updateStatus('错误：无法开始连拍');
                      });
              } else {
                  // 停止连拍
                  isContinuousShooting = false;
                  startStopBtn.textContent = '开始连拍';
                  startStopBtn.classList.add('pause-btn');
                  fetch('/stop_continuous')
                      .then(response => response.text())
                      .then(data => {
                          updateStatus(data);
                          clearInterval(countdownInterval);
                          document.getElementById('countdown').textContent = '';
                      })
                      .catch(error => {
                          console.error('Error:', error);
                          updateStatus('错误：无法停止连拍');
                      });
              }
          });

          // 开始倒计时
          function startCountdown() {
              const interval = parseInt(document.getElementById('interval-input').value) * 1000;
              let remainingTime = interval;

              countdownInterval = setInterval(() => {
                  remainingTime -= 1000;
                  if (remainingTime <= 0) {
                      remainingTime = interval;
                  }
                  document.getElementById('countdown').textContent = `下一次拍照剩余 ${remainingTime / 1000} 秒`;
              }, 1000);
          }

          // 保存WiFi设置按钮事件
          document.getElementById('save-wifi-btn').addEventListener('click', () => {
              const wifiName = document.getElementById('wifi-name').value;
              const wifiPassword = document.getElementById('wifi-password').value;
              
              if (!wifiName || !wifiPassword) {
                  alert('请填写WiFi名称和密码');
                  return;
              }
              
              fetch('/save_wifi', {
                  method: 'POST',
                  headers: {
                      'Content-Type': 'application/json',
                  },
                  body: JSON.stringify({ name: wifiName, password: wifiPassword }),
              })
                  .then(response => response.text())
                  .then(data => {
                      document.getElementById('wifi-status').textContent = data;
                  })
                  .catch(error => {
                      console.error('Error:', error);
                      document.getElementById('wifi-status').textContent = '错误：无法保存WiFi设置';
                  });
          });

          // 保存蓝牙设置按钮事件
          document.getElementById('save-bluetooth-btn').addEventListener('click', () => {
              const bluetoothName = document.getElementById('bluetooth-name').value;
              const keySelect = document.getElementById('key-select').value;
              
              if (!bluetoothName) {
                  alert('请填写蓝牙名称');
                  return;
              }
              
              fetch('/save_bluetooth', {
                  method: 'POST',
                  headers: {
                      'Content-Type': 'application/json',
                  },
                  body: JSON.stringify({ name: bluetoothName, key: keySelect }),
              })
                  .then(response => response.text())
                  .then(data => {
                      document.getElementById('bluetooth-status').textContent = data;
                  })
                  .catch(error => {
                      console.error('Error:', error);
                      document.getElementById('bluetooth-status').textContent = '错误：无法保存蓝牙设置';
                  });
          });

          // 心跳功能
          function updateHeartbeat() {
              fetch('/heartbeat')
                  .then(response => response.text())
                  .then(data => {
                      if (data === 'alive') {
                          updateBluetoothIndicator();
                      } else {
                          document.getElementById('bluetooth-indicator').classList.remove('connected');
                          updateButtonState(false);
                      }
                  })
                  .catch(error => {
                      console.error('Error:', error);
                      document.getElementById('bluetooth-indicator').classList.remove('connected');
                      updateButtonState(false);
                  });
          }

          // 每30秒发送一次心跳请求
          setInterval(updateHeartbeat, 30000);

          // 蓝牙连接状态指示器
          function updateBluetoothIndicator() {
              fetch('/bluetooth_status')
                  .then(response => response.text())
                  .then(data => {
                      const indicator = document.getElementById('bluetooth-indicator');
                      if (data === 'connected') {
                          indicator.classList.add('connected');
                          updateButtonState(true);
                      } else {
                          indicator.classList.remove('connected');
                          updateButtonState(false);
                      }
                  })
                  .catch(error => {
                      console.error('Error:', error);
                      document.getElementById('bluetooth-indicator').classList.remove('connected');
                      updateButtonState(false);
                  });
          }

          // 每5秒更新一次蓝牙连接状态
          setInterval(updateBluetoothIndicator, 5000);

          // 页面加载时立即更新一次蓝牙连接状态
          window.onload = function() {
              updateHeartbeat();
              updateBluetoothIndicator();
              
              // 获取保存的设置
              fetch('/get_settings')
                  .then(response => response.json())
                  .then(data => {
                      if (data.wifiName) {
                          document.getElementById('wifi-name').value = data.wifiName;
                      }
                      if (data.wifiPassword) {
                          document.getElementById('wifi-password').value = data.wifiPassword;
                      }
                      if (data.bluetoothName) {
                          document.getElementById('bluetooth-name').value = data.bluetoothName;
                      }
                      if (data.selectedKey) {
                          document.getElementById('key-select').value = data.selectedKey;
                          document.getElementById('current-key').textContent = `当前设置控制按键为：${getKeyText(data.selectedKey)}`;
                      }
                  })
                  .catch(error => {
                      console.error('Error:', error);
                  });
          };

          // 获取按键文本
          function getKeyText(keyValue) {
              const keyMap = {
                  'KEY_MEDIA_VOLUME_UP': '音量+',
                  'KEY_MEDIA_VOLUME_DOWN': '音量-',
                  'KEY_ESC': 'ESC',
                  'KEY_UP_ARROW': '上箭头',
                  'KEY_DOWN_ARROW': '下箭头',
                  'KEY_LEFT_ARROW': '左箭头',
                  'KEY_RIGHT_ARROW': '右箭头',
                  'KEY_PAGE_UP': 'PgUp',
                  'KEY_PAGE_DOWN': 'PgDn',
                  'KEY_TAB': 'Tab',
                  'KEY_SHIFT': 'Shift',
                  'KEY_CONTROL': 'Ctrl',
                  'KEY_ALT': 'Alt',
                  'KEY_F1': 'F1',
                  'KEY_F2': 'F2',
                  'KEY_F3': 'F3',
                  'KEY_F4': 'F4',
                  'KEY_F5': 'F5',
                  'KEY_F6': 'F6',
                  'KEY_F7': 'F7',
                  'KEY_F8': 'F8',
                  'KEY_F9': 'F9',
                  'KEY_F10': 'F10',
                  'KEY_F11': 'F11',
                  'KEY_F12': 'F12'
              };
              return keyMap[keyValue] || keyValue;
          }
      </script>
  </body>
  </html>
  )rawliteral";
  return html;
}

void setup() {
  Serial.begin(115200);
  preferences.begin("settings", false);
  loadSettings();
  bleKeyboard.begin();
  bleKeyboard.releaseAll();
  setupWiFi();
  setupServer();
  setupLed(); // 初始化LED
}

void loadSettings() {
  savedWifiName = preferences.getString("wifiName", "");
  savedWifiPassword = preferences.getString("wifiPassword", "");
  savedBleName = preferences.getString("bleName", "ESP32-C3");
  selectedKey = preferences.getString("selectedKey", "KEY_MEDIA_VOLUME_UP");
  bleKeyboard.setName(savedBleName.c_str());
}

void loop() {
  server.handleClient();
  
  // 更新LED状态
  updateLed();
  
  // 连拍逻辑
  if (isContinuousShooting && bleKeyboard.isConnected()) {
    unsigned long currentTime = millis();
    if (currentTime - lastPhotoTime >= photoInterval) {
      sendKeyPress(selectedKey);
      lastPhotoTime = currentTime;
      // 连拍时调整LED闪烁间隔
      ledInterval = 500; // 0.5秒闪烁一次
    }
  } else {
    // 非连拍时恢复默认闪烁间隔
    ledInterval = 1000; // 1秒闪烁一次
  }
}
