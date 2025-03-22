**Используемые компоненты**
> - STM32-CAM - микроконтроллер с камерой, используемый как основная часть кормушки
> - Сервопривод - для механизма кормления питомца
> - Питание: *will be soon...*

**Сервер**

[Код](https://github.com/kiriksik/KoffeAutoFeeder/blob/main/Server_ESP/Server_ESPconvertor/server.py)

Используется:
- Flask для реализации Web-сервера
- SocketsIO для общения с кормушкой
- JSON для общения с кормушкой через SocketsIO
- Bootstrap для стилей Web-сервера


**Кормушка**

*Основное устройство*

[Код](https://github.com/kiriksik/KoffeAutoFeeder/blob/main/Corm/esp_config/esp_config.ino)

Используется:
- EEPROM для хранения информации о WiFi в энергонезависимой памяти
- SocketIOclient для связи с сервером с помощью SocketsIO
- ESPAsyncWebServer
- JsonArduino для формирования пакетов SocketsIO
- ESP32Servo для работы сервопривода
- esp_camera для работы с видеопотоком

