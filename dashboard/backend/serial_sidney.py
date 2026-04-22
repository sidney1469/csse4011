import serial
import json

# Open a serial port (example for Windows 'COM3' or Linux '/dev/ttyUSB0')
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)

# Read data
while True:
    line = ser.readline().decode('utf-8').strip()
    print(line)
    #line_json = json.loads(line)
    #print(line_json)

# Always close the port when finished
ser.close()
