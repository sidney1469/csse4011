import asyncio
import json
import math
import serial
import serial.tools.list_ports
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
BAUD_RATE = 115200
SERIAL_PORT = "/dev/tty.usbmodem1101"

NUM_NODES = 13
NODE_NAMES = ["NODE_A", "NODE_B", "NODE_C", "NODE_D", "NODE_E",
              "NODE_F", "NODE_G", "NODE_H", "NODE_I", "NODE_J",
              "NODE_K", "NODE_L", "NODE_M"]

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

@app.get("/get")
def get_fun():
    print("here")
    return 200

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    clients.add(websocket)
    print(f"Client connected — {len(clients)} total")
    try:
        await websocket.receive_text()
    except WebSocketDisconnect:
        pass
    finally:
        clients.discard(websocket)
        print(f"Client disconnected — {len(clients)} total")

def parse_packet(line: str) -> dict | None:
    try:
        raw = json.loads(line)
        data = raw.get("data_buffer", [])
        data_len = len(data)

        if data_len != NUM_NODES:
            print(f"Unexpected data_len: {data_len}, skipping")
            return None
        
        nodes = []
        for i in range(data_len):
            rssi = data[i]
            nodes.append({
                "name": NODE_NAMES[i],
                "rssi": rssi
            })
        return {"nodes": nodes, "position": { "x": raw.get("pos_x", 0) / 100.0, 
                                             "y": raw.get("pos_y", 0) / 100.0, 
                                             "z": raw.get("pos_z", 0) / 100.0,
                                             }}
    except (json.JSONDecodeError, KeyError, TypeError) as e:
        print(f"Parse error: {e}")
        return None

async def broadcast(message: str):
    disconnected = set()
    for client in clients:
        try:
            await client.send_text(message)
        except:
            disconnected.add(client)
    clients.difference_update(disconnected)

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
        packet = parse_packet(decoded_line)
        if packet:
            await broadcast(json.dumps(packet))


@app.on_event("startup")
async def startup_event():
    asyncio.create_task(serial_reader())