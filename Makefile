PROJECT_NAME := app-template


.PHONY: build flash monitor linux-build linux-run linux-clean

.PHONY: hardclean
hardclean:
	rm -rf managed_components build sdkconfig

.PHONY: linux-hardclean
linux-hardclean:
	rm -rf ./simulator/managed_components ./simulator/build ./simulator/sdkconfig

.PHONY fm:
	idf.py build flash monitor

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

