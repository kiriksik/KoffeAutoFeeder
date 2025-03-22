import time

from flask import Flask, render_template, request, session, redirect, url_for
from flask_socketio import SocketIO, emit
from flask_socketio import join_room, leave_room
import json
from functools import wraps

from pyexpat.errors import messages

app = Flask(__name__)
app.config['SECRET_KEY'] = 'your_secret_key'
socketio = SocketIO(app, cors_allowed_origins="*")

device_credentials = {}
browsers = {}
devices = {}
feeding_intervals = {}
urls_to_cams = {}
device_id_for_browser = {}
device_last_activity_time = {}

last_frame = None


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

    if device_id in device_credentials and device_credentials[device_id] == password:
        session['logged_in'] = True
        session['device_id'] = device_id
        return {"message": "Авторизация успешна"}, 200
    else:
        return {"message": "Неверный логин или пароль"}, 401


@app.route('/')
@login_required
def index():
    return render_template('index.html')


@app.route('/feed', methods=['POST'])
@login_required
def feed():
    device_id = session.get('device_id')
    if device_id in devices:
        socketio.emit('command', {'action': 'feed'}, room=devices[device_id])
        return "Команда отправлена!"
    else:
        return "Устройство не подключено.", 404


@app.route('/set_interval', methods=['POST'])
@login_required
def set_interval():
    device_id = session.get('device_id')
    interval = request.json.get('interval')
    if device_id in devices:
        feeding_intervals[device_id] = interval
        socketio.emit('command', {'action': 'set_interval', 'interval': interval}, room=devices[device_id])
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
    print("Новое соединение")
    emit('status', {'message': 'Connected to server'})


@socketio.on('identify')
def handle_identify(data):
    client_type = data.get('type')
    device_id = data.get('device_id')
    password = data.get('password')
    feeding_interval = data.get('feeding_interval')
    url_to_cam = data.get('url_to_cam')

    if client_type == 'device' and device_id:
        if device_id is not None and url_to_cam is not None and feeding_interval is not None:
            devices[device_id] = request.sid
            device_credentials[device_id] = password
            feeding_intervals[device_id] = feeding_interval // 60000
            urls_to_cams[device_id] = url_to_cam
            join_room('devices')
            print(f"Устройство зарегистрировано: {device_id} {feeding_interval} {url_to_cam}")
    elif client_type == 'browser':
        browsers[request.sid] = True
        if device_id:

            device_id_for_browser[request.sid] = device_id
            device_last_activity_time[request.sid] = time.time()
        join_room('browsers')
        print("Браузер подключен")


@socketio.on('connect')
def on_connect():
    print("Устройство подключено:", request.sid)


@socketio.on("camera_frame")
def handle_camera_frame(data):
    device_id = data.get('device_id')
    last_frame = data.get('image')

    for browser_sid, browser_device_id in device_id_for_browser.items():
        if browser_device_id == device_id:
            if device_last_activity_time[browser_sid] < (time.time() + 2):
                print(device_last_activity_time[browser_sid], "; ", time.time())
                socketio.emit("new_frame", {"image": last_frame}, room=browser_sid)
            else:
                del(device_last_activity_time[browser_sid])
                del(device_id_for_browser[browser_sid])
                del(browsers[browser_sid])



@socketio.on("get_frame")
def send_frame_to_client():
    device_id = session.get('device_id')
    if device_id:
        if devices[device_id]:
            device_last_activity_time[request.sid] = time.time()
            socketio.emit('command', {'action': 'get_frame'}, room=devices[device_id])


@socketio.on('disconnect')
def handle_disconnect():
    if request.sid in browsers:
        del browsers[request.sid]
        leave_room('browsers')
        print("Браузер отключен")
    elif request.sid in devices.values():
        device_id = next((k for k, v in devices.items() if v == request.sid), None)
        if device_id:
            del devices[device_id]
            leave_room('devices')
            print(f"Устройство отключено: {device_id}")


@socketio.on('smth')
def handle_smth(data):
    info = data.get('info')
    print(f"Устройство что то сделало:", info)


@socketio.on('disconnect')
def handle_disconnect():
    for device_id, sid in list(devices.items()):
        if sid == request.sid:
            del devices[device_id]
            print(f"Устройство отключено: {device_id}")


if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000)
