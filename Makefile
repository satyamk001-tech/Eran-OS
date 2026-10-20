# ==============================================================================
# Eran OS - Master Makefile
# ==============================================================================

ASM        := nasm
QEMU       := qemu-system-i386
PYTHON     := python3

SRC_DIR    := src
BUILD_DIR  := build
TOOLS_DIR  := tools
DOCS_DIR   := docs

BOOT_SRC   := $(SRC_DIR)/main.asm
BOOT_BIN   := $(BUILD_DIR)/main.bin
FLOPPY_IMG := $(BUILD_DIR)/main_floppy.img

.PHONY: all dirs clean run run-cli debug test help

# Default target: build boot sector and floppy disk image
all: dirs $(FLOPPY_IMG)

# Ensure build directory exists
dirs:
	@mkdir -p $(BUILD_DIR)

# Assemble 512-byte MBR boot sector
$(BOOT_BIN): $(BOOT_SRC) | dirs
	@echo "  [ASM]  $< -> $@"
	$(ASM) $< -f bin -o $@
	@SIZE=$$(wc -c < "$@"); \
	if [ "$$SIZE" -ne 512 ]; then \
		echo "ERROR: Bootloader binary size is $$SIZE bytes, must be exactly 512 bytes!"; \
		exit 1; \
	fi

# Generate 1.44MB raw floppy disk image
$(FLOPPY_IMG): $(BOOT_BIN)
	@echo "  [IMG]  Creating 1.44MB floppy image: $@"
	cp $(BOOT_BIN) $(FLOPPY_IMG)
	truncate -s 1440k $(FLOPPY_IMG)

# Run Eran OS in QEMU graphical mode
run: all
	$(QEMU) -drive format=raw,if=floppy,file=$(FLOPPY_IMG)

# Run Eran OS in headless CLI mode with COM1 serial redirected to stdio
run-cli: all
	$(QEMU) -drive format=raw,if=floppy,file=$(FLOPPY_IMG) -display none -serial stdio

# Run Eran OS with GDB debugging enabled (waits for GDB on localhost:1234)
debug: all
	$(QEMU) -drive format=raw,if=floppy,file=$(FLOPPY_IMG) -s -S

# Run automated test suite
test: all
	$(PYTHON) $(TOOLS_DIR)/test_boot.py

# Clean build artifacts
clean:
	@echo "  [CLEAN] Removing build directory"
	rm -rf $(BUILD_DIR)

# Help display
help:
	@echo "Eran OS Build System"
	@echo "Targets:"
	@echo "  all       - Build bootloader binary and 1.44MB floppy image (default)"
	@echo "  run       - Launch Eran OS in QEMU with graphical window"
	@echo "  run-cli   - Launch Eran OS in QEMU headlessly with COM1 serial on stdio"
	@echo "  debug     - Launch QEMU paused, waiting for GDB connection on :1234"
	@echo "  test      - Execute automated verification test suite"
	@echo "  clean     - Remove build artifacts"
