import math
import serial
import serial.tools.list_ports

BAUD_RATE = 115200
MEAS_POWER = -56   # RSSI at 1 metre
PATH_LOSS_EXP = 2.5 # Needs to be calibrated (higher for more obstructions)
                # ~2 for free space ~2.5 - 4 indoors

def rssi_to_distance(rssi: int) -> float:
    if rssi == 0:
        return -1; # error
    distance = round(10 ** ((MEAS_POWER - rssi) / (10 * PATH_LOSS_EXP)),2)
    return distance