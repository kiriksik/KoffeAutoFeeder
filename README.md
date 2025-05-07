# 🐾 Система управления кормушками

**KoffeAutoFeeder** — это кормушка с видеопотоком, управляемая через веб-интерфейс. Устройство основано на STM32-CAM и взаимодействует с сервером через Socket.IO.

---

## ⚙️ Используемые компоненты

### Аппаратная часть (Кормушка)
- **STM32-CAM** — микроконтроллер с камерой, выступающий в роли основного управляющего устройства
- **Сервопривод** — управляет механизмом подачи корма
- **Питание** — *will be soon*

📁 [Исходный код устройства](https://github.com/kiriksik/KoffeAutoFeeder/blob/main/Corm/esp_config/esp_config.ino)

Используемые библиотеки:
- `EEPROM` — для хранения параметров Wi-Fi
- `SocketIOclient` — для подключения к серверу через Socket.IO
- `ESPAsyncWebServer` — облегчённый веб-сервер на устройстве
- `ArduinoJson` — для формирования и разбора сообщений
- `ESP32Servo` — управление сервоприводом
- `esp_camera` — потоковое видео с камеры

---

## 🌐 Сервер

📁 [Исходный код сервера](https://github.com/kiriksik/KoffeAutoFeeder/blob/main/Server_ESP/Server_ESPconvertor/server.py)

Функции сервера:
- Авторизация устройств и пользователей
- Управление интервалом подачи корма
- Приём и трансляция видео с устройства в браузер
- Отправка управляющих команд на кормушку

Технологии:
- **Flask** — основной web-фреймворк
- **Flask-SocketIO** — для двустороннего общения с кормушкой
- **Eventlet** — асинхронная обработка сокетов
- **Bootstrap** — стилизация веб-интерфейса

---

## 📦 Установка и запуск
> Убедитесь, что установлен [Docker](https://docs.docker.com/get-docker/) и [Docker Compose](https://docs.docker.com/compose/).

### 1. Клонирование репозитория

```bash
git clone https://github.com/kiriksik/KoffeAutoFeeder.git
cd KoffeAutoFeeder/Server_ESP/Server_ESPconvertor
```

### 2. Запуск сервера

```bash
docker-compose up --build
```

#### Сервер будет доступен по адресу: http://localhost:2222

---

## 🧩 Структура проекта

```
KoffeAutoFeeder/
├── Corm/
│   ├── esp_config/
│   │   └── esp_config.ino # Код прошивки ESP32-CAM
│   └── Local_corm/ # Старая локальная версия кормушки для Arduino
├── Server_ESP/
│   └── Server_ESPconvertor/
│       ├── templates/ # HTML-шаблоны веб-интерфейса
│       ├── server.py # Основной серверный скрипт
│       ├── requirements.txt # Зависимости Python
│       ├── Dockerfile # Образ сервера
│       └── docker-compose.yml # Компоновка сервера в контейнере
└── README.md # Этот файл
```