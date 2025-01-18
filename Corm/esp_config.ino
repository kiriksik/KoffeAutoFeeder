#include <EEPROM.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

#define CONFIG_ESP32_SPIRAM_SUPPORT 1

// #define RXD2 16
// #define TXD2 17


// Серво
Servo servo;
const int servoPin = 15;  // Пин сервопривода
const int ledPin = 2;     // Пин встроенного светодиода

// Начальный Wi-Fi
const char* ssidAP = "AutoFeederSetup";
const char* passwordAP = "12345678";

String wifiSSID = "";
String wifiPassword = "";

AsyncWebServer server(80);
WebSocketsClient webSocket;
SocketIOclient socketIO;

unsigned long feedingInterval = 3600000;  // в миллисекундах
unsigned long lastFeedingTime = 0;

const char* serverHost = "192.168.31.172";
const int serverPort = 5000;
const char* deviceID = "feeder_001";

unsigned long lastPingTime = 0;
unsigned long pingInterval = 30000;  // Интервал пинга

int trying = 0;
bool busy = false;

void feedAnimal() {
  servo.write(90);
  delay(1000);
  servo.write(0);
}

void test() {
  digitalWrite(ledPin, HIGH);
  delay(500);
  digitalWrite(ledPin, LOW);
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


void sendImageToServer(String imageData) {
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  array.add("camera_frame");
  JsonObject param = array.createNestedObject();
  param["image"] = imageData;

  String output;
  serializeJson(doc, output);
  socketIO.sendEVENT(output);
  Serial.println("[Socket.IO] Image sent to server.");
  doc.clear();
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

void socketIOEvent(socketIOmessageType_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case sIOtype_DISCONNECT:
      Serial.println("[Socket.IO] Disconnected!");
      break;
    case sIOtype_CONNECT:
      {
        socketIO.send(sIOtype_CONNECT, "/");
        Serial.println("[Socket.IO] Connected!");

        DynamicJsonDocument doc(1024);
        JsonArray array = doc.to<JsonArray>();
        array.add("identify");
        JsonObject param = array.createNestedObject();
        param["device_id"] = deviceID;
        param["type"] = "device";
        param["feeding_interval"] = feedingInterval;
        param["url_to_cam"] = "someurl";

        String output;
        serializeJson(doc, output);
        socketIO.sendEVENT(output);
        break;
      }
    case sIOtype_EVENT:
      {
        String data;
        for (size_t i = 0; i < length; i++) {
          data += (char)payload[i];
        }
        Serial.println("[SocketIO] Received event: " + data);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, data);
        if (error) {
          Serial.print("JSON Parsing failed: ");
          Serial.println(error.c_str());
          return;
        }

        JsonObject jsonObject = doc[1].as<JsonObject>();
        String command = jsonObject["action"];

        if (command == "feed") {
          Serial.println("Command: Feed!");
          feedAnimal();
        } else if (command == "set_interval") {
          int interval = jsonObject["interval"];
          feedingInterval = interval * 60000;
        } else if (command == "get_frame") {
          // // if (busy == true)
          // //   return;
          // // busy = true;
          // if (Serial2.available()) {






          //   String imageData = "";
          //   while (Serial2.available()) {
          //     char c = Serial2.read();
          //     imageData += c;
          //     if (imageData.endsWith("---END---")) {
          //       break;  // Конец данных от камеры
          //     }
          //   }
          //   Serial.println(imageData);
          //   // Убираем маркер конца
          //   imageData.replace("---END---", "");


          //       DynamicJsonDocument doc2(4096);
          //       JsonArray array2 = doc2.to<JsonArray>();
          //       array2.add("smth");
          //       JsonObject param = array2.createNestedObject();
          //       param["info"] = imageData;

          //       String output2;
          //       serializeJson(doc2, output2);

          //       socketIO.sendEVENT(output2);
          //   }
          //   else {
          //   DynamicJsonDocument doc2(1024);
          //   JsonArray array2 = doc2.to<JsonArray>();
          //   array2.add("smth");
          //   JsonObject param = array2.createNestedObject();
          //   param["info"] = "serial not avaliable";

          //   String output2;
          //   serializeJson(doc2, output2);

          //   socketIO.sendEVENT(output2);
          //   busy = false;
          //   }
          }
        // }
        else {
          Serial.println("Unknown command: " + command);
        }
        break;
      }
    default:
      break;
  }
}

void handleConfig(AsyncWebServerRequest* request) {
  if (request->hasParam("ssid") && request->hasParam("password")) {
    wifiSSID = request->getParam("ssid")->value();
    wifiPassword = request->getParam("password")->value();
    saveWiFiCredentials(wifiSSID, wifiPassword);
    WiFi.softAPdisconnect(true);
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    request->send(200, "text/html; charset=utf-8", "<b>Device will now reboot...</b>");
  } else {
    request->send(200, "text/html; charset=utf-8", "<form action=\"/config\" method=\"get\">SSID:<input name=\"ssid\"><br>Password:<input name=\"password\" type=\"password\"><br><input type=\"submit\" value=\"Save\"></form>");
  }
}


void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  servo.attach(servoPin);
  servo.write(0);

  Serial.begin(9600);
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
    Serial.println("\nConnected to Wi-Fi!");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi. Starting AP mode.");
  }
  WiFi.softAP(ssidAP, passwordAP);
  Serial.println("AP IP address: " + WiFi.softAPIP().toString());

  server.on("/config", HTTP_GET, handleConfig);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String html = "<h1>AutoFeeder Setup</h1><a href=\"/config\">Configure Wi-Fi</a><br>";
    request->send(200, "text/html; charset=utf-8", html);
  });
server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Image received");
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Собираем входящие данные в строку
    static String jsonData = "";  // Используем static для сохранения между вызовами
    jsonData += String((char*)data).substring(0, len);  // Добавляем поступившие данные

    if (index + len == total) { // Если все данные получены
      DynamicJsonDocument doc(8192);
      DeserializationError error = deserializeJson(doc, jsonData);

      if (error) {
        Serial.println("JSON Parsing failed!");
        jsonData = "";  // Очищаем для следующего запроса
        return;
      }

      // Извлекаем Base64-строку
      String base64Image = doc["image"].as<String>();

      // Вызываем функцию для отправки изображения на сервер
      sendImageToServer(base64Image);

      // Очистка jsonData для следующего запроса
      jsonData = "";
    }
  });

  server.begin();

  if (WiFi.status() == WL_CONNECTED) {
    socketIO.begin(serverHost, serverPort, "/socket.io/?EIO=4");
    socketIO.onEvent(socketIOEvent);
  }
}


void loop() {
  if (trying > 1) {
    test();
    trying--;
  }


  socketIO.loop();
}