ESP-32 Client
====================
>ESP32-S3-Touch-LCD-7B Client for our energy optimizer server

![C](https://img.shields.io/badge/C-%2300599C.svg?style=flat&logo=c&logoColor=white)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Build](https://github.com/Stockholm-3/esp32-client/actions/workflows/build.yml/badge.svg)](https://github.com/Stockholm-3/esp32-client/actions/workflows/build.yml)
[![Simulator Build](https://github.com/Stockholm-3/esp32-client/actions/workflows/build_linux.yml/badge.svg)](https://github.com/Stockholm-3/esp32-client/actions/workflows/build_linux.yml)
[![Format](https://github.com/Stockholm-3/esp32-client/actions/workflows/format.yml/badge.svg)](https://github.com/Stockholm-3/esp32-client/actions/workflows/format.yml)
[![Lint](https://github.com/Stockholm-3/esp32-client/actions/workflows/lint.yml/badge.svg)](https://github.com/Stockholm-3/esp32-client/actions/workflows/lint.yml)

---

## Related repos
[stockholm-3/bingus-lib](https://github.com/Stockholm-3/bingus-lib) — Platform-generic library providing useful modules such as SMW and cache.

[stockholm-3/just-api](https://github.com/Stockholm-3/just-api) — Energy optimizer server providing electricity price plans and weather data displayed on the device.

---

## Local Server (Web Configuration UI)

The ESP32 runs a built-in HTTP server on **port 80** that serves a browser-based settings page. It lets you configure WiFi credentials, network options, display behavior, and data sources — all without serial access or re-flashing.

### How it works

When the local server is running, the device hosts a single-page web app. The browser connects via **WebSocket** (`ws://<device-ip>/ws`) and exchanges JSON messages to read and update device settings in real time. All configuration is persisted to NVS flash and survives reboots.

```
Browser ──── HTTP GET / ──────────────────► ESP32 (port 80)
        ◄─── index.html, app.js, style.css ─
        ──── WebSocket /ws ───────────────►
        ◄─── {"type":"settings", ...} ───────
        ──── {"type":"set_settings", ...} ──►
```

The server starts automatically when the device has a network connection (STA mode) or when Access Point mode is enabled.

---

### First run — Access Point mode

On first boot the device has no saved WiFi credentials. It automatically starts its own open Wi-Fi access point so you can configure it from a browser.

1. **Flash the firmware** and open the serial monitor:
   ```
   make fm
   ```

2. **Connect your phone or laptop** to the Wi-Fi network named **`ESP32-Settings`** (no password).

3. **Open a browser** and navigate to:
   ```
   http://192.168.4.1
   ```

4. Go to the **WiFi** tab, press **Scan**, select your network from the list, enter the password, and press **Connect**.

5. The device connects to your network and **restarts automatically**. The access point disappears.

> **Note:** AP mode can be re-enabled at any time from the **Settings** tab on the device display, or by pressing the AP toggle in the web UI.

---

### Accessing the UI on your local network (STA mode)

After the device joins your Wi-Fi, the local server is reachable in two ways:

| Method | URL | Notes |
| :--- | :--- | :--- |
| mDNS hostname | `http://esp32-client.local/` | Works on macOS, Linux, iOS; requires mDNS support |
| IP address | `http://<device-ip>/` | IP is printed in the serial monitor on boot |

The current IP is also shown in the **Network** section of the web UI.

---

### What you can configure

| Setting | Description |
| :--- | :--- |
| **WiFi SSID / Password** | Network credentials to connect to |
| **Static IP** | Fix the device IP in your LAN (optional) |
| **Gateway / Netmask** | Required when static IP is enabled |
| **mDNS Hostname** | Local hostname, default `esp32-client` → `esp32-client.local` |
| **City** | City name used for weather forecast fetching |
| **Price Zone** | Swedish electricity zone: SE1, SE2, SE3, or SE4 |
| **Screen Timeout** | Time before display dims: 5 / 10 / 15 / 20 min, or Never |

> Changing any network setting (static IP, gateway, netmask, or hostname) triggers an automatic device restart so the new configuration takes effect.

---

## Prerequisites

This project is based on the **ESP-IDF** framework and includes a Makefile wrapper for common development tasks, including a Linux-based simulator and static analysis tools.

### 1. Core Toolchain
*   **[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html):** Ensure `idf.py` is in your PATH. (Compatible with ESP32-S3 and other targets).
*   **Python 3.x:** Required for IDF and project helper scripts.
*   **Make & Bash:** Used to run the automation commands.

### 2. Analysis & Formatting Tools
To use the `make lint` and `make format` commands, you need:
*   **Clang Tools:** `clang-format` and `clang-tidy`.
*   **run-clang-tidy:** Usually bundled with the `clang-tools` or `llvm` package.

### 3. Documentation & Simulator
*   **Doxygen:** Required to generate API docs (`make docs`).
*   **SDL2:** Recommended for the Linux simulator if your app uses graphical components (Squareline/LVGL).

---

## Development Workflow

### Build & Flash
| Command | Action |
| :--- | :--- |
| `make build` | Compile the project using `idf.py`. |
| `make flash` | Flash the firmware to the connected device. |
| `make monitor` | Open the serial monitor. |
| `make fm` | Shortcut to Flash + Monitor. |
| `make hardclean` | Wipe the build folder, managed components, and sdkconfig. |

### Linux Simulator
You can test logic and UI natively on your host machine:
*   `make linux-run`: Builds and executes the simulator (`./simulator/build/simulator.elf`).
*   `make linux-hardclean`: Removes simulator build artifacts.

### GDB Debugging (Hardware)
Requires the physical ESP32-S3-Touch-LCD-7B board connected via its native USB port (the JTAG/USB port, not the UART port).

**One-time Windows setup:**
1. Download [Zadig](https://zadig.akeo.ie/).
2. In Zadig: **Options → List All Devices**, find `USB JTAG/serial debug unit (Interface 2)`.
3. Set driver to **WinUSB** and click "Install Driver". Do not change Interface 0 or 1 (that is the serial/UART used by `make monitor`).

> Linux/macOS users do not need Zadig. Linux may require a udev rule, which the ESP-IDF installer typically handles automatically.

**Debugging workflow:**

*Option A — Terminal (any OS):*
```bash
# Terminal 1
make openocd

# Terminal 2
make gdb
```

*Option B — VS Code:*
1. Install the [ESP-IDF extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension).
2. Start OpenOCD: `make openocd` (or use the extension's OpenOCD Manager in the status bar).
3. Press **F5** — the `.vscode/launch.json` config will connect automatically.

**Note:** The default build uses performance optimizations (`CONFIG_COMPILER_OPTIMIZATION_PERF`). For cleaner stepping and variable inspection, temporarily set `CONFIG_COMPILER_OPTIMIZATION_DEBUG=y` in `sdkconfig` before building.

### GDB Debugging (Simulator)
No hardware required. Runs natively on Linux/WSL.

```bash
make linux-build
gdb ./simulator/build/simulator.elf
```

Common GDB commands:
| Command | Action |
| :--- | :--- |
| `break app_main` | Set a breakpoint at `app_main` |
| `run` | Start the program |
| `next` | Step over one line |
| `step` | Step into a function call |
| `print my_variable` | Inspect a variable |
| `backtrace` | Show the call stack |
| `continue` | Resume until next breakpoint |
| `list` | Show source code around current line |
| `quit` | Exit GDB |

Start with `break app_main` then `run` to stop at the entry point and step from there. Note that hardware-specific code (display, touch, GPIO) is stubbed out in the simulator.

### Quality Control
| Command | Action |
| :--- | :--- |
| `make format-fix` | Automatically fix code style using `clang-format`. |
| `make lint` | Run static analysis (must run `idf.py reconfigure` first). |
| `make lint-fix` | Attempt to automatically apply safe linter suggestions. |
| `make format-check` | Dry-run for CI to ensure code meets style guidelines. |

### Testing

Run unit test on esp.
```bash
make test-monitor
```

### UART CLI INTERFACE

To run the CLI interface make sure to connect your USB-C cable to the UART1 port on the esp and also make sure the switch is on UART1. After than you can simply run the following:
```bash
make monitor
```
AVAILABLE COMMANDS
help            — show available commands
status          — system telemetry
sensor          — BME280 reading
backlight       — set backlight 0-255
simulate_touch  — simulate touch event
test_backlight  — run backlight test (~5s)
restart         — software reset


---

## Project Structure
*   `main/`: Primary application logic.
*   `components/`: Reusable project modules.
*   `simulator/`: Linux-native build configuration and entry point.
*   `scripts/`: Python utilities for scrubbing compile commands and filtering linter output.
