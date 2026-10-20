# ==============================================================================
# Eran OS - Master Build System
# Architecture: x86 (IA-32) Freestanding Kernel & Protected Mode Bootloader
# ==============================================================================

ASM        := nasm
CC         := gcc
LD         := ld
QEMU       := qemu-system-i386
PYTHON     := python3

CFLAGS     := -m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector \
              -nostdlib -O2 -Wall -Wextra -I.

BUILD_DIR  := build
BOOT_DIR   := boot
KERNEL_DIR := kernel
DRV_DIR    := drivers
HAL_DIR    := hal
MM_DIR     := mm
FS_DIR     := fs
NET_DIR    := net
UI_DIR     := ui
USER_DIR   := user
GAMES_DIR  := games
PROC_DIR   := process
TOOLS_DIR  := tools

BOOT_SRC   := $(BOOT_DIR)/boot.asm
BOOT_BIN   := $(BUILD_DIR)/boot.bin
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
IMG_FILE   := $(BUILD_DIR)/eran_os.img

# Kernel Object Files
KERNEL_OBJS := $(BUILD_DIR)/entry.o \
               $(BUILD_DIR)/isr.o \
               $(BUILD_DIR)/kernel.o \
               $(BUILD_DIR)/gdt.o \
               $(BUILD_DIR)/idt.o \
               $(BUILD_DIR)/pic.o \
               $(BUILD_DIR)/syscall.o \
               $(BUILD_DIR)/elf.o \
               $(BUILD_DIR)/process.o \
               $(BUILD_DIR)/shell.o \
               $(BUILD_DIR)/vga.o \
               $(BUILD_DIR)/serial.o \
               $(BUILD_DIR)/timer.o \
               $(BUILD_DIR)/keyboard.o \
               $(BUILD_DIR)/mouse.o \
               $(BUILD_DIR)/ata.o \
               $(BUILD_DIR)/pmm.o \
               $(BUILD_DIR)/heap.o \
               $(BUILD_DIR)/vmm.o \
               $(BUILD_DIR)/pci.o \
               $(BUILD_DIR)/usb.o \
               $(BUILD_DIR)/e1000.o \
               $(BUILD_DIR)/net.o \
               $(BUILD_DIR)/tcp.o \
               $(BUILD_DIR)/audio.o \
               $(BUILD_DIR)/gfx.o \
               $(BUILD_DIR)/desktop.o \
               $(BUILD_DIR)/browser.o \
               $(BUILD_DIR)/acpi.o \
               $(BUILD_DIR)/cpu.o \
               $(BUILD_DIR)/longmode.o \
               $(BUILD_DIR)/ai_engine.o \
               $(BUILD_DIR)/micro_ai.o \
               $(BUILD_DIR)/snake.o \
               $(BUILD_DIR)/snakes_ladders.o \
               $(BUILD_DIR)/space_impact.o \
               $(BUILD_DIR)/vfs.o

.PHONY: all dirs clean run run-cli debug test test-ai run-ai help

all: dirs $(IMG_FILE)

dirs:
	@mkdir -p $(BUILD_DIR)

# 1. Assemble Stage 1 Boot Sector (512 bytes MBR)
$(BOOT_BIN): $(BOOT_SRC) | dirs
	@echo "  [ASM]  $< -> $@"
	$(ASM) $< -f bin -o $@
	@SIZE=$$(wc -c < "$@"); \
	if [ "$$SIZE" -ne 512 ]; then \
		echo "ERROR: Bootloader binary size is $$SIZE bytes, must be exactly 512 bytes!"; \
		exit 1; \
	fi

# 2. Assemble 32-bit Kernel Assembly Stubs
$(BUILD_DIR)/entry.o: $(KERNEL_DIR)/entry.asm | dirs
	@echo "  [ASM]  $< -> $@"
	$(ASM) $< -f elf32 -o $@

$(BUILD_DIR)/isr.o: $(KERNEL_DIR)/isr.asm | dirs
	@echo "  [ASM]  $< -> $@"
	$(ASM) $< -f elf32 -o $@

# 3. Compile Kernel C Sources
$(BUILD_DIR)/kernel.o: $(KERNEL_DIR)/kernel.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/gdt.o: $(KERNEL_DIR)/gdt.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: $(KERNEL_DIR)/idt.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pic.o: $(KERNEL_DIR)/pic.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall.o: $(KERNEL_DIR)/syscall.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/elf.o: $(KERNEL_DIR)/elf.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/process.o: $(PROC_DIR)/process.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: $(KERNEL_DIR)/shell.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga.o: $(DRV_DIR)/vga.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/serial.o: $(DRV_DIR)/serial.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/timer.o: $(DRV_DIR)/timer.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: $(DRV_DIR)/keyboard.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mouse.o: $(DRV_DIR)/mouse.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ata.o: $(DRV_DIR)/ata.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pmm.o: $(MM_DIR)/pmm.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/heap.o: $(MM_DIR)/heap.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vmm.o: $(MM_DIR)/vmm.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pci.o: $(HAL_DIR)/pci.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/usb.o: $(DRV_DIR)/usb.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/e1000.o: $(DRV_DIR)/e1000.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/net.o: $(NET_DIR)/net.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tcp.o: $(NET_DIR)/tcp.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/audio.o: $(DRV_DIR)/audio.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/gfx.o: $(DRV_DIR)/gfx.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/desktop.o: $(UI_DIR)/desktop.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/browser.o: $(NET_DIR)/browser.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/acpi.o: $(HAL_DIR)/acpi.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cpu.o: $(HAL_DIR)/cpu.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/longmode.o: $(HAL_DIR)/longmode.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ai_engine.o: $(USER_DIR)/ai_engine.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/micro_ai.o: $(USER_DIR)/micro_ai.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/snake.o: $(GAMES_DIR)/snake.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/snakes_ladders.o: $(GAMES_DIR)/snakes_ladders.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/space_impact.o: $(GAMES_DIR)/space_impact.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vfs.o: $(FS_DIR)/vfs.c | dirs
	@echo "  [CC]   $< -> $@"
	$(CC) $(CFLAGS) -c $< -o $@

# 4. Link 32-bit Freestanding Kernel Binary
$(KERNEL_BIN): $(KERNEL_OBJS) $(KERNEL_DIR)/linker.ld
	@echo "  [LD]   Linking Kernel: $@"
	$(LD) -m elf_i386 -T $(KERNEL_DIR)/linker.ld --oformat binary -o $@ $(KERNEL_OBJS)

# 5. Construct 1.44MB Bootable Operating System Image
$(IMG_FILE): $(BOOT_BIN) $(KERNEL_BIN)
	@echo "  [IMG]  Creating bootable OS floppy image: $@"
	dd if=/dev/zero of=$@ bs=1024 count=1440 status=none
	dd if=$(BOOT_BIN) of=$@ bs=512 count=1 conv=notrunc status=none
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc status=none

# Run in Graphical Mode (VGA Window + Keyboard input)
run: all
	$(QEMU) -drive format=raw,if=floppy,file=$(IMG_FILE) -net nic,model=e1000 -net user

# Run in Headless CLI Mode (COM1 Serial on stdio)
run-cli: all
	$(QEMU) -drive format=raw,if=floppy,file=$(IMG_FILE) -net nic,model=e1000 -net user -display none -serial stdio

# Run with GDB Debug Server (waits on localhost:1234)
debug: all
	$(QEMU) -drive format=raw,if=floppy,file=$(IMG_FILE) -net nic,model=e1000 -net user -s -S

# Run Kernel Automated Test Suite
test: all
	$(PYTHON) $(TOOLS_DIR)/test_boot.py

# Run AI Shell Test Suite
test-ai:
	$(PYTHON) $(TOOLS_DIR)/test_ai.py

# Run Graphical Desktop UI Test Suite
test-ui: all
	$(PYTHON) $(TOOLS_DIR)/test_desktop.py

# Run Mini Linux & Web Browser Test Suite
test-linux: all
	$(PYTHON) $(TOOLS_DIR)/test_linux.py

# Run Interactive Offline AI Shell
run-ai:
	$(PYTHON) ai-shell/src/eran_shell/main.py

# Clean Build Artifacts
clean:
	@echo "  [CLEAN] Removing build directory"
	rm -rf $(BUILD_DIR)

help:
	@echo "Eran OS Build Targets:"
	@echo "  all       - Build bootloader, kernel, and bootable disk image (default)"
	@echo "  run       - Launch Eran OS in QEMU graphical window"
	@echo "  run-cli   - Launch Eran OS headlessly with COM1 serial on stdio"
	@echo "  debug     - Launch QEMU with GDB server on localhost:1234"
	@echo "  test      - Run automated kernel regression test suite"
	@echo "  test-ai   - Run offline AI Shell test suite"
	@echo "  test-ui   - Run graphical desktop UI test suite"
	@echo "  test-linux- Run Mini Linux & Web Browser test suite"
	@echo "  run-ai    - Launch interactive offline AI productivity shell"
	@echo "  clean     - Remove build artifacts"
