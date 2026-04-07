"""ClaudeMeter Native Messaging Host.

Reads Chrome Native Messaging frames from stdin,
forwards usage JSON to the RP2350 board via USB serial.
"""

import json
import os
import struct
import sys
import time

from serial_writer import SerialWriter


def get_config_path():
    """Locate claudemeter_config.json next to the executable or script."""
    if getattr(sys, "_MEIPASS", None):
        base = sys._MEIPASS
    else:
        base = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(base, "claudemeter_config.json")


def read_native_message():
    """Read a single Chrome Native Messaging frame from stdin."""
    raw_length = sys.stdin.buffer.read(4)
    if not raw_length or len(raw_length) < 4:
        return None
    length = struct.unpack("<I", raw_length)[0]
    if length > 1024 * 1024:
        return None
    data = sys.stdin.buffer.read(length)
    if len(data) < length:
        return None
    return json.loads(data.decode("utf-8"))


def send_native_message(obj):
    """Write a Chrome Native Messaging frame to stdout."""
    encoded = json.dumps(obj).encode("utf-8")
    sys.stdout.buffer.write(struct.pack("<I", len(encoded)))
    sys.stdout.buffer.write(encoded)
    sys.stdout.buffer.flush()


def main():
    config_path = get_config_path()
    with open(config_path, "r") as f:
        config = json.load(f)

    com_port = config.get("com_port", "COM5")
    writer = SerialWriter(com_port)

    print(f"ClaudeMeter host starting on {com_port}", file=sys.stderr)

    while True:
        msg = read_native_message()
        if msg is None:
            break

        serial_json = json.dumps(msg, separators=(",", ":"))

        try:
            writer.connect()
            writer.write(serial_json)
            send_native_message({"ok": True})
        except Exception as e:
            print(f"Serial error: {e}", file=sys.stderr)
            writer.close()
            time.sleep(3)
            send_native_message({"ok": False, "error": str(e)})


if __name__ == "__main__":
    main()
