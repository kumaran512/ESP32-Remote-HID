# ESP32 Remote HID

Standalone ESP32-S3 N16R8 wireless keyboard + mouse.

## Architecture

Android/PC browser → Wi-Fi → ESP32-S3 N16R8 → native USB HID → target laptop.

## Main improvement over the original project

The original firmware uses the Arduino `WebServer` class and sends mouse movement through repeated HTTP requests. The Arduino WebServer implementation supports only one simultaneous client. This project keeps HTTP for settings/Ducky compatibility but moves live keyboard/mouse traffic to a persistent WebSocket connection.

Mouse path:

Browser touch → accumulate/throttle → WebSocket → ESP32 → `USBHIDMouse::move()`

Keyboard path:

Browser key → WebSocket → ESP32 → `USBHIDKeyboard`

The browser sends pointer updates at the existing 15 ms cadence (~66 Hz), but without opening/queuing a new HTTP request for every movement report.

## Build in GitHub Codespaces

1. Create a GitHub repository.
2. Upload this project preserving the folder structure.
3. Open **Code → Codespaces → Create codespace on main**.
4. In the Codespace terminal:

```bash
python3 -m pip install --user platformio
pio run
```

The firmware is created at:

```text
.pio/build/esp32-s3-n16r8/firmware.bin
```

## Local PlatformIO

Open the repository in VS Code with the PlatformIO extension, then run **Build**.

## Flashing

For a first flash, put the ESP32-S3 into download mode if required by the board. Then use:

```bash
pio run -t upload
```

If PlatformIO cannot automatically find the port, specify it:

```bash
pio run -t upload --upload-port /dev/ttyACM0
```

Windows example:

```text
pio run -t upload --upload-port COM5
```

## Default Wi-Fi

SSID:

```text
ESP32 Remote HID
```

Password:

```text
password123
```

Open either:

```text
http://192.168.4.1
```

or, if mDNS works on the client:

```text
http://esp32hid.local
```

## Target USB connection

Connect the ESP32-S3 native USB data port to the laptop being controlled. The ESP32-S3 presents itself as a USB keyboard/mouse; the phone or other computer only supplies the WebUI commands over Wi-Fi.

## Notes

- The original Ducky/script HTTP endpoints are retained for compatibility.
- Live HID events use `/ws` WebSocket.
- No cloud service or Internet connection is required.
- The ESP32 runs as its own Wi-Fi access point.
