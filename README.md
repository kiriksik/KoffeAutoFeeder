**Используемые компоненты**
> - STM32 Wroom 32 - основная плата кормушки
> - STM32-CAM OV2640 - плата с камерой для кормушки
> - Сервопривод - для механизма кормления питомца
> - Провода, диоды и кнопки - для индикации и ручного взаимодействия

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


*Камера IN-DEV*

[Код]()
