import sys
import time
import serial


class SerialWriter:
    def __init__(self, port, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None

    def connect(self):
        if self.ser and self.ser.is_open:
            return
        self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
        time.sleep(2)  # Wait for board DTR reset
        print(f"Connected to {self.port}", file=sys.stderr)

    def write(self, data):
        if not self.ser or not self.ser.is_open:
            self.connect()
        self.ser.write((data + "\n").encode("utf-8"))
        self.ser.flush()

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.ser = None
