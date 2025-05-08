import time
from flask import Flask, render_template, request, session, redirect, url_for
from flask_socketio import SocketIO, emit, join_room, leave_room
from functools import wraps
from werkzeug.security import generate_password_hash, check_password_hash
import eventlet
import eventlet.wsgi

app = Flask(__name__)
app.config['SECRET_KEY'] = 'your_secret_key'
socketio = SocketIO(app, cors_allowed_origins="*")

FRAME_REQUEST_INTERVAL = 0.3
INACTIVITY_TIMEOUT = 60  # секунд


class DeviceManager:
    def __init__(self):
        self.device_credentials = {}
        self.browsers = {}
        self.devices = {}
        self.feeding_intervals = {}
        self.device_id_for_browser = {}
        self.device_last_activity_time = {}
        self.last_frame_request_time = {}
        self.last_frame = {}

    def register_device(self, device_id, sid, password, interval):
        self.devices[device_id] = sid
        self.device_credentials[device_id] = password
        self.feeding_intervals[device_id] = interval // 60000
        self.device_last_activity_time[device_id] = time.time()

    def register_browser(self, sid, device_id):
        self.browsers[sid] = True
        if device_id:
            self.device_id_for_browser[sid] = device_id
            self.device_last_activity_time[sid] = time.time()

    def remove_device(self, device_id):
        self.devices.pop(device_id, None)
        self.device_credentials.pop(device_id, None)
        self.feeding_intervals.pop(device_id, None)
        self.device_last_activity_time.pop(device_id, None)
        self.last_frame.pop(device_id, None)
        self.last_frame_request_time.pop(device_id, None)

    def remove_browser(self, sid):
        self.browsers.pop(sid, None)
        self.device_last_activity_time.pop(sid, None)
        self.device_id_for_browser.pop(sid, None)

    def cleanup(self):
        now = time.time()
        for device_id, sid in list(self.devices.items()):
            last_seen = self.device_last_activity_time.get(device_id, 0)
            if now - last_seen > INACTIVITY_TIMEOUT:
                self.remove_device(device_id)
                print(f"Устройство {device_id} удалено за неактивность")


manager = DeviceManager()


def login_required(f):
    @wraps(f)
    def decorated_function(*args, **kwargs):
        if not session.get('logged_in'):
            return redirect(url_for('login'))
        return f(*args, **kwargs)
    return decorated_function


@app.route('/login')
def login_page():
    return render_template('login.html')


@app.route('/logout', methods=['POST'])
def logout():
    session.clear()
    return redirect(url_for('login'))


@app.route('/login', methods=['POST'])
def login():
    device_id = request.json.get('device_id')
    password = request.json.get('password')

    if device_id in manager.device_credentials and check_password_hash(manager.device_credentials[device_id], password):
        session['logged_in'] = True
        session['device_id'] = device_id
        return {"message": "Авторизация успешна"}, 200
    else:
        return {"message": "Неверный логин или пароль"}, 401


@app.route('/')
@login_required
def index():
    device_id = session.get('device_id')
    return render_template('index.html')


@app.route('/feed', methods=['POST'])
@login_required
def feed():
    device_id = session.get('device_id')
    if device_id in manager.devices:
        socketio.emit('command', {'action': 'feed'}, room=manager.devices[device_id])
        manager.device_last_activity_time[device_id] = time.time()
        return "Команда отправлена!"
    else:
        return "Устройство не подключено.", 404


@app.route('/set_interval', methods=['POST'])
@login_required
def set_interval():
    try:
        interval = int(request.json.get('interval'))
        if interval <= 0:
            raise ValueError
    except (TypeError, ValueError):
        return {"message": "Некорректный интервал"}, 400

    device_id = session.get('device_id')
    if device_id in manager.devices:
        manager.feeding_intervals[device_id] = interval
        socketio.emit('command', {'action': 'set_interval', 'interval': interval}, room=manager.devices[device_id])
        return f"Интервал кормления для {device_id} установлен на {interval} минут."
    else:
        return "Устройство не подключено.", 404



@app.route('/stream')
@login_required
def stream():
    device_id = session.get('device_id')
    return render_template('stream.html', device_id=device_id)


@socketio.on('connect')
def handle_connect():
    print("Устройство или браузер подключено:", request.sid)


@socketio.on('disconnect')
def handle_disconnect():
    sid = request.sid
    if sid in manager.browsers:
        manager.remove_browser(sid)
        leave_room('browsers')
        print("Браузер отключен")
    else:
        for device_id, dev_sid in list(manager.devices.items()):
            if dev_sid == sid:
                manager.remove_device(device_id)
                leave_room('devices')
                print(f"Устройство отключено: {device_id}")


@socketio.on('identify')
def handle_identify(data):
    if not isinstance(data, dict):
        emit('error', {'message': 'Некорректные данные'})
        return

    client_type = data.get('type')
    device_id = data.get('device_id')
    password = data.get('password')
    feeding_interval = data.get('feeding_interval', 60000)

    if client_type not in ('device', 'browser'):
        emit('error', {'message': 'Неизвестный тип клиента'})
        return

    if client_type == 'device':
        if not all([device_id, password]) or not isinstance(feeding_interval, (int, str)):
            emit('error', {'message': 'Отсутствуют обязательные параметры для устройства'})
            return

        try:
            feeding_interval = int(feeding_interval)
        except ValueError:
            emit('error', {'message': 'Интервал должен быть числом'})
            return

        manager.register_device(device_id, request.sid, generate_password_hash(password), feeding_interval)
        join_room('devices')
        print(f"Устройство зарегистрировано: {device_id} {feeding_interval}")

    elif client_type == 'browser':
        manager.register_browser(request.sid, device_id)
        join_room('browsers')
        print("Браузер подключен")



@socketio.on('request_status')
def send_feeding_status():
    device_id = session.get('device_id')
    if not device_id:
        return

    interval_minutes = int(manager.feeding_intervals.get(device_id, 60))
    last_feed_time = manager.device_last_activity_time.get(device_id)

    if last_feed_time is None:
        last_feed_time = time.time()
        manager.device_last_activity_time[device_id] = last_feed_time

    time_since_last = time.time() - last_feed_time
    time_left_ms = max(interval_minutes * 60 * 1000 - int(time_since_last * 1000), 0)

    emit('feeding_status', {
        'interval': interval_minutes,
        'time_left_ms': time_left_ms
    })


@socketio.on("camera_frame")
def handle_camera_frame(data):
    if not isinstance(data, dict):
        emit("error", {"message": "Некорректные данные"})
        return

    device_id = data.get('device_id')
    image = data.get('image')

    if not isinstance(device_id, str) or not isinstance(image, str):
        emit("error", {"message": "Неверный формат изображения или ID устройства"})
        return

    manager.last_frame[device_id] = image

    for browser_sid, browser_device_id in manager.device_id_for_browser.items():
        if browser_device_id == device_id:
            if browser_sid in manager.device_last_activity_time:
                if time.time() - manager.device_last_activity_time[browser_sid] < 2:
                    socketio.emit("new_frame", {"image": image}, room=browser_sid)
                else:
                    manager.remove_browser(browser_sid)



@socketio.on("get_frame")
def send_frame_to_client():
    device_id = session.get('device_id')
    if not device_id or device_id not in manager.devices:
        emit("error", {"message": "Устройство не подключено."})
        return

    current_time = time.time()
    last_time = manager.last_frame_request_time.get(device_id, 0)

    if current_time - last_time < FRAME_REQUEST_INTERVAL:
        return

    manager.last_frame_request_time[device_id] = current_time
    manager.device_last_activity_time[request.sid] = current_time

    socketio.emit('command', {'action': 'get_frame'}, room=manager.devices[device_id])


@socketio.on('smth')
def handle_smth(data):
    info = data.get('info')
    if not isinstance(info, str):
        emit('error', {'message': 'Поле "info" должно быть строкой'})
        return

    print(f"Устройство что то сделало:", info)


if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=2222, debug=False)

