# Platform Build Config - QEMU virt (AArch64)

# Auto-detect cross-compiler: prefer aarch64-linux-gnu- (apt), fallback to aarch64-none-elf-
CROSS_COMPILER ?= $(shell command -v aarch64-linux-gnu-gcc >/dev/null 2>&1 && echo aarch64-linux-gnu- || echo aarch64-none-elf-)

MEM_BASE = 0x40000000

QEMU = qemu-system-aarch64
QEMU_MACHINE = virt,gic-version=3,secure=on,virtualization=on
QEMU_CPU = max
QEMU_TIMEOUT ?= 10
QEMU_OPTS = -M $(QEMU_MACHINE) -cpu $(QEMU_CPU) -m 256M -smp 1 \
            -nographic -kernel
