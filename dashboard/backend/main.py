import asyncio
import json
import re
import serial
import serial.tools.list_ports
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

BAUD_RATE = 115200
SERIAL_PORT = "/dev/tty.usbmodem1101"

NUM_NODES = 13
NODE_NAMES = ['4011-A','4011-B','4011-C','4011-D','4011-E','4011-F',
                    '4011-G','4011-H','4011-I','4011-J','4011-K','4011-L','4011-M']

MEAS_POWER = -56
PATH_LOSS_EXP = 2.5

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)

clients: set[WebSocket] = set()
ser: serial.Serial = None

# In-memory beacon registry — populated from device on connect
beacon_registry: dict[str, dict] = {}


# ── Models ────────────────────────────────────────────────────────────────────

class Node(BaseModel):
    name: str
    mac: str
    major: int
    minor: int
    x: float
    y: float
    z: float
    left: str = ""
    right: str = ""


# ── Path loss ─────────────────────────────────────────────────────────────────

def rssi_to_distance(rssi: int) -> float:
    if rssi == 0:
        return 0.0
    return round(10 ** ((MEAS_POWER - rssi) / (10 * PATH_LOSS_EXP)), 2)


# ── Beacon view parser ────────────────────────────────────────────────────────
# Accumulates the multi-line shell output from "beacon view -a" into records.
#
# Expected block per beacon:
#   ----------------------------
#   Name:    4011-A
#   MAC:     F5:75:FE:85:34:67
#   Major:   2753
#   Minor:   32998
#   Pos:     (0.0, 0.0, 1.0)
#   Left:
#   Right:   4011-B

class BeaconViewParser:
    def __init__(self):
        self._current: dict = {}
        self._complete: list[dict] = []

    def feed(self, line: str) -> list[dict]:
        """Feed one decoded serial line. Returns list of completed beacon dicts (usually empty)."""
        completed = []

        if line.startswith("---"):
            # Separator — if we have a partial record, discard it (shouldn't happen)
            self._current = {}
            return []

        def field(prefix):
            if line.startswith(prefix):
                return line[len(prefix):].strip()
            return None

        if (v := field("Name:"))    is not None: self._current["name"]  = v
        if (v := field("MAC:"))     is not None: self._current["mac"]   = v
        if (v := field("Major:"))   is not None: self._current["major"] = int(v)
        if (v := field("Minor:"))   is not None: self._current["minor"] = int(v)
        if (v := field("Left:"))    is not None: self._current["left"]  = v
        if (v := field("Right:"))   is not None:
            self._current["right"] = v
            # "Right" is the last field in the block — record is complete
            if "name" in self._current:
                completed.append(dict(self._current))
                self._current = {}

        if (v := field("Pos:")) is not None:
            m = re.match(r"\(\s*([-\d.]+),\s*([-\d.]+),\s*([-\d.]+)\s*\)", v)
            if m:
                self._current["x"] = float(m.group(1))
                self._current["y"] = float(m.group(2))
                self._current["z"] = float(m.group(3))

        return completed


_beacon_parser = BeaconViewParser()


# ── REST endpoints ────────────────────────────────────────────────────────────

@app.post("/add_node")
async def add_node(node: Node):
    if ser is None:
        raise HTTPException(status_code=503, detail="Serial port not open")
    cmd = (f"beacon add {node.name} {node.mac} {node.major} {node.minor} "
           f"{node.x} {node.y} {node.z} {node.left} {node.right}\n")
    ser.write(cmd.encode())
    beacon_registry[node.name] = node.dict()
    return {"status": "ok"}


@app.delete("/remove_node/{name}")
async def remove_node(name: str):
    if ser is None:
        raise HTTPException(status_code=503, detail="Serial port not open")
    ser.write(f"beacon remove {name}\n".encode())
    beacon_registry.pop(name, None)
    return {"status": "ok", "removed": name}


@app.get("/view_node/{name}")
async def view_node(name: str):
    if ser is None:
        raise HTTPException(status_code=503, detail="Serial port not open")
    ser.write(f"beacon view {name}\n".encode())
    return {"status": "ok", "queried": name}


@app.get("/view_nodes")
async def view_all_nodes():
    if ser is None:
        raise HTTPException(status_code=503, detail="Serial port not open")
    ser.write(b"beacon view -a\n")
    return {"status": "ok"}


@app.get("/beacon_registry")
async def get_beacon_registry():
    """Return the in-memory beacon registry (populated from device on connect)."""
    return {"beacons": list(beacon_registry.values())}


@app.get("/config")
async def get_config():
    return {
        "meas_power": MEAS_POWER,
        "path_loss_exp": PATH_LOSS_EXP,
        "baud_rate": BAUD_RATE,
        "serial_port": SERIAL_PORT,
        "num_nodes": NUM_NODES,
        "node_names": NODE_NAMES,
    }


@app.post("/config")
async def set_config(body: dict):
    global MEAS_POWER, PATH_LOSS_EXP
    if "meas_power" in body:
        MEAS_POWER = float(body["meas_power"])
    if "path_loss_exp" in body:
        PATH_LOSS_EXP = float(body["path_loss_exp"])
    return {"status": "ok", "meas_power": MEAS_POWER, "path_loss_exp": PATH_LOSS_EXP}


# ── WebSocket ─────────────────────────────────────────────────────────────────

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    clients.add(websocket)
    print(f"Client connected — {len(clients)} total")

    # Send whatever we already know immediately so the UI populates on reconnect
    if beacon_registry:
        try:
            await websocket.send_text(json.dumps({
                "type": "beacon_list",
                "beacons": list(beacon_registry.values()),
            }))
        except Exception:
            pass

    try:
        await websocket.receive_text()
    except WebSocketDisconnect:
        pass
    finally:
        clients.discard(websocket)
        print(f"Client disconnected — {len(clients)} total")


# ── Packet / line parsing ─────────────────────────────────────────────────────

def parse_packet(line: str) -> dict | None:
    try:
        line = line.replace("{{{", "{").replace("}}}", "}")
        line = line.replace("{{", "{").replace("}}", "}")
        if not (line.startswith("{") and line.endswith("}")):
            if line:
                print(f"Discarding incomplete line: {line!r}")
            return None
        raw = json.loads(line)
        data = raw.get("data_buffer", [])
        if len(data) != NUM_NODES:
            print(f"Unexpected data_len: {len(data)}, skipping")
            return None
        nodes = []
        for i, rssi in enumerate(data):
            nodes.append({
                "name": NODE_NAMES[i],
                "rssi": rssi,
                "distance": rssi_to_distance(rssi),
            })
        return {
            "type": "rssi",
            "nodes": nodes,
            "position": {
                "x": raw.get("pos_x", 0) / 100.0,
                "y": raw.get("pos_y", 0) / 100.0,
                "z": raw.get("pos_z", 0) / 100.0,
            },
        }
    except (json.JSONDecodeError, KeyError, TypeError) as e:
        print(f"Parse error: {e}")
        return None


async def broadcast(message: str):
    disconnected = set()
    for client in clients:
        try:
            await client.send_text(message)
        except Exception:
            disconnected.add(client)
    clients.difference_update(disconnected)


# ── Serial reader ─────────────────────────────────────────────────────────────

async def query_beacons_after_delay(delay: float = 1.5):
    """Wait for the shell to be ready then request all beacon data."""
    await asyncio.sleep(delay)
    if ser and ser.is_open:
        print("Querying beacon list from device...")
        ser.write(b"beacon view -a\n")


async def serial_reader():
    global ser
    loop = asyncio.get_event_loop()
    RETRY_DELAY = 3

    while True:
        print(f"Opening serial port {SERIAL_PORT} at {BAUD_RATE} baud...")
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        except serial.SerialException as e:
            print(f"Failed to open serial port: {e}")
            for port in serial.tools.list_ports.comports():
                print(f"  {port.device}")
            ser = None
            await asyncio.sleep(RETRY_DELAY)
            continue

        print("Serial port open — querying beacons in 1.5s...")
        asyncio.create_task(query_beacons_after_delay(1.5))

        try:
            while True:
                line = await loop.run_in_executor(None, ser.readline)
                decoded_line = line.decode("utf-8", errors="ignore").strip()
                if not decoded_line:
                    continue
                print(f"Raw: {decoded_line}")

                # Try beacon view parser first (shell text lines)
                completed = _beacon_parser.feed(decoded_line)
                for beacon in completed:
                    beacon_registry[beacon["name"]] = beacon
                    print(f"  → Registered beacon: {beacon['name']} at "
                          f"({beacon['x']}, {beacon['y']}, {beacon['z']})")
                    await broadcast(json.dumps({
                        "type": "beacon_list",
                        "beacons": list(beacon_registry.values()),
                    }))

                # Try JSON RSSI packet
                packet = parse_packet(decoded_line)
                if packet:
                    await broadcast(json.dumps(packet))

        except serial.SerialException as e:
            print(f"Serial error: {e} — retrying in {RETRY_DELAY}s...")
        except Exception as e:
            print(f"Unexpected error in serial reader: {e}")
        finally:
            try:
                if ser and ser.is_open:
                    ser.close()
            except Exception:
                pass
            ser = None

        await asyncio.sleep(RETRY_DELAY)


@app.on_event("startup")
async def startup_event():
    asyncio.create_task(serial_reader())