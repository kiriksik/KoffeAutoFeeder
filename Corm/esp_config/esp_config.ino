#include <EEPROM.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <Base64.h>

#define CONFIG_ESP32_SPIRAM_SUPPORT 1

#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22
#define LED_GPIO_NUM   4


// Серво
Servo feederServo;
const int servoPin = 2;  // Пин сервопривода
const int ledPin = 2;     // Пин встроенного светодиода

// Начальный Wi-Fi
// const char* ssidAP = "AutoFeederSetup";
const char* passwordAP = "12345678";

String wifiSSID = "";
String wifiPassword = "";

AsyncWebServer server(80);
WebSocketsClient webSocket;
SocketIOclient socketIO;

unsigned long feedingInterval = 3600000;  // в миллисекундах
unsigned long servoOpenTime = 1000; //в миллисекундах
unsigned long lastFeedingTime = 0;


const char* serverHost = "188.134.78.158";
const int serverPort = 2222;
const char* deviceID = "feeder_003";
const char* password = "123123123";

unsigned long lastPingTime = 0;
unsigned long pingInterval = 30000;  // Интервал пинга


int trying = 0;
bool busy = false;

void EEPROM_writeULong(int address, unsigned long value) {
  for (int i = 0; i < 4; i++) {
    EEPROM.write(address + i, (value >> (8 * i)) & 0xFF);
  }
}

unsigned long EEPROM_readULong(int address) {
  unsigned long value = 0;
  for (int i = 0; i < 4; i++) {
    value |= ((unsigned long)EEPROM.read(address + i)) << (8 * i);
  }
  return value;
}




void feedAnimal() {

  feederServo.attach(servoPin);
  Serial.println("Opening feeder...");
  feederServo.write(0);  
  delay(100);
  feederServo.write(90);
  delay(servoOpenTime);
  Serial.println("Closing feeder...");
  feederServo.write(0); 
  delay(500);
  feederServo.detach();
}

camera_fb_t* getCameraFrameWithTimeout(unsigned long timeout = 3000) {
  unsigned long start = millis();
  camera_fb_t* fb = nullptr;

  while ((millis() - start) < timeout) {
    fb = esp_camera_fb_get();
    if (fb != nullptr) {
      return fb;
    }
    delay(100);
  }

  Serial.println("Камера не ответила за отведённое время!");
  return nullptr;
}

void sendImage(camera_fb_t* fb) {
  String base64Image = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);
  sendImageToServer(base64Image);

}

void sendImageToServer(String imageData) {
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  array.add("camera_frame");
  JsonObject param = array.createNestedObject();
  if (imageData.length() > 100000) {
    esp_camera_fb_return(fb);
    Serial.println("Слишком большое изображение");
    return;
  }
  param["image"] = imageData;
  param["device_id"] = deviceID;

  String output;
  serializeJson(doc, output);
  socketIO.sendEVENT(output);
  Serial.println("[Socket.IO] Image sent to server.");
  doc.clear();
}

void saveFeedingInterval(unsigned long interval) {
  EEPROM.begin(512);
  EEPROM.writeULong(100, interval); // сохраняем с offset = 100
  EEPROM.commit();
  EEPROM.end();
}

unsigned long loadFeedingInterval() {
  EEPROM.begin(512);
  unsigned long interval = EEPROM.readULong(100);
  EEPROM.commit();
  EEPROM.end();
  return interval;
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
  EEPROM.end();
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
  EEPROM.commit();
  EEPROM.end();
}






void socketIOEvent(socketIOmessageType_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case sIOtype_DISCONNECT:
      Serial.println("[Socket.IO] Disconnected! Trying to reconnect...");
      socketIO.begin(serverHost, serverPort, "/socket.io/?EIO=4");
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
        param["password"] = password;
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
          
          // feedAnimal();
          trying++;
        } else if (command == "set_interval") {
          int interval = jsonObject["interval"];
          feedingInterval = interval * 60000;
          saveFeedingInterval(feedingInterval);
        } else if (command == "get_frame") {
          camera_fb_t* fb = getCameraFrameWithTimeout();
          for (int i = 0; i < 3 && fb == nullptr; i++) {
            fb = getCameraFrameWithTimeout();
            if (!fb) delay(100);
          }
          if (!fb) {
            Serial.println("Failed to capture image");
            return;
          }
          sendImage(fb);
        } else {
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
  feederServo.attach(servoPin);
  feederServo.write(0);
  // feederServo.detach();

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
  WiFi.softAP(deviceID, password);
  Serial.println("AP IP address: " + WiFi.softAPIP().toString());

  server.on("/config", HTTP_GET, handleConfig);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    String html = "<h1>AutoFeeder Setup</h1><a href=\"/config\">Configure Wi-Fi</a><br>";
    request->send(200, "text/html; charset=utf-8", html);
  });

  server.begin();

camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    Serial.println("Psram Avaliable");
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 8;
    config.fb_count = 2;
  } else {
    Serial.println("Psram Not Avaliable");
    config.frame_size = FRAMESIZE_VGA;     // 640x480
    config.jpeg_quality = 8;
    config.fb_count = 2;

  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Ошибка инициализации камеры");
    return;
  }

  Serial.println("Камера инициализирована");

  feedingInterval = loadFeedingInterval();
  Serial.print("Интервал кормления сохранён: ");
  Serial.println(feedingInterval);

  if (WiFi.status() == WL_CONNECTED) {
    socketIO.begin(serverHost, serverPort, "/socket.io/?EIO=4");
    socketIO.onEvent(socketIOEvent);
  }

  

}


void loop() {
  // Serial.printf("[MEM] Free heap: %u, PSRAM: %u\n", ESP.getFreeHeap(), ESP.getFreePsram());

  unsigned long currentMillis = millis();
  if (currentMillis - lastFeedingTime >= feedingInterval) {
      lastFeedingTime = currentMillis;

      feedAnimal();
  }
  if (trying >= 1) {
    feedAnimal();
    trying--;
  }

  socketIO.loop();
}
