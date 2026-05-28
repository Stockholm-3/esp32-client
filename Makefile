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
  -not -path "*/bingus-lib/*" \
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
.PHONY: build reconfigure flash monitor flash-monitor fm
.PHONY: linux-build linux-run linux-clean linux-hardclean
.PHONY: hardclean format-check format-fix format-ci
.PHONY: lint lint-fix lint-ci lint-scrub lint-check-deps linux-reconfigure

# ----------------------------------------
# Build / Flash / Monitor
# ----------------------------------------
build:
	idf.py build

reconfigure:
	idf.py reconfigure

flash:
	idf.py flash

monitor:
	idf.py monitor

flash-monitor:
	idf.py flash monitor

fm: flash-monitor

##Overides and updates the certs for the http_client.
.PHONY: update-certs
update-certs:
	curl -s https://pki.goog/repo/certs/gtsr1.pem https://pki.goog/repo/certs/gtsr2.pem https://pki.goog/repo/certs/gtsr3.pem https://pki.goog/repo/certs/gtsr4.pem https://letsencrypt.org/certs/isrgrootx1.pem https://letsencrypt.org/certs/isrg-root-x2.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem https://cacerts.digicert.com/DigiCertGlobalRootCA.crt.pem > components/http_client/certs/roots.pem

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
	mkdir -p managed_components simulator/managed_components
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

# Prefer the Xtensa-specific clang-tidy if available, fall back to host clang-tidy.
CLANG_TIDY_EXE ?= clang-tidy

# Only report diagnostics in our own source tree; skip managed_components.
HEADER_FILTER  := ^$(PROJECT_ROOT)/(main|components)/(?!managed_components)

# Extra args passed to every clang-tidy invocation via run-clang-tidy.
# These suppress the cascade of errors that originate in newlib/Xtensa/nix
# headers so they never reach our filter script at all.
TIDY_EXTRA_ARGS := \
  -extra-arg="-Wno-unknown-attributes" \
  -extra-arg="-Wno-unknown-pragmas" \
  -extra-arg="-Wno-ignored-attributes" \
  -extra-arg="-Wno-error"

lint-check-deps:
	@command -v run-clang-tidy >/dev/null 2>&1 || \
	  { echo "[ERROR] run-clang-tidy not found."; exit 1; }
	@[ -n "$(CLANG_TIDY_EXE)" ] || \
	  { echo "[ERROR] No clang-tidy binary found in PATH."; exit 1; }
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

# -----------------------------------------------------------------------
# lint — run clang-tidy, show all findings, fail on any warning or error
# -----------------------------------------------------------------------
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
	    $(TIDY_EXTRA_ARGS) \
	    -quiet \
	  2>&1 \
	  | python3 $(FILTER_SCRIPT) --root "$(PROJECT_ROOT)" --force-color \
	  | tee "$$TMPFILE"; \
	FINDINGS=$$(grep -cP ":\d+:\d+:\s+(warning|error):" "$$TMPFILE" 2>/dev/null || true); \
	rm -f "$$TMPFILE"; \
	if [ "$${FINDINGS:-0}" -gt 0 ]; then \
	  echo "[FAIL] clang-tidy found $${FINDINGS} issue(s)"; exit 1; \
	fi; \
	echo "[OK] clang-tidy clean"

# -----------------------------------------------------------------------
# lint-ci — errors block, warnings are advisory
# -----------------------------------------------------------------------
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
			-header-filter "$(HEADER_FILTER)" \
			$(TIDY_EXTRA_ARGS) \
			-quiet \
		2>&1 \
		| python3 $(FILTER_SCRIPT) --root "$(PROJECT_ROOT)" \
		| tee "$$TMPFILE"; \
	WARNINGS=$$(grep -E -c "warning:" "$$TMPFILE" || true); \
	ERRORS=$$(grep -E -c "error:" "$$TMPFILE" || true); \
	rm -f "$$TMPFILE"; \
	if [ "$${ERRORS:-0}" -gt 0 ]; then \
		echo "[FAIL] clang-tidy: $${ERRORS} error(s) must be fixed (warnings: $${WARNINGS:-0})"; \
		exit 1; \
	elif [ "$${WARNINGS:-0}" -gt 0 ]; then \
		echo "[WARN] clang-tidy: $${WARNINGS} warning(s) — non-blocking"; \
	else \
		echo "[OK] clang-tidy clean"; \
	fi

# -----------------------------------------------------------------------
# lint-fix — apply safe automatic fixes
# -----------------------------------------------------------------------
lint-fix: lint-scrub
	@echo "Running clang-tidy with auto-fix (project files only)..."
	@SOURCE_FILES="$$($(call find_sources))"; \
	if [ -z "$$SOURCE_FILES" ]; then echo "[SKIP] No source files found"; exit 0; fi; \
	echo "$$SOURCE_FILES" | tr '\n' '\0' | xargs -0 \
	  run-clang-tidy \
	    -clang-tidy-binary "$(CLANG_TIDY_EXE)" \
	    -p "$(LINT_DB_DIR)" \
	    -checks='' \
	    -header-filter "$(HEADER_FILTER)" \
	    -source-filter "$(HEADER_FILTER)" \
	    $(TIDY_EXTRA_ARGS) \
	    -fix \
	    -format \
	    -quiet \
	  2>&1 \
	  | python3 $(FILTER_SCRIPT) --root "$(PROJECT_ROOT)" --force-color; \
	echo "[OK] fixes applied (if any)"
# ------------------------------------------------------------
# Documentation
# ------------------------------------------------------------
.PHONY: docs
docs:
	@echo "Generating documentation..."
	@doxygen
	@echo "Documentation generated in docs/html/index.html"

.PHONY: docs-clean
docs-clean:
	@echo "Removing documentation..."
	@rm -rf docs
	@echo "Documentation removed."

.PHONY: docs-open
docs-open:
	@echo "Opening documentation..."
	@xdg-open docs/html/index.html
	@echo "Documentation opened in default browser."
