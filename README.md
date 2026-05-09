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

### Quality Control
| Command | Action |
| :--- | :--- |
| `make format-fix` | Automatically fix code style using `clang-format`. |
| `make lint` | Run static analysis (must run `idf.py reconfigure` first). |
| `make lint-fix` | Attempt to automatically apply safe linter suggestions. |
| `make format-check` | Dry-run for CI to ensure code meets style guidelines. |

---

## Project Structure
*   `main/`: Primary application logic.
*   `components/`: Reusable project modules.
*   `simulator/`: Linux-native build configuration and entry point.
*   `scripts/`: Python utilities for scrubbing compile commands and filtering linter output.
