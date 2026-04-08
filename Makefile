#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := app-template


.PHONY: build flash monitor linux-build linux-run linux-clean

build:
	idf.py build

flash:
	idf.py flash

monitor:
	idf.py monitor

flash-monitor:
	idf.py flash monitor

linux-build:
	cd simulator && idf.py build

linux-run: linux-build
	./simulator/build/simulator.elf

linux-clean:
	rm -rf simulator/build

