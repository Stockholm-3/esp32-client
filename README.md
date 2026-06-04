ESP-32 Client
====================
>ESP32-S3-Touch-LCD-7B Client for our energy optmimzer server

![C](https://img.shields.io/badge/C-%2300599C.svg?style=flat&logo=c&logoColor=white)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Build](https://github.com/Stockholm-3/esp32-client/actions/workflows/build.yml/badge.svg)](https://github.com/Stockholm-3/esp32-client/actions/workflows/build.yml)
[![Simulator Build](https://github.com/Stockholm-3/esp32-client/actions/workflows/build_linux.yml/badge.svg)](https://github.com/Stockholm-3/esp32-client/actions/workflows/build_linux.yml)
[![Format](https://github.com/Stockholm-3/esp32-client/actions/workflows/format.yml/badge.svg)](https://github.com/Stockholm-3/esp32-client/actions/workflows/format.yml)
[![Lint](https://github.com/Stockholm-3/esp32-client/actions/workflows/lint.yml/badge.svg)](https://github.com/Stockholm-3/esp32-client/actions/workflows/lint.yml)
---

## Related repos
[stockholm-3/bingus-lib](https://github.com/Stockholm-3/bingus-lib) - Platform generic library providing usefull modules such as smw and cache

[stockholm-3/just-api](https://github.com/Stockholm-3/just-api) - Our energy optimizer server providing an energy plan displayed as a graph on the esp

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

### Unit Tests
Tests are written using the Unity framework (ESP-IDF's built-in test library) and live in each component's `test/` subdirectory. They require the physical ESP32-S3 board to run.

| Command | Action |
| :--- | :--- |
| `make test` | Build the test firmware. |
| `make test-flash` | Build and flash the test firmware to the connected device. |
| `make test-monitor` | Build, flash, and open the serial monitor to view results. |

**Workflow:**
1. Connect the ESP32-S3 board via USB.
2. Run `make test-monitor`.
3. Unity prints pass/fail results over serial:
```
TEST(cache_fs, cache_fs_config stores root_path correctly) PASS
TEST(cache_fs, cache_fs_config stores zero TTL correctly) PASS
...
14 Tests 0 Failures 0 Ignored
OK
```

> Tests cannot run without the hardware — the Linux simulator uses stubs that do not implement the ESP32-specific logic that many tests exercise.

---

## Project Structure
*   `main/`: Primary application logic.
*   `components/`: Reusable project modules.
*   `simulator/`: Linux-native build configuration and entry point.
*   `scripts/`: Python utilities for scrubbing compile commands and filtering linter output.
