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
# ----------------------------------------
# Sources (STRICT isolation)
# ----------------------------------------
ESP_ROOTS := main components
SIM_ROOTS := simulator/main simulator/components
C_FILES := -name '*.c'
define find_esp_sources
find $(ESP_ROOTS) \( $(C_FILES) \) \
  -not -path "*/managed_components/*" \
  -not -path "*/build/*" \
  2>/dev/null
endef
define find_sim_sources
find $(SIM_ROOTS) \( $(C_FILES) \) \
  -not -path "*/managed_components/*" \
  -not -path "*/build/*" \
  2>/dev/null
endef
# ----------------------------------------
# FORMAT
# ----------------------------------------
.PHONY: format-check format-fix
format-check:
	@echo "Checking formatting..."
	@FILES="$$( $(find_esp_sources); $(find_sim_sources) )"; \
	echo "$$FILES" | xargs clang-format --dry-run --Werror && \
	echo "[OK] format clean"
format-fix:
	@echo "Formatting..."
	@$(find_esp_sources); $(find_sim_sources) | xargs clang-format -i
	@echo "[OK] formatted"
# ----------------------------------------
# CLANG-TIDY
# ----------------------------------------
TIDY_FLAGS = \
	-extra-arg=-Wno-error

build/compile_commands_tidy.json: build/compile_commands.json
	@python3 scripts/fix_compile_commands.py $< $@

simulator/build/compile_commands_tidy.json: simulator/build/compile_commands.json
	@python3 scripts/fix_compile_commands.py $< $@

.PHONY: tidy-check tidy-fix
tidy-check: build/compile_commands_tidy.json simulator/build/compile_commands_tidy.json
	@echo "Running clang-tidy (STRICT MODE)..."
	@RESULT=0; \
	for file in $$($(find_esp_sources)); do \
		clang-tidy -p build/compile_commands_tidy.json $(TIDY_FLAGS) $$file || RESULT=1; \
	done; \
	for file in $$($(find_sim_sources)); do \
		clang-tidy -p simulator/build/compile_commands_tidy.json $(TIDY_FLAGS) $$file || RESULT=1; \
	done; \
	if [ $$RESULT -eq 0 ]; then \
		echo "[OK] tidy clean"; \
	else \
		echo "[FAIL] issues found"; \
	fi; \
	exit $$RESULT
tidy-fix: build/compile_commands_tidy.json simulator/build/compile_commands_tidy.json
	@echo "Running clang-tidy fixes..."
	@for file in $$($(find_esp_sources)); do \
		clang-tidy -p build/compile_commands_tidy.json $(TIDY_FLAGS) $$file -fix -fix-errors; \
	done; \
	for file in $$($(find_sim_sources)); do \
		clang-tidy -p simulator/build/compile_commands_tidy.json $(TIDY_FLAGS) $$file -fix -fix-errors; \
	done; \
	echo "[OK] fixes applied"
