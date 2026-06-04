#!/usr/bin/env python3
"""Build (and optionally flash + monitor) the Unity test app."""

import argparse
import os
import shutil
import subprocess
import sys

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
TEST_DIR     = os.path.join(PROJECT_ROOT, "test")
BUILD_DIR    = os.path.join(TEST_DIR, "build")


def run(cmd: str, cwd: str) -> int:
    proc = subprocess.run(cmd, shell=True, cwd=cwd)
    return proc.returncode


def main() -> None:
    p = argparse.ArgumentParser(description="ESP32-S3 Unity test runner")
    p.add_argument("--target",  default="esp32s3", help="IDF target chip")
    p.add_argument("--flash",   action="store_true", help="Flash after build")
    p.add_argument("--monitor", action="store_true", help="Open serial monitor")
    args = p.parse_args()

    cmake_cache = os.path.join(BUILD_DIR, "CMakeCache.txt")

    # Wipe a half-baked build directory
    if os.path.exists(BUILD_DIR) and not os.path.exists(cmake_cache):
        print("--> Removing incomplete build directory...")
        shutil.rmtree(BUILD_DIR)

    # Fresh build: set the target first
    if not os.path.exists(BUILD_DIR):
        print(f"--> Initialising build for {args.target}...")
        if run(f"idf.py set-target {args.target}", TEST_DIR) != 0:
            print("[ERROR] set-target failed.")
            sys.exit(1)

    print("--> Building test app...")
    if run("idf.py build", TEST_DIR) != 0:
        print("[ERROR] Build failed.")
        sys.exit(1)

    if args.flash:
        print("--> Flashing...")
        if run("idf.py flash", TEST_DIR) != 0:
            print("[ERROR] Flash failed.")
            sys.exit(1)

    if args.monitor:
        print("--> Opening monitor (Ctrl-] to exit)...")
        run("idf.py monitor", TEST_DIR)


if __name__ == "__main__":
    main()

