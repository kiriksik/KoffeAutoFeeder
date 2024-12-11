#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsClient.h>
#include <Servo.h>
// Серво
Servo servo;
const int servoPin = 5; // Пин сервопривода

// Начальный вайфай
const char* ssidAP = "AutoFeederSetup";
const char* passwordAP = "12345678";

String wifiSSID = "";
String wifiPassword = "";

AsyncWebServer server(80);
WebSocketsClient webSocket;

unsigned long feedingInterval = 3600000; // в миллисекундах
unsigned long lastFeedingTime = 0;

const char* serverHost = "192.168.31.172";
const int serverPort = 5000;
const char* deviceID = "feeder_001";

unsigned long lastPingTime = 0;
unsigned long pingInterval = 30000; // Интервал пинга


void feedAnimal() {
  servo.write(90);
  delay(1000);
  servo.write(0);
}


void saveWiFiCredentials(const String& ssid, const String& password) {
  EEPROM.begin(512);
  for (size_t i = 0; i < ssid.length(); i++) {
    EEPROM.write(i, ssid[i]);
  }
  EEPROM.write(ssid.length(), '\0');
  for (size_t i = 0; i < password.length(); i++) {
    EEPROM.write(32 + i, password[i]);
  }
  EEPROM.write(32 + password.length(), '\0');
  EEPROM.commit();
}


void loadWiFiCredentials(String& ssid, String& password) {
  EEPROM.begin(512);
  char ssidBuff[32];
  char passBuff[32];
  for (size_t i = 0; i < 32; i++) {
    ssidBuff[i] = EEPROM.read(i);
    passBuff[i] = EEPROM.read(32 + i);
  }
  ssid = String(ssidBuff);
  password = String(passBuff);
}


void handleWebSocketMessage(const String& message) {
  if (message == "feed") {
    feedAnimal();
  } else {
    Serial.println("Получено сообщение: " + message);
  }
}


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("[WebSocket] Disconnected!");
            break;
        case WStype_CONNECTED:
            Serial.println("[WebSocket] Connected to server!");
            //"identify"
            webSocket.sendTXT("{\"device_id\":\"feeder_001\"}");
            break;
        case WStype_TEXT:
            Serial.printf("[WebSocket] Message: %s\n", payload);
            break;
        default:
            break;
    }
}


void handleConfig(AsyncWebServerRequest *request) {
  if (request->hasParam("ssid") && request->hasParam("password")) {
    wifiSSID = request->getParam("ssid")->value();
    wifiPassword = request->getParam("password")->value();
    saveWiFiCredentials(wifiSSID, wifiPassword);
    WiFi.softAPdisconnect(true);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    request->send(200, "text/html; charset=utf-8", "<b>Устройство сейчас будет перезагруженно...<\b>");
  }
  else {
    request->send(200, "text/html; charset=utf-8", "<form action=\"/config\" method=\"get\">SSID:<input name=\"ssid\"><br>Password:<input name=\"password\" type=\"password\"><br><input type=\"submit\" value=\"Save\"></form>");
  }
}


void setup() {

  servo.attach(servoPin);
  servo.write(0);

  loadWiFiCredentials(wifiSSID, wifiPassword);
  if (wifiSSID.length() > 0 && wifiPassword.length() > 0) {
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  }
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nПодключено!");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nНе удалось подключиться к Wi-Fi.");
  }

  if (attempts > 19) {
    WiFi.disconnect(true);
    delay(500);
    WiFi.softAP(ssidAP, passwordAP);
    delay(500);
  }
  else {
    webSocket.begin(serverHost, serverPort, "/socket.io/");
    webSocket.onEvent(webSocketEvent);
  }

  Serial.begin(115200);
  Serial.println("Точка доступа запущена. Подключитесь к сети AutoFeederSetup.");


  server.on("/config", HTTP_GET, handleConfig);


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<h1>Подключение кормушки</h1>";
    html += "<a href=\"/config\">Настройки сети</a><br>";
    request->send(200, "text/html; charset=utf-8", html);
  });


  server.begin();
}


void loop() {
    webSocket.loop();

    // Пример отправки данных каждые 5 секунд
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime > 5000) {
        lastSendTime = millis();
        if (webSocket.isConnected()) {
            webSocket.sendTXT("{\"device_id\":\"feeder_001\", \"status\":\"ok\"}");
        }
    }
}