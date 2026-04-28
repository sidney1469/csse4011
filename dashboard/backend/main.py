import asyncio
import json
import re
import serial
import serial.tools.list_ports
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

BAUD_RATE = 115200
SERIAL_PORT = "/dev/ttyACM0"

NUM_NODES = 13
NODE_NAMES = ['4011-A','4011-B','4011-C','4011-D','4011-E','4011-F',
              '4011-G','4011-H','4011-I','4011-J','4011-K','4011-L','4011-M']

MEAS_POWER = -56
PATH_LOSS_EXP = 2.5

app = FastAPI()
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_credentials=False, allow_methods=["*"], allow_headers=["*"])

clients: set[WebSocket] = set()
ser: serial.Serial = None
beacon_registry: dict[str, dict] = {}
SNIFFER_MODE = False


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


def rssi_to_distance(rssi: int) -> float:
    if rssi == 0:
        return 0.0
    return round(10 ** ((MEAS_POWER - rssi) / (10 * PATH_LOSS_EXP)), 2)


# ── Beacon view parser ────────────────────────────────────────────────────────

class BeaconViewParser:
    def __init__(self):
        self._current: dict = {}

    def feed(self, line: str) -> list[dict]:
        completed = []
        if line.startswith("---"):
            self._current = {}
            return []

        def field(prefix):
            if line.startswith(prefix):
                return line[len(prefix):].strip()
            return None

        if (v := field("Name:"))  is not None: self._current["name"]  = v
        if (v := field("MAC:"))   is not None: self._current["mac"]   = v
        if (v := field("Major:")) is not None: self._current["major"] = int(v)
        if (v := field("Minor:")) is not None: self._current["minor"] = int(v)
        if (v := field("Left:"))  is not None: self._current["left"]  = v
        if (v := field("Right:")) is not None:
            self._current["right"] = v
            if "name" in self._current:
                completed.append(dict(self._current))
                self._current = {}

        if (v := field("Pos:")) is not None:
            v = v.strip('(').strip(')')
            m = tuple(map(float, v.split(",")))
            print(v)
            if m:
                self._current["x"] = float(m[0])
                self._current["y"] = float(m[1])

        return completed


_beacon_parser = BeaconViewParser()


# ── REST endpoints ────────────────────────────────────────────────────────────

@app.post("/add_node")
async def add_node(node: Node):
    if ser is None:
        raise HTTPException(status_code=503, detail="Serial port not open")
    cmd = (f"beacon add {node.name} {node.mac} {node.major} {node.minor} "
           f"{node.x} {node.y} {node.left} {node.right}\n")
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
    return {"beacons": list(beacon_registry.values())}

@app.get("/config")
async def get_config():
    return {"meas_power": MEAS_POWER, "path_loss_exp": PATH_LOSS_EXP,
            "baud_rate": BAUD_RATE, "serial_port": SERIAL_PORT,
            "num_nodes": NUM_NODES, "node_names": NODE_NAMES}

@app.post("/config")
async def set_config(body: dict):
    global MEAS_POWER, PATH_LOSS_EXP
    if "meas_power"    in body: MEAS_POWER    = float(body["meas_power"])
    if "path_loss_exp" in body: PATH_LOSS_EXP = float(body["path_loss_exp"])
    return {"status": "ok", "meas_power": MEAS_POWER, "path_loss_exp": PATH_LOSS_EXP}

@app.post("/sniffer/{state}")
async def set_sniffer(state: str):
    global SNIFFER_MODE
    if ser is None:
        raise HTTPException(status_code=503, detail="Serial port not open")
    if state == "on":
        ser.write(b"beacon sniffer\n")
        SNIFFER_MODE = True
    elif state == "off":
        ser.write(b"beacon sniffer\n")
        SNIFFER_MODE = False
    else:
        raise HTTPException(status_code=400, detail="state must be 'on' or 'off'")
    return {"status": "ok", "sniffer": SNIFFER_MODE}

@app.get("/sniffer")
async def get_sniffer():
    return {"sniffer": SNIFFER_MODE}


# ── WebSocket ─────────────────────────────────────────────────────────────────

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    clients.add(websocket)
    print(f"Client connected — {len(clients)} total")
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


# Replace the global node_packet dict and parse_packet function with this:

_current_node: dict = {}

def parse_packet(line: str) -> dict | None:
    global _current_node

    try:
        line = line.replace("{{{", "{").replace("}}}", "}")
        line = line.replace("{{",  "{").replace("}}",  "}")

        # ── JSON path (localisation packet) ─────────────────────────────
        if line.startswith("{") and line.endswith("}"):
            raw = json.loads(line)
            data = raw.get("data_buffer", [])
            if len(data) != NUM_NODES:
                print(f"Unexpected data_len={len(data)}, expected {NUM_NODES} — skipping")
                return None
            nodes = [
                {"name": NODE_NAMES[i], "rssi": rssi, "distance": rssi_to_distance(rssi)}
                for i, rssi in enumerate(data)
            ]
            raw_pos      = {"x": raw.get("raw_pos_x",      0) / 100.0, "y": raw.get("raw_pos_y",      0) / 100.0}
            filtered_pos = {"x": raw.get("filtered_pos_x", 0) / 100.0, "y": raw.get("filtered_pos_y", 0) / 100.0}
            velocity     = {"x": raw.get("velocity_x",     0) / 100.0, "y": raw.get("velocity_y",     0) / 100.0}
            timestamp    = raw.get("timestamp", 0)

            return {
                "type":         "rssi",
                "nodes":        nodes,
                "raw_position": raw_pos,
                "position":     filtered_pos,
                "velocity":     velocity,
                "timestamp":    timestamp,
            }


        # ── Plain-text sniffer path ──────────────────────────────────────
        line = line.strip()
        # Separator line — start of a new node record
        if line.startswith("=====") or line.startswith("-----"):
            _current_node = {"type": "sniffer_node"}
            return None

        if ":" not in line:
            return None

        key, _, value = line.partition(":")
        key   = key.strip()
        value = value.strip()

        field_map = {
            "addr":                    "addr",
            "rssi":                    "rssi",
            "tx_power":                "tx_power",
            "adv_type":                "adv_type",
            "adv_props":               "adv_props",
            "has_name":                "has_name",
            "name":                    "name",
            "has_flags":               "has_flags",
            "flags":                   "flags",
            "has_manufacturer_data":   "has_manufacturer_data",
            "manufacturer_company_id": "manufacturer_company_id",
            "manufacturer_data":       "manufacturer_data",
            "has_service_data":        "has_service_data",
            "service_data_type":       "service_data_type",
            "service_data":            "service_data",
            "interval":                "interval",
            "primary_phy":             "primary_phy",
            "secondary_phy":           "secondary_phy",
            "payload raw":                     "raw",
        }

        if key in field_map:
            _current_node[field_map[key]] = value

        # "raw" is the last field printed — emit the completed record
        if key == "payload raw":
            completed = dict(_current_node)   # snapshot, not a reference
            _current_node = {"type": "sniffer_node"}
            print(f"Sniffer node: {completed}")
            return completed

        return None

    except (json.JSONDecodeError, KeyError, TypeError) as e:
        print(f"Parse error: {e} | {line!r}")
        return None

# ── Broadcast ─────────────────────────────────────────────────────────────────

async def broadcast(message: str):
    dead = set()
    for client in clients:
        try:
            await client.send_text(message)
        except Exception:
            dead.add(client)
    clients.difference_update(dead)


# ── Serial reader ─────────────────────────────────────────────────────────────

async def query_beacons_after_delay(delay: float = 1.5):
    await asyncio.sleep(delay)
    if ser and ser.is_open:
        print("Querying beacon list from device...")
        ser.write(b"beacon view -a\n")


async def serial_reader():
    global ser
    loop = asyncio.get_event_loop()
    RETRY = 3

    while True:
        print(f"Opening {SERIAL_PORT} @ {BAUD_RATE}...")
        try:
            ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        except serial.SerialException as e:
            print(f"Failed: {e}")
            for p in serial.tools.list_ports.comports():
                print(f"  {p.device}")
            ser = None
            await asyncio.sleep(RETRY)
            continue

        print("Serial open — querying beacons in 1.5 s...")
        asyncio.create_task(query_beacons_after_delay(1.5))

        try:
            while True:
                line = await loop.run_in_executor(None, ser.readline)
                decoded = line.decode("utf-8", errors="ignore").strip()
                if not decoded:
                    continue

                # Shell text output (beacon view)
                for beacon in _beacon_parser.feed(decoded):
                    beacon_registry[beacon["name"]] = beacon
                    print(f"  → Beacon: {beacon['name']} @ ({beacon['x']}, {beacon['y']})")
                    await broadcast(json.dumps({
                        "type": "beacon_list",
                        "beacons": list(beacon_registry.values()),
                    }))

                # Firmware JSON packet
                packet = parse_packet(decoded)
                if packet:
                    await broadcast(json.dumps(packet))

        except serial.SerialException as e:
            print(f"Serial error: {e} — retrying in {RETRY} s...")
        except Exception as e:
            print(f"Unexpected: {e}")
        finally:
            try:
                if ser and ser.is_open:
                    ser.close()
            except Exception:
                pass
            ser = None

        await asyncio.sleep(RETRY)


@app.on_event("startup")
async def startup_event():
    asyncio.create_task(serial_reader())