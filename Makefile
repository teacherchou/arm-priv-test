# ARM Privilege Test Framework - Top-level Makefile
#
# Usage:
#   make all                 # Build all extensions
#   make sysreg              # Build sysreg extension
#   make irq                 # Build irq extension
#   make el2                 # Build el2 extension
#   make el3                 # Build el3 extension
#   make sysreg-qemu         # Build and run sysreg on QEMU
#   make test                # Build and run ALL tests on QEMU
#   make clean               # Clean all

EXTENSIONS = sysreg irq el2 el3 ecc
QEMU_TIMEOUT ?= 10

.PHONY: all clean test $(EXTENSIONS)

all: $(EXTENSIONS)

$(EXTENSIONS):
	$(MAKE) -C $@

# QEMU run targets (single extension)
%-qemu:
	$(MAKE) -C $* qemu

# Run ALL tests sequentially with timeout
test: all
	@pass=0; fail=0; skip=0; total=0; \
	for ext in $(EXTENSIONS); do \
		echo ""; \
		echo "=== Running $ext tests ==="; \
		$(MAKE) -C $ext qemu 2>&1 & pid=$!; \
		sleep $(QEMU_TIMEOUT) && kill $pid 2>/dev/null & watcher=$!; \
		wait $pid 2>/dev/null; \
		kill $watcher 2>/dev/null; wait $watcher 2>/dev/null; \
	done; \
	echo ""; echo "=== All test suites complete ==="

# Docker build & run
docker-build:
	docker build -t arm-priv-test -f common/Dockerfile .

docker-run: docker-build
	docker run -d --name arm-build -v $(pwd):/workspace arm-priv-test tail -f /dev/null

docker-test:
	docker exec arm-build make test

# Clean all
clean:
	@for ext in $(EXTENSIONS); do \
		$(MAKE) -C $ext clean; \
	done