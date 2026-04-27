SHELL := bash

PROJECT_NAME := app-template

# ----------------------------------------
# Source discovery
# ----------------------------------------
ROOTS := main components

define find_sources
find $(ROOTS) -name '*.c' \
  -not -path "*/managed_components/*" \
  -not -path "*/build/*" \
  -not -path "*/squareline/*" \
  -not -path "*/lib/*" \
  2>/dev/null
endef

define find_headers
find $(ROOTS) -name '*.h' \
  -not -path "*/managed_components/*" \
  -not -path "*/build/*" \
  -not -path "*/squareline/*" \
  -not -path "*/lib/*" \
  2>/dev/null
endef

# ----------------------------------------
# Phony targets
# ----------------------------------------
.PHONY: build flash monitor flash-monitor fm
.PHONY: linux-build linux-run linux-clean linux-hardclean
.PHONY: hardclean format-check format-fix format-ci
.PHONY: lint lint-fix lint-ci lint-scrub lint-check-deps linux-reconfigure

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
	cd simulator && IDF_TARGET=linux idf.py build
linux-reconfigure:
	cd simulator && IDF_TARGET=linux idf.py reconfigure

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

format-ci:
	@echo "Checking formatting (CI)..."
	@FILES="$$($(call find_sources))"; \
	if [ -z "$$FILES" ]; then echo "[SKIP] No source files found"; exit 0; fi; \
	if echo "$$FILES" | xargs clang-format --dry-run --Werror 2>&1; then \
	  echo "[OK] formatting clean"; \
	else \
	  echo "[FAIL] formatting issues found — run 'make format-fix' locally and commit the result"; \
	  exit 1; \
	fi

# ----------------------------------------
# Static analysis (clang-tidy)
# ----------------------------------------
BUILD_DIR      := build
LINT_DB_DIR    := $(BUILD_DIR)/lint
COMPILE_DB_RAW := $(BUILD_DIR)/compile_commands.json
SCRUB_SCRIPT   := scripts/scrub_compile_commands.py
FILTER_SCRIPT  := scripts/filter_lint.py
PROJECT_ROOT   := $(shell pwd)

# Surgical fix: Explicitly look for the Xtensa binary
CLANG_TIDY_EXE := $(shell which xtensa-esp32s3-elf-clang-tidy 2>/dev/null || which clang-tidy)

HEADER_FILTER  := ^$(PROJECT_ROOT)/(main|components)/(?!managed_components)

lint-check-deps:
	@command -v run-clang-tidy >/dev/null 2>&1 || \
	  { echo "[ERROR] run-clang-tidy wrapper not found."; exit 1; }
	@if [ -z "$(CLANG_TIDY_EXE)" ]; then echo "[ERROR] No clang-tidy binary found in PATH."; exit 1; fi
	@test -f $(COMPILE_DB_RAW) || \
	  { echo "[ERROR] $(COMPILE_DB_RAW) not found. Run 'idf.py reconfigure' first."; exit 1; }
	@test -f $(SCRUB_SCRIPT) || \
	  { echo "[ERROR] $(SCRUB_SCRIPT) missing."; exit 1; }
	@test -f $(FILTER_SCRIPT) || \
	  { echo "[ERROR] $(FILTER_SCRIPT) missing."; exit 1; }

lint-scrub: lint-check-deps
	@echo "Scrubbing compile_commands.json -> $(LINT_DB_DIR)/compile_commands.json ..."
	@mkdir -p $(LINT_DB_DIR)
	@python3 $(SCRUB_SCRIPT) \
		--input  $(COMPILE_DB_RAW) \
		--output $(LINT_DB_DIR)/compile_commands.json \
		--roots  $(ROOTS)

lint: lint-scrub
	@echo "Running clang-tidy using $(CLANG_TIDY_EXE)..."
	@SOURCE_FILES="$$($(call find_sources))"; \
	if [ -z "$$SOURCE_FILES" ]; then echo "[SKIP] No source files found"; exit 0; fi; \
	TMPFILE=$$(mktemp /tmp/lint.XXXXXX); \
	set -o pipefail; \
	echo "$$SOURCE_FILES" | tr '\n' '\0' | xargs -0 \
	  run-clang-tidy \
	    -clang-tidy-binary "$(CLANG_TIDY_EXE)" \
	    -p "$(LINT_DB_DIR)" \
	    -checks='' \
	    -header-filter "$(HEADER_FILTER)" \
	    -quiet \
	  2>&1 \
	  | python3 $(FILTER_SCRIPT) --root "$(PROJECT_ROOT)" \
	  | tee "$$TMPFILE"; \
	FINDINGS=$$(grep -cP ":\d+:\d+:\s+(warning|error):" "$$TMPFILE" 2>/dev/null || true); \
	rm -f "$$TMPFILE"; \
	if [ "$${FINDINGS:-0}" -gt 0 ]; then \
	  echo "[FAIL] clang-tidy found $${FINDINGS} issue(s)"; exit 1; \
	fi; \
	echo "[OK] clang-tidy clean"

lint-ci: lint-scrub
	@echo "Running clang-tidy (CI) using $(CLANG_TIDY_EXE)..."
	@SOURCE_FILES="$$($(call find_sources))"; \
	if [ -z "$$SOURCE_FILES" ]; then echo "[SKIP] No source files found"; exit 0; fi; \
	TMPFILE=$$(mktemp /tmp/lint.XXXXXX); \
	set -o pipefail; \
	echo "$$SOURCE_FILES" | tr '\n' '\0' | xargs -0 \
	  run-clang-tidy \
	    -clang-tidy-binary "$(CLANG_TIDY_EXE)" \
	    -p "$(LINT_DB_DIR)" \
	    -checks='' \
	    -header-filter "$(HEADER_FILTER)" \
	    -quiet \
	  2>&1 \
	  | python3 $(FILTER_SCRIPT) --root "$(PROJECT_ROOT)" \
	  | tee "$$TMPFILE"; \
	WARNINGS=$$(grep -cP ":\d+:\d+:\s+warning:" "$$TMPFILE" 2>/dev/null || true); \
	ERRORS=$$(grep -cP ":\d+:\d+:\s+error:" "$$TMPFILE" 2>/dev/null || true); \
	rm -f "$$TMPFILE"; \
	if [ "$${ERRORS:-0}" -gt 0 ]; then \
	  echo "[FAIL] clang-tidy: $${ERRORS} error(s) must be fixed (warnings: $${WARNINGS:-0})"; \
	  exit 1; \
	elif [ "$${WARNINGS:-0}" -gt 0 ]; then \
	  echo "[WARN] clang-tidy: $${WARNINGS} warning(s) — non-blocking"; \
	else \
	  echo "[OK] clang-tidy clean"; \
	fi

lint-fix: lint-scrub
	@echo "Running clang-tidy with auto-fix..."
	@SOURCE_FILES="$$($(call find_sources))"; \
	if [ -z "$$SOURCE_FILES" ]; then echo "[SKIP] No source files found"; exit 0; fi; \
	echo "$$SOURCE_FILES" | tr '\n' '\0' | xargs -0 \
	  run-clang-tidy \
	    -clang-tidy-binary "$(CLANG_TIDY_EXE)" \
	    -p "$(LINT_DB_DIR)" \
	    -checks='' \
	    -header-filter "$(HEADER_FILTER)" \
	    -fix \
	    -quiet \
	  && echo "[OK] fixes applied"
