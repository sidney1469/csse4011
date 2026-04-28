import json
import threading
import collections
import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation

SERIAL_PORT = "/dev/ttyACM0"
BAUD_RATE   = 115200
HISTORY     = 200

raw_x      = collections.deque([float("nan")] * HISTORY, maxlen=HISTORY)
raw_y      = collections.deque([float("nan")] * HISTORY, maxlen=HISTORY)
filtered_x = collections.deque([float("nan")] * HISTORY, maxlen=HISTORY)
filtered_y = collections.deque([float("nan")] * HISTORY, maxlen=HISTORY)
lock = threading.Lock()

# ── Serial reader ─────────────────────────────────────────────────────────────

def parse_line(line: str):
    try:
        line = line.replace("{{{", "{").replace("}}}", "}").replace("{{", "{").replace("}}", "}")
        if not (line.startswith("{") and line.endswith("}")):
            return
        raw = json.loads(line)
        if "raw_pos_x" not in raw:
            return
        rx = raw["raw_pos_x"]      / 100.0
        ry = raw["raw_pos_y"]      / 100.0
        fx = raw["filtered_pos_x"] / 100.0
        fy = raw["filtered_pos_y"] / 100.0
        with lock:
            raw_x.append(rx);      raw_y.append(ry)
            filtered_x.append(fx); filtered_y.append(fy)
    except Exception as e:
        print(f"Parse error: {e} | {line!r}")

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

fig, axes = plt.subplots(2, 2, figsize=(12, 8))
fig.suptitle("Live position — raw vs Kalman filtered", fontsize=13, fontweight="bold")
fig.tight_layout(rect=[0, 0, 1, 0.95])

ax_x, ax_y, ax_xy_raw, ax_xy_filt = axes[0][0], axes[0][1], axes[1][0], axes[1][1]

# Time-series X
ax_x.set_title("X position over time")
ax_x.set_ylabel("x (m)"); ax_x.set_xlabel("samples")
ax_x.set_xlim(0, HISTORY - 1); ax_x.grid(True, alpha=0.3)
line_raw_x,  = ax_x.plot([], [], lw=1.0, alpha=0.5, color="steelblue", label="raw")
line_filt_x, = ax_x.plot([], [], lw=1.8, color="steelblue", label="kalman")
ax_x.legend(fontsize=8)

# Time-series Y
ax_y.set_title("Y position over time")
ax_y.set_ylabel("y (m)"); ax_y.set_xlabel("samples")
ax_y.set_xlim(0, HISTORY - 1); ax_y.grid(True, alpha=0.3)
line_raw_y,  = ax_y.plot([], [], lw=1.0, alpha=0.5, color="coral", label="raw")
line_filt_y, = ax_y.plot([], [], lw=1.8, color="coral", label="kalman")
ax_y.legend(fontsize=8)

# XY trajectory — raw
ax_xy_raw.set_title("XY trajectory — raw")
ax_xy_raw.set_xlabel("x (m)"); ax_xy_raw.set_ylabel("y (m)")
ax_xy_raw.grid(True, alpha=0.3)
traj_raw,  = ax_xy_raw.plot([], [], lw=1.0, alpha=0.6, color="steelblue")
dot_raw,   = ax_xy_raw.plot([], [], "o", ms=6, color="steelblue")

# XY trajectory — filtered
ax_xy_filt.set_title("XY trajectory — kalman")
ax_xy_filt.set_xlabel("x (m)"); ax_xy_filt.set_ylabel("y (m)")
ax_xy_filt.grid(True, alpha=0.3)
traj_filt, = ax_xy_filt.plot([], [], lw=1.5, color="coral")
dot_filt,  = ax_xy_filt.plot([], [], "o", ms=6, color="coral")

x_idx = list(range(HISTORY))

def update(_frame):
    with lock:
        rx = list(raw_x);      ry = list(raw_y)
        fx = list(filtered_x); fy = list(filtered_y)

    # time series
    line_raw_x.set_data(x_idx, rx)
    line_filt_x.set_data(x_idx, fx)
    line_raw_y.set_data(x_idx, ry)
    line_filt_y.set_data(x_idx, fy)

    # auto-scale time-series axes
    for ax, vals in [(ax_x, rx + fx), (ax_y, ry + fy)]:
        finite = [v for v in vals if v == v]  # filter nan
        if finite:
            pad = max((max(finite) - min(finite)) * 0.15, 0.5)
            ax.set_ylim(min(finite) - pad, max(finite) + pad)

    # XY trajectories
    traj_raw.set_data(rx, ry)
    traj_filt.set_data(fx, fy)

    # current position dot (last non-nan)
    def last_valid(xs, ys):
        for x, y in zip(reversed(xs), reversed(ys)):
            if x == x and y == y:
                return [x], [y]
        return [], []

    dot_raw.set_data(*last_valid(rx, ry))
    dot_filt.set_data(*last_valid(fx, fy))

    # auto-scale XY axes (shared limits so they're comparable)
    all_x = [v for v in rx + fx if v == v]
    all_y = [v for v in ry + fy if v == v]
    if all_x and all_y:
        xpad = max((max(all_x) - min(all_x)) * 0.15, 0.5)
        ypad = max((max(all_y) - min(all_y)) * 0.15, 0.5)
        for ax in [ax_xy_raw, ax_xy_filt]:
            ax.set_xlim(min(all_x) - xpad, max(all_x) + xpad)
            ax.set_ylim(min(all_y) - ypad, max(all_y) + ypad)

    return line_raw_x, line_filt_x, line_raw_y, line_filt_y, traj_raw, traj_filt, dot_raw, dot_filt

ani = animation.FuncAnimation(fig, update, interval=200, blit=True, cache_frame_data=False)
plt.show()
