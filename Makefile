PROJECT_NAME := app-template

# ----------------------------------------
# Source discovery
# ----------------------------------------
ROOTS := main components

define find_sources
find $(ROOTS) -name '*.c' \
  -not -path "*/managed_components/*" \
  -not -path "*/build/*" \
  2>/dev/null
endef

define find_headers
find $(ROOTS) -name '*.h' \
  -not -path "*/managed_components/*" \
  -not -path "*/build/*" \
  2>/dev/null
endef

# ----------------------------------------
# Phony targets
# ----------------------------------------
.PHONY: build flash monitor flash-monitor fm
.PHONY: linux-build linux-run linux-clean linux-hardclean
.PHONY: hardclean format-check format-fix
.PHONY: lint lint-fix lint-ci lint-scrub lint-check-deps

# ----------------------------------------
# Build / Flash / Monitor
# ----------------------------------------
build:
	idf.py build

flash:
	idf.py flash

monitor:
	idf.py monitor

flash-monitor:
	idf.py flash monitor

fm: flash-monitor

# ----------------------------------------
# Clean
# ----------------------------------------
hardclean:
	rm -rf managed_components build sdkconfig

# ----------------------------------------
# Linux simulator targets
# ----------------------------------------
linux-build:
	cd simulator && idf.py build

linux-run: linux-build
	./simulator/build/simulator.elf

linux-clean:
	rm -rf simulator/build

linux-hardclean:
	rm -rf ./simulator/managed_components ./simulator/build ./simulator/sdkconfig

# ----------------------------------------
# Formatting
# ----------------------------------------
format-check:
	@echo "Checking formatting..."
	@FILES="$$($(call find_sources))"; \
	if [ -z "$$FILES" ]; then echo "[SKIP] No source files found"; exit 0; fi; \
	echo "$$FILES" | xargs clang-format --dry-run --Werror && \
	echo "[OK] format clean"

format-fix:
	@echo "Formatting..."
	@FILES="$$($(call find_sources))"; \
	if [ -z "$$FILES" ]; then echo "[SKIP] No source files found"; exit 0; fi; \
	echo "$$FILES" | xargs clang-format -i
	@echo "[OK] formatted"

# ----------------------------------------
# Static analysis (clang-tidy)
#
# Three-layer defence against ESP-IDF/Xtensa noise:
#
#   1. scrub_compile_commands.py
#      - Expands @response-files that hide Xtensa GCC flags
#      - Strips all flags unknown to clang
#      - Injects -isystem paths for the Xtensa newlib sysroot
#      - Keeps only entries for YOUR files
#      Output: build/lint/compile_commands.json
#
#   2. -header-filter regex
#      Tells clang-tidy to only EMIT diagnostics for headers under
#      your project root.  Foreign headers are still parsed (clang
#      needs to understand them) but never reported on.
#
#   3. filter_lint.py
#      Post-processes clang-tidy stdout+stderr, drops all notes, and
#      colorizes warnings (yellow) and errors (red) when writing to a
#      terminal.
#
# TARGETS
# -------
#   lint        — warnings + errors, non-zero exit on any finding (dev use)
#   lint-fix    — apply safe automatic fixes in-place (never use in CI)
#   lint-ci     — errors only, non-zero exit only on errors (CI use)
#
# WORKFLOW
# --------
#   idf.py reconfigure        # once, or after CMakeLists changes
#   make lint                 # scrub → run → filter
# ----------------------------------------

BUILD_DIR      := build
LINT_DB_DIR    := $(BUILD_DIR)/lint
COMPILE_DB_RAW := $(BUILD_DIR)/compile_commands.json
SCRUB_SCRIPT   := scripts/scrub_compile_commands.py
FILTER_SCRIPT  := scripts/filter_lint.py
PROJECT_ROOT   := $(shell pwd)

HEADER_FILTER  := ^$(PROJECT_ROOT)/(main|components)/(?!managed_components)

lint-check-deps:
	@command -v run-clang-tidy >/dev/null 2>&1 || \
	  { echo "[ERROR] run-clang-tidy not found. Add libclang to your flake buildInputs."; exit 1; }
	@test -f $(COMPILE_DB_RAW) || \
	  { echo "[ERROR] $(COMPILE_DB_RAW) not found. Run 'idf.py reconfigure' first."; exit 1; }
	@test -f $(SCRUB_SCRIPT) || \
	  { echo "[ERROR] $(SCRUB_SCRIPT) missing."; exit 1; }
	@test -f $(FILTER_SCRIPT) || \
	  { echo "[ERROR] $(FILTER_SCRIPT) missing."; exit 1; }

lint-scrub: lint-check-deps
	@echo "Scrubbing compile_commands.json -> $(LINT_DB_DIR)/compile_commands.json ..."
	@python3 $(SCRUB_SCRIPT) \
	    --input  $(COMPILE_DB_RAW) \
	    --output $(LINT_DB_DIR)/compile_commands.json \
	    --roots  $(ROOTS)

# lint — report warnings and errors, non-zero exit on any finding (dev use).
lint: lint-scrub
	@echo "Running clang-tidy..."
	@SOURCE_FILES="$$($(call find_sources))"; \
	if [ -z "$$SOURCE_FILES" ]; then echo "[SKIP] No source files found"; exit 0; fi; \
	set -o pipefail; \
	echo "$$SOURCE_FILES" | tr '\n' '\0' | xargs -0 \
	  run-clang-tidy \
	    -p "$(LINT_DB_DIR)" \
	    -header-filter "$(HEADER_FILTER)" \
	    -quiet \
	  2>&1 | python3 $(FILTER_SCRIPT) --root "$(PROJECT_ROOT)"; \
	EXIT=$$?; \
	if [ $$EXIT -ne 0 ]; then echo "[FAIL] clang-tidy found issues"; exit $$EXIT; fi; \
	echo "[OK] clang-tidy clean"

# lint-ci — only fail the build on actual errors, not warnings.
# Warnings are still shown for visibility but do not break CI.
lint-ci: lint-scrub
	@echo "Running clang-tidy (CI mode: fail on errors only)..."
	@SOURCE_FILES="$$($(call find_sources))"; \
	if [ -z "$$SOURCE_FILES" ]; then echo "[SKIP] No source files found"; exit 0; fi; \
	TMPFILE=$$(mktemp); \
	echo "$$SOURCE_FILES" | tr '\n' '\0' | xargs -0 \
	  run-clang-tidy \
	    -p "$(LINT_DB_DIR)" \
	    -header-filter "$(HEADER_FILTER)" \
	    -quiet \
	  2>&1 \
	  | python3 $(FILTER_SCRIPT) --root "$(PROJECT_ROOT)" \
	  | tee "$$TMPFILE"; \
	if grep -q "^.*: error:" "$$TMPFILE" 2>/dev/null || \
	   grep -qP "\033\[1m\033\[31merror:" "$$TMPFILE" 2>/dev/null || \
	   grep -P ":\d+:\d+:.*\berror\b:" "$$TMPFILE" >/dev/null 2>&1; then \
	  rm -f "$$TMPFILE"; \
	  echo "[FAIL] clang-tidy found errors"; exit 1; \
	fi; \
	rm -f "$$TMPFILE"; \
	echo "[OK] clang-tidy: no errors (warnings ignored in CI mode)"

# lint-fix — apply safe automatic fixes in-place (do not use in CI).
lint-fix: lint-scrub
	@echo "Running clang-tidy with auto-fix..."
	@SOURCE_FILES="$$($(call find_sources))"; \
	if [ -z "$$SOURCE_FILES" ]; then echo "[SKIP] No source files found"; exit 0; fi; \
	echo "$$SOURCE_FILES" | tr '\n' '\0' | xargs -0 \
	  run-clang-tidy \
	    -p "$(LINT_DB_DIR)" \
	    -header-filter "$(HEADER_FILTER)" \
	    -fix \
	    -quiet \
	  && echo "[OK] fixes applied"
