import serial
import time

SERIAL_PORT = "/dev/ttyACM0"
BAUD_RATE = 115200

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
last = time.time()

while True:
    line = ser.readline()
    if line:
        print(line.decode('utf-8', errors='ignore').strip())

    now = time.time()
    if now - last > 10:
        ser.write(b"beacon sniffer\r\n")
        print("---- Sent: beacon sniffer ----")
        last = now