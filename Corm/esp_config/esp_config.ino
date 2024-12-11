#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Servo.h>

// Серво
Servo servo;
const int servoPin = 5; // Сервопривод

// Начальный вайфай
const char* ssidAP = "AutoFeederSetup";
const char* passwordAP = "12345678";

String wifiSSID = "";
String wifiPassword = "";

AsyncWebServer server(80);

unsigned long feedingInterval = 3600000; // Интервал между кормлениями (в миллисекундах)
unsigned long lastFeedingTime = 0;


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
  EEPROM.write(ssid.length(), '\0'); // Конец строки
  for (size_t i = 0; i < password.length(); i++) {
    EEPROM.write(32 + i, password[i]);
  }
  EEPROM.write(32 + password.length(), '\0'); // Конец строки
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
  // Настройка сервопривода
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

  Serial.begin(115200);
  Serial.println("Точка доступа запущена. Подключитесь к сети AutoFeederSetup.");


  server.on("/config", HTTP_GET, handleConfig);


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<h1>Управление автокормушкой</h1>";
    html += "<form action=\"/setInterval\" method=\"get\">Интервал кормления (в часах):<input name=\"interval\"><input type=\"submit\" value=\"Сохранить\"></form>";
    html += "<a href=\"/feed\">Высыпать порцию корма</a><br>";
    html += "<a href=\"/stream\">Посмотреть видео</a><br>";
    html += "<a href=\"/config\">Настройки сети</a><br>";
    request->send(200, "text/html; charset=utf-8", html);
  });


  server.on("/setInterval", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("interval")) {
      feedingInterval = request->getParam("interval")->value().toInt() * 3600000;
      request->send(200, "text/plain; charset=utf-8", "Интервал кормления изменен.");
    } else {
      request->send(400, "text/plain; charset=utf-8", "Ошибка: параметр interval не найден.");
    }
  });


  server.on("/feed", HTTP_GET, [](AsyncWebServerRequest *request) {
    feedAnimal();
    request->send(200, "text/plain; charset=utf-8", "Порция корма высыпана.");
  });


  server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain; charset=utf-8", "Здесь будет стрим с камеры.");
  });


  server.begin();
}

void loop() {
  // Автоматическая подача корма
  if (millis() - lastFeedingTime > feedingInterval) {
    feedAnimal();
    lastFeedingTime = millis();
  }
}
