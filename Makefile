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
#   lint      — show all warnings + errors (dev use). Fails if any found.
#   lint-ci   — show all warnings + errors (CI use). Fails only on errors.
#   lint-fix  — apply safe automatic fixes in-place. Never use in CI.
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

# lint — show all warnings and errors. Fails if anything is found.
# If you see [FAIL] with no diagnostics listed, run:
#   make lint 2>&1 | cat
# to bypass the filter and see raw clang-tidy output.
lint: lint-scrub
	@echo "Running clang-tidy..."
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
	FINDINGS=$$(grep -cP ":\d+:\d+:\s+(warning|error):" "$$TMPFILE" 2>/dev/null || true); \
	rm -f "$$TMPFILE"; \
	if [ "$${FINDINGS:-0}" -gt 0 ]; then \
	  echo "[FAIL] clang-tidy found $${FINDINGS} issue(s) — see above"; exit 1; \
	fi; \
	echo "[OK] clang-tidy clean — no warnings or errors"

# lint-ci — show all warnings and errors, but only fail on errors.
# Use this in CI pipelines. Warnings are visible but non-blocking.
lint-ci: lint-scrub
	@echo "Running clang-tidy (CI: warnings visible, only errors block the build)..."
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
	WARNINGS=$$(grep -cP ":\d+:\d+:\s+warning:" "$$TMPFILE" 2>/dev/null || true); \
	ERRORS=$$(grep -cP ":\d+:\d+:\s+error:" "$$TMPFILE" 2>/dev/null || true); \
	rm -f "$$TMPFILE"; \
	if [ "$${ERRORS:-0}" -gt 0 ]; then \
	  echo "[FAIL] clang-tidy: $${ERRORS} error(s) must be fixed before merging (warnings: $${WARNINGS:-0})"; \
	  exit 1; \
	elif [ "$${WARNINGS:-0}" -gt 0 ]; then \
	  echo "[WARN] clang-tidy: $${WARNINGS} warning(s) — non-blocking, but worth fixing"; \
	else \
	  echo "[OK] clang-tidy clean — no warnings or errors"; \
	fi

# lint-fix — apply safe automatic fixes in-place. Never use in CI.
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
