import json
import threading
import collections
import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation

SERIAL_PORT = "/dev/ttyACM0"
BAUD_RATE = 115200
HISTORY = 100
MA_WINDOW = 100

NODE_NAMES = [
    '4011-A','4011-B','4011-C','4011-D','4011-E','4011-F','4011-G',
    '4011-H','4011-I','4011-J','4011-K','4011-L','4011-M'
]
N = len(NODE_NAMES)

buffers    = [collections.deque([None] * HISTORY, maxlen=HISTORY) for _ in range(N)]
ma_buffers = [collections.deque([None] * HISTORY, maxlen=HISTORY) for _ in range(N)]
ma_windows = [collections.deque(maxlen=MA_WINDOW) for _ in range(N)]
lock = threading.Lock()

# ── Moving average ────────────────────────────────────────────────────────────

def push_rssi(i, rssi):
    """Push a new RSSI value for node i, update raw + MA buffers."""
    buffers[i].append(rssi)

    if rssi is not None:
        ma_windows[i].append(rssi)
        valid = [v for v in ma_windows[i] if v is not None]
        ma_buffers[i].append(sum(valid) / len(valid) if valid else None)
    else:
        # Don't let a dead beacon corrupt the window — just gap the MA too
        ma_buffers[i].append(None)

# ── Serial reader thread ──────────────────────────────────────────────────────

def parse_line(line: str):
    try:
        line = line.replace("{{{", "{").replace("}}}", "}").replace("{{", "{").replace("}}", "}")
        if not (line.startswith("{") and line.endswith("}")):
            return
        raw = json.loads(line)
        data = raw.get("data_buffer", [])
        if len(data) != N:
            return
        with lock:
            for i, rssi in enumerate(data):
                push_rssi(i, rssi if rssi != 0 else None)
    except Exception as e:
        print(f"Parse error: {e} | line: {line!r}")

def serial_reader():
    while True:
        try:
            print(f"Opening {SERIAL_PORT} at {BAUD_RATE} baud...")
            with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
                print("Serial open.")
                while True:
                    line = ser.readline().decode("utf-8", errors="ignore").strip()
                    if line:
                        parse_line(line)
        except serial.SerialException as e:
            print(f"Serial error: {e} — retrying...")
            import time; time.sleep(3)

threading.Thread(target=serial_reader, daemon=True).start()

# ── Plot setup ────────────────────────────────────────────────────────────────

fig, axes = plt.subplots(N, 1, figsize=(12, 26), sharex=True)
fig.suptitle("Live RSSI — 13 iBeacon nodes", fontsize=13, fontweight="bold")
fig.tight_layout(rect=[0, 0, 1, 0.97])

raw_lines = []
ma_lines  = []

for i, ax in enumerate(axes):
    (raw_line,) = ax.plot([], [], lw=0.8, alpha=0.35, color="steelblue", label="raw")
    (ma_line,)  = ax.plot([], [], lw=1.6, color="steelblue", label=f"MA{MA_WINDOW}")
    ax.set_xlim(0, HISTORY - 1)
    ax.set_ylim(-100, -30)
    ax.set_ylabel(NODE_NAMES[i], fontsize=8, rotation=0, labelpad=40, va="center")
    ax.tick_params(axis="y", labelsize=7)
    ax.tick_params(axis="x", labelsize=7)
    ax.grid(True, alpha=0.3)
    ax.axhline(-70, color="red", lw=0.5, linestyle="--", alpha=0.4)
    raw_lines.append(raw_line)
    ma_lines.append(ma_line)

# Single legend on the first axis
axes[0].legend(loc="upper right", fontsize=7, framealpha=0.6)
axes[-1].set_xlabel("Samples")

x_data = list(range(HISTORY))

def to_plot(buf):
    return [v if v is not None else float("nan") for v in buf]

def update(_frame):
    with lock:
        raw_snap = [list(b) for b in buffers]
        ma_snap  = [list(b) for b in ma_buffers]

    for i in range(N):
        raw_lines[i].set_data(x_data, to_plot(raw_snap[i]))
        ma_lines[i].set_data(x_data,  to_plot(ma_snap[i]))

    return raw_lines + ma_lines

ani = animation.FuncAnimation(fig, update, interval=200, blit=True, cache_frame_data=False)
plt.show()