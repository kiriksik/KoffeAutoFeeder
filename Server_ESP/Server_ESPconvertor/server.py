from flask import Flask, render_template, request
from flask_socketio import SocketIO, emit
import json

app = Flask(__name__)
app.config['SECRET_KEY'] = 'your_secret_key'
socketio = SocketIO(app, cors_allowed_origins="*")


devices = {}
feeding_intervals = {}
urls_to_cams = {}


@app.route('/')
def index():
    return render_template('index.html')


@app.route('/feed', methods=['POST'])
def feed():
    device_id = request.json.get('device_id')
    if device_id in devices:
        socketio.emit('command', {'action': 'feed'}, room=devices[device_id])
        return "Команда отправлена!"
    else:
        return "Устройство не подключено.", 404


@app.route('/set_interval', methods=['POST'])
def set_interval():
    device_id = request.json.get('device_id')
    interval = request.json.get('interval')
    if device_id in devices:
        feeding_intervals[device_id] = interval
        socketio.emit('command', {'action': 'set_interval', 'interval': interval}, room=devices[device_id])
        return f"Интервал кормления для {device_id} установлен на {interval} минут."
    else:
        return "Устройство не подключено.", 404


@app.route('/stream')
def stream():
    return render_template('stream.html', stream_url='http://192.168.1.100:81/stream')


@socketio.on('connect')
def handle_connect():
    print("Новое соединение")
    emit('status', {'message': 'Connected to server'})


@socketio.on('identify')
def handle_identify(data):
    device_id = data.get('device_id')
    feeding_interval = data.get('feeding_interval')
    url_to_cam = data.get('url_to_cam')
    if device_id is not None and url_to_cam is not None and feeding_interval is not None:
        devices[device_id] = request.sid
        feeding_intervals[device_id] = feeding_interval // 60000
        urls_to_cams[device_id] = url_to_cam
        print(f"Устройство зарегистрировано: {device_id} {feeding_interval} {url_to_cam}")


# @socketio.on('status')
# def handle_status(data):
    # device_id = data.get('device_id')
    # if device_id:
    #     devices[device_id] = request.sid
    #     print(f"Устройство доступно:", request.sid)


@socketio.on('connect')
def on_connect():
    print("Устройство подключено:", request.sid)


@socketio.on('disconnect')
def on_disconnect():
    print("Устройство отключено:", request.sid)

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
