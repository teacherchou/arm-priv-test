# ARM Privilege Test Framework - Top-level Makefile
#
# Common usage:
#   make help               List all targets
#   make all                Build all extensions
#   make <ext>              Build one extension (e.g. make sysreg)
#   make <ext>-qemu         Build & run one extension on QEMU; bounded by
#                           QEMU_TIMEOUT, exits 0/1/2 (pass/fail/timeout)
#   make test               Run every extension and aggregate the result
#   make clean              Clean every extension
#
# For raw interactive QEMU (no timeout, Ctrl+A X to quit), run
# qemu-system-aarch64 directly — see docs/onboarding.md §6.

EXTENSIONS    = sysreg irq el2 el3 ecc
QEMU_TIMEOUT ?= 30

.PHONY: all help clean test norm-check coverpoint-generate coverpoint-flow $(EXTENSIONS) \
        docker-build docker-run docker-test

all: $(EXTENSIONS)

$(EXTENSIONS):
	$(MAKE) -C $@

# `make <ext>-qemu` runs that extension via run_qemu.sh
%-qemu:
	$(MAKE) -C $* qemu QEMU_TIMEOUT=$(QEMU_TIMEOUT)

# Run every extension, collect each's "[ext] RESULT: PASS|FAIL" line,
# aggregate, propagate exit code.
test: all
	@total_pass=0; total_fail=0; total_skip=0; failed_exts=""; \
	for ext in $(EXTENSIONS); do \
	  echo ""; echo "=== [$$ext] running ==="; \
	  log=$$(mktemp); \
	  $(MAKE) -s -C $$ext qemu QEMU_TIMEOUT=$(QEMU_TIMEOUT) > $$log 2>&1; \
	  rc=$$?; \
	  cat $$log; \
	  if [ "$$rc" -ne 0 ]; then failed_exts="$$failed_exts $$ext"; fi; \
	  p=$$(grep -E "^[[:space:]]*Passed:" $$log | tail -n1 | awk '{print $$2}' | tr -d '\r'); \
	  f=$$(grep -E "^[[:space:]]*Failed:" $$log | tail -n1 | awk '{print $$2}' | tr -d '\r'); \
	  s=$$(grep -E "^[[:space:]]*Skipped:" $$log | tail -n1 | awk '{print $$2}' | tr -d '\r'); \
	  total_pass=$$((total_pass + $${p:-0})); \
	  total_fail=$$((total_fail + $${f:-0})); \
	  total_skip=$$((total_skip + $${s:-0})); \
	  rm -f $$log; \
	done; \
	echo ""; \
	echo "================================================================"; \
	echo "  TOTAL  PASS=$$total_pass  FAIL=$$total_fail  SKIP=$$total_skip"; \
	if [ -n "$$failed_exts" ]; then \
	  echo "  FAILED EXTENSIONS:$$failed_exts"; \
	  echo "================================================================"; \
	  exit 1; \
	fi; \
	echo "  [total] RESULT: PASS"; \
	echo "================================================================"

norm-check:
	python3 common/scripts/coverpoint_flow.py check

coverpoint-generate:
	python3 common/scripts/coverpoint_flow.py generate

coverpoint-flow:
	python3 common/scripts/coverpoint_flow.py flow

docker-build:
	docker build -t arm-priv-test -f common/Dockerfile .

docker-run: docker-build
	docker run -d --name arm-build -v $$(pwd):/workspace arm-priv-test tail -f /dev/null

docker-test:
	docker exec arm-build make test

clean:
	@for ext in $(EXTENSIONS); do \
	  $(MAKE) -s -C $$ext clean; \
	done

help:
	@echo "ARM Privilege Test Framework — top-level targets"
	@echo ""
	@echo "  make all              Build every extension"
	@echo "  make <ext>            Build one extension ($(EXTENSIONS))"
	@echo "  make <ext>-qemu       Build & run one extension on QEMU"
	@echo "                        (timeout-bounded, exit 0/1/2)"
	@echo "  make test             Run every extension, aggregate result,"
	@echo "                        exit non-zero on any failure"
	@echo "  make clean            Clean every extension"
	@echo "  make norm-check       Validate NORM rule/coverpoint/testcase links"
	@echo "  make coverpoint-generate  Generate SVH skeleton and traceability reports"
	@echo "  make coverpoint-flow  Run full offline CoverPoint check + generation"
	@echo ""
	@echo "  make docker-build     Build the container image"
	@echo "  make docker-run       Start a long-lived build container"
	@echo "  make docker-test      Run \`make test\` inside the container"
	@echo ""
	@echo "Variables:"
	@echo "  QEMU_TIMEOUT=$(QEMU_TIMEOUT)   per-extension timeout in seconds"
	@echo "  CROSS_COMPILER=...   override toolchain prefix"
	@echo ""
	@echo "Raw interactive QEMU: run qemu-system-aarch64 directly with"
	@echo "the .elf — see docs/onboarding.md §6."
	@echo ""
	@echo "First-time onboarding: docs/onboarding.md"
