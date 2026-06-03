#!/usr/bin/env python3
import argparse
import os
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
TEST_DIR = os.path.join(PROJECT_ROOT, "test")
BUILD_DIR = os.path.join(TEST_DIR, "build")

def run_command(cmd, cwd):
    """Runs a shell command and streams the output in real-time."""
    try:
        process = subprocess.Popen(
            cmd,
            shell=True,
            stdout=sys.stdout,
            stderr=sys.stderr,
            cwd=cwd
        )
        process.communicate()
        return process.returncode
    except KeyboardInterrupt:
        print("\n\n[!] Test execution interrupted by user.")
        return 130

def main():
    parser = argparse.ArgumentParser(description="ESP32-S3 Test Runner Wrapper")
    parser.add_argument("--target", default="esp32s3", help="Target chip")
    parser.add_argument("--flash", action="store_true", help="Flash the device")
    parser.add_argument("--monitor", action="store_true", help="Open monitor")
    args = parser.parse_args()

    cmake_cache = os.path.join(BUILD_DIR, "CMakeCache.txt")
    
    # If the build directory exists but is broken/half-baked, wipe it out via Python
    if os.path.exists(BUILD_DIR) and not os.path.exists(cmake_cache):
        print("--> Found half-baked or broken build directory. Cleaning via Python...")
        shutil.rmtree(BUILD_DIR)

    # Step 1: Only set the target if it's a completely fresh start
    if not os.path.exists(BUILD_DIR):
        print(f"--> Initializing a clean build directory for {args.target}...")
        if run_command(f"idf.py set-target {args.target}", TEST_DIR) != 0:
            print("[X] Failed to set target.")
            sys.exit(1)
    else:
        print("--> Build directory is valid. Ready for incremental build.")

    # Step 2: Incremental Build (Fast!)
    print("--> Building unit test app...")
    if run_command("idf.py build", TEST_DIR) != 0:
        print("[X] Build failed.")
        sys.exit(1)

    # Step 3: Optional Flash
    if args.flash:
        print("--> Flashing device...")
        if run_command("idf.py flash", TEST_DIR) != 0:
            print("[X] Flashing failed.")
            sys.exit(1)

    # Step 4: Optional Monitor
    if args.monitor:
        print("--> Launching monitor...")
        run_command("idf.py monitor", TEST_DIR)

if __name__ == "__main__":
    main()
