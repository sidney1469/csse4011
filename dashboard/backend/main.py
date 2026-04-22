import asyncio
import json
import math
import serial
import serial.tools.list_ports
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

BAUD_RATE = 115200
SERIAL_PORT = "/dev/tty.usbmodem1101"

MEAS_POWER = -56   # RSSI at 1 metre
PATH_LOSS_EXP = 2.5 # Needs to be calibrated (higher for more obstructions)
                # ~2 for free space ~2.5 - 4 indoors

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

clients: set[WebSocket] = set()

def rssi_to_distance(rssi: int) -> float:
    if rssi == 0:
        return -1; # error
    distance = round(10 ** ((MEAS_POWER - rssi) / (10 * PATH_LOSS_EXP)),2)
    return distance

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    clients.add(websocket)
    print(f"Client connected — {len(clients)} total")
    try:
        await websocket.wait_for_disconnect()
    except WebSocketDisconnect:
        pass
    finally:
        clients.discard(websocket)
        print(f"Client disconnected — {len(clients)} total")

async def serial_reader():

    loop = asyncio.get_event_loop()

    print(f"Opening serial port {SERIAL_PORT} at {BAUD_RATE} baud...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except serial.SerialException as e:
        print(f"Failed to open serial port: {e}")
        print("Available ports:")
        for port in serial.tools.list_ports.comports():
            print(f"  {port.device}")
        return

    print("Serial port open, waiting for packets...")

    while True:
        line = await loop.run_in_executor(None, ser.readline)
        decoded_line = line.decode("utf-8", errors="ignore").strip()
        print(f"Raw: {decoded_line}")


@app.on_event("startup")
async def startup_event():
    asyncio.create_task(serial_reader())