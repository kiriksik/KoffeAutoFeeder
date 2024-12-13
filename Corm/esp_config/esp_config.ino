#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <Servo.h>
#include <ArduinoJson.h>

// Серво
Servo servo;
const int servoPin = 16; // Пин сервопривода (D1)
const int ledPin = 2; //Пин диода (D?)

// Начальный вайфай
const char* ssidAP = "AutoFeederSetup";
const char* passwordAP = "12345678";

String wifiSSID = "";
String wifiPassword = "";

AsyncWebServer server(80);
WebSocketsClient webSocket;
SocketIOclient  socketIO;

unsigned long feedingInterval = 3600000; // в миллисекундах
unsigned long lastFeedingTime = 0;

const char* serverHost = "192.168.31.172";
const int serverPort = 5000;
const char* deviceID = "feeder_001";

unsigned long lastPingTime = 0;
unsigned long pingInterval = 30000; // Интервал пинга

int trying = 0;

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


void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length) {
    switch(type) {
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
          param["feeding_interval"] = feedingInterval;
          param["url_to_cam"] = "someurl";

          String output;
          serializeJson(doc, output);

          socketIO.sendEVENT(output);
          break;
        }
        case sIOtype_EVENT: {
          Serial.println("[SocketIO] Received event:");

          String data;
          for (size_t i = 0; i < length; i++) {
              // trying += 1;
              data += (char)payload[i];
          }
          Serial.println(data);

          DynamicJsonDocument doc(1024);
          DeserializationError error = deserializeJson(doc, data);
          if (error) {
              trying += 10;
              Serial.print(F("JSON Parsing failed: "));
              Serial.println(error.c_str());
              return;
          }

          JsonObject jsonObject = doc[1].as<JsonObject>();
          String command = jsonObject["action"];
    
          if (command == "feed") {
              Serial.println("Команда: кормление!");
              trying += 1;
          } else if (command == "set_interval") {
            int interval = jsonObject["interval"];
            feedingInterval = interval * 60000;
          } else {
              Serial.println("Неизвестная команда: " + command);
          }
          DynamicJsonDocument doc2(1024);
          JsonArray array2 = doc2.to<JsonArray>();
          array2.add("smth");
          JsonObject param = array2.createNestedObject();
          param["info"] = command;

          String output2;
          serializeJson(doc2, output2);

          // Отправляем событие
          socketIO.sendEVENT(output2);
          break;
        }
        case sIOtype_ACK:
        {
          // Serial.println("[IOc] get ack: %u\n", length);
          hexdump(payload, length);
          break;
        }
        case sIOtype_ERROR:
            // Serial.println("[IOc] get error: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_BINARY_EVENT:
            // Serial.println("[IOc] get binary: %u\n", length);
            hexdump(payload, length);
            break;
        case sIOtype_BINARY_ACK:
            // Serial.println("[IOc] get binary ack: %u\n", length);
            hexdump(payload, length);
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
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
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
    // webSocket.beginSocketIO(serverHost, serverPort);
    // webSocket.onEvent(webSocketEvent);

    // socketIO.setReconnectInterval(10000);
    socketIO.begin(serverHost, serverPort, "/socket.io/?EIO=4");
    socketIO.onEvent(socketIOEvent);
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
    // webSocket.loop();
    if (trying > 1) {
      test();
      trying -= 1;
    }
    // feedAnimal();
    socketIO.loop();

    //5 секунд
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime > 5000) {
        lastSendTime = millis();
        if (socketIO.isConnected()) {
          // DynamicJsonDocument doc(1024);
          // JsonArray array = doc.to<JsonArray>();
          // array.add("identify");

          // JsonObject param1 = array.createNestedObject();
          // JsonObject param2 = array.createNestedObject();
          // param1["device_id"] = "feeder_001";
          // param2["status"] = "ok";
          // String output;
          // serializeJson(doc, output);
          // socketIO.sendEVENT(output);

        }
    }
}