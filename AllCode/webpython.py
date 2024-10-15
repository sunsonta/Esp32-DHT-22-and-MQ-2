import json
import time
import requests
from flask import Flask, jsonify, request, render_template
import os
import sqlite3
import logging
from flask_cors import CORS

# Create Flask app
app = Flask(__name__)
CORS(app)

# Line Notify Token (replace with your actual token)
LINE_NOTIFY_TOKEN = 'LINE_NOTIFY_TOKEN'#change

# Database file path
DATABASE_FILE_PATH = 'sensor_data.db'

# Set up logging
logging.basicConfig(level=logging.DEBUG)

# Function to get a new database connection
def get_db_connection():
    conn = sqlite3.connect(DATABASE_FILE_PATH)
    conn.row_factory = sqlite3.Row
    return conn

# Create sensor data table if it doesn't exist
def initialize_database():
    with get_db_connection() as conn:
        c = conn.cursor()
        c.execute('''CREATE TABLE IF NOT EXISTS sensor_data
                     (timestamp TEXT, sensor_name TEXT, value REAL)''')
        c.execute("CREATE INDEX IF NOT EXISTS idx_timestamp ON sensor_data (timestamp)")
        c.execute('''CREATE TABLE IF NOT EXISTS last_sensor_values
                     (sensor_name TEXT PRIMARY KEY, value REAL, timestamp TEXT)''')
        conn.commit()

initialize_database()

# Function to send message to Line Notify
def send_line_notify(message):
    url = 'https://notify-api.line.me/api/notify'
    headers = {
        'Authorization': f'Bearer {LINE_NOTIFY_TOKEN}'
    }
    data = {'message': message}
    requests.post(url, headers=headers, data=data)

# Initialize the danger condition timers, cooldown timers, and notification timers
danger_timers = {
    "CO_400_4min": None,
    "CO_150_10min": None,
    "CO_1000_5sec": None,
    "Smoke_350_4min": None,
    "Smoke_1000_5sec": None,
    "LPG_500_3min": None,
    "LPG_1000_5sec": None,
    "HeatIndex_54_68_3min": None,
    "HeatIndex_36_53_6min": None,
}

cooldown_timers = {key: None for key in danger_timers}
notify_timers = {key: None for key in danger_timers}

# Store sensor data received from ESP32
sensor_data = {}

# Function to calculate Heat Index (Celsius)
def calculate_heat_index(temp, humidity):
    c1 = -8.78469475556
    c2 = 1.61139411
    c3 = 2.33854883889
    c4 = -0.14611605
    c5 = -0.012308094
    c6 = -0.0164248277778
    c7 = 0.002211732
    c8 = 0.00072546
    c9 = -0.000003582
    HI = (c1 + (c2 * temp) + (c3 * humidity) + (c4 * temp * humidity) +
          (c5 * temp ** 2) + (c6 * humidity ** 2) + (c7 * temp ** 2 * humidity) +
          (c8 * temp * humidity ** 2) + (c9 * temp ** 2 * humidity ** 2))
    return round(HI, 2)

# Function to log sensor data in the database
def log_sensor_data(sensor_type, value):
    current_time = time.strftime("%Y-%m-%d %H:%M:%S")
    with get_db_connection() as conn:
        c = conn.cursor()
        c.execute("INSERT INTO sensor_data (timestamp, sensor_name, value) VALUES (?, ?, ?)",
                  (current_time, sensor_type, value))
        c.execute("INSERT OR REPLACE INTO last_sensor_values (sensor_name, value, timestamp) VALUES (?, ?, ?)",
                  (sensor_type, value, current_time))
        conn.commit()

# Function to check danger conditions with cooldowns and notifications
def check_danger_conditions():
    alerts = []
    current_time = time.time()

    def check_cooldown(condition_name, value, threshold, danger_time, cooldown_time, display_name, line_message):
        if value >= threshold:
            if danger_timers[condition_name] is None:
                danger_timers[condition_name] = current_time
            if current_time - danger_timers[condition_name] >= danger_time:
                alerts.append(f"{display_name} exceeded threshold for required time!")
                if notify_timers[condition_name] is None or current_time - notify_timers[condition_name] >= 10:
                    send_line_notify(line_message)
                    notify_timers[condition_name] = current_time
                cooldown_timers[condition_name] = None
        else:
            if danger_timers[condition_name] is not None:
                if cooldown_timers[condition_name] is None:
                    cooldown_timers[condition_name] = current_time
                elif current_time - cooldown_timers[condition_name] >= cooldown_time:
                    danger_timers[condition_name] = None
                    alerts.append(f"{display_name} cooldown complete, alert reset.")
            else:
                cooldown_timers[condition_name] = None

    # Check CO conditions
    check_cooldown("CO_400_4min", sensor_data.get('co', 0), 400, 240, 60,
                   "CO >= 400 for 4 minutes", "กรุณาออกจากห้องปริมาณ CO สูงเกินกว่าค่าที่กำหนด")
    check_cooldown("CO_150_10min", sensor_data.get('co', 0), 150, 600, 60,
                   "CO >= 150 for 10 minutes", "กรุณาออกจากห้องปริมาณ CO สูงเกินกว่าค่าที่กำหนด")
    check_cooldown("CO_1000_5sec", sensor_data.get('co', 0), 1000, 5, 60,
                   "CO >= 1000 ppm for 5 seconds", "กรุณาออกจากห้องปริมาณ CO สูงเกินกว่าค่าที่กำหนด")

    # Check Smoke conditions
    check_cooldown("Smoke_350_4min", sensor_data.get('smoke', 0), 350, 240, 60,
                   "Smoke >= 350 for 4 minutes", "กรุณาออกจากห้องปริมาณควันสูงเกินกว่าค่าที่กำหนด")
    check_cooldown("Smoke_1000_5sec", sensor_data.get('smoke', 0), 1000, 5, 60,
                   "Smoke >= 1000 ppm for 5 seconds", "กรุณาออกจากห้องปริมาณควันสูงเกินกว่าค่าที่กำหนด")

    # Check LPG conditions
    check_cooldown("LPG_500_3min", sensor_data.get('lpg', 0), 500, 180, 60,
                   "LPG >= 500 for 3 minutes", "กรุณาออกจากห้องปริมาณ LPG สูงเกินกว่าค่าที่กำหนด")
    check_cooldown("LPG_1000_5sec", sensor_data.get('lpg', 0), 1000, 5, 60,
                   "LPG >= 1000 ppm for 5 seconds", "กรุณาออกจากห้องปริมาณ LPG สูงเกินกว่าค่าที่กำหนด")

    # Check Heat Index conditions
    if 54 <= sensor_data.get('heat_index', 0) <= 68:
        check_cooldown("HeatIndex_54_68_3min", sensor_data.get('heat_index', 0), 54, 180, 60,
                       "Heat Index between 54-68°C for 3 minutes", "กรุณาออกจากห้องอุณหภูมิห้องสูงเกินกว่าค่าที่กำหนด")
    elif 36 <= sensor_data.get('heat_index', 0) <= 53:
        check_cooldown("HeatIndex_36_53_6min", sensor_data.get('heat_index', 0), 36, 360, 60,
                       "Heat Index between 36-53°C for 6 minutes", "กรุณาออกจากห้องอุณหภูมิห้องสูงเกินกว่าค่าที่กำหนด")

    return alerts

# Route to serve the index.html page
@app.route('/')
def dashboard():
    return render_template('index.html')

# Route to receive real sensor data from ESP32
@app.route('/data', methods=['POST'])
def receive_data():
    global sensor_data
    data = request.get_json()  # Get JSON data from the request
    logging.debug(f"Received Data: {data}")

    # Extract sensor values and store them
    temperature = data.get('temperature')
    humidity = data.get('humidity')
    lpg = data.get('lpg')
    co = data.get('co')
    smoke = data.get('smoke')

    # Calculate the heat index based on the received temperature and humidity
    heat_index = calculate_heat_index(temperature, humidity)

    # Store the sensor data, including the calculated heat index
    sensor_data.update({
        'temperature': temperature,
        'humidity': humidity,
        'heat_index': heat_index,
        'lpg': lpg,
        'co': co,
        'smoke': smoke,
    })

    # Log the sensor data for plotting
    log_sensor_data('temperature', temperature)
    log_sensor_data('humidity', humidity)
    log_sensor_data('lpg', lpg)
    log_sensor_data('co', co)
    log_sensor_data('smoke', smoke)
    log_sensor_data('heat_index', heat_index)

    # Check danger conditions and send notifications if necessary
    alerts = check_danger_conditions()

    return jsonify({"status": "success", "message": "Data received successfully!", "alerts": alerts}), 200

# API to provide real-time data for the selected sensor
@app.route('/api/sensor_data')
def api_sensor_data():
    sensor = request.args.get('sensor')
    with get_db_connection() as conn:
        c = conn.cursor()
        c.execute("SELECT timestamp, value FROM sensor_data WHERE sensor_name = ? AND timestamp >= datetime('now', '-24 hours')", (sensor,))
        data = c.fetchall()
    if data:
        result = [{"timestamp": row["timestamp"], "value": row["value"]} for row in data]
        return jsonify(result)
    else:
        return jsonify({"error": "No data found for this sensor"}), 404

# Route to provide all real-time sensor values
@app.route('/api/all_sensor_values', methods=['GET'])
def api_all_sensor_values():
    return jsonify({
        'temperature': sensor_data.get('temperature', 'N/A'),
        'humidity': sensor_data.get('humidity', 'N/A'),
        'heat_index': sensor_data.get('heat_index', 'N/A'),
        'lpg': sensor_data.get('lpg', 'N/A'),
        'co': sensor_data.get('co', 'N/A'),
        'smoke': sensor_data.get('smoke', 'N/A')
    }), 200


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=2777, debug=False)