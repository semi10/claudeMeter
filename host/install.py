"""Install ClaudeMeter native messaging host for Chrome on Windows.

Usage:
    python install.py --exe C:\\path\\to\\claudemeter_host.exe --extension-id abcdef...
"""

import argparse
import json
import os
import sys
import winreg


MANIFEST_NAME = "com.example.claudemeter"
REGISTRY_KEY = f"Software\\Google\\Chrome\\NativeMessagingHosts\\{MANIFEST_NAME}"


def main():
    parser = argparse.ArgumentParser(description="Install ClaudeMeter NM host")
    parser.add_argument("--exe", required=True, help="Absolute path to claudemeter_host.exe")
    parser.add_argument("--extension-id", required=True, help="Chrome extension ID")
    args = parser.parse_args()

    exe_path = os.path.abspath(args.exe)
    if not os.path.isfile(exe_path):
        print(f"Error: exe not found at {exe_path}", file=sys.stderr)
        sys.exit(1)

    # Write manifest
    manifest_dir = os.path.join(os.environ["APPDATA"], "ClaudeMeter")
    os.makedirs(manifest_dir, exist_ok=True)
    manifest_path = os.path.join(manifest_dir, f"{MANIFEST_NAME}.json")

    manifest = {
        "name": MANIFEST_NAME,
        "description": "ClaudeMeter serial bridge",
        "path": exe_path,
        "type": "stdio",
        "allowed_origins": [f"chrome-extension://{args.extension_id}/"],
    }

    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"Manifest written to {manifest_path}")

    # Create registry key
    try:
        key = winreg.CreateKey(winreg.HKEY_CURRENT_USER, REGISTRY_KEY)
        winreg.SetValueEx(key, "", 0, winreg.REG_SZ, manifest_path)
        winreg.CloseKey(key)
        print(f"Registry key created: HKCU\\{REGISTRY_KEY}")
    except Exception as e:
        print(f"Error creating registry key: {e}", file=sys.stderr)
        sys.exit(1)

    print("Installation complete.")


if __name__ == "__main__":
    main()
