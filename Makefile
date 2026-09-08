PROJECT := justrt
.DEFAULT_GOAL := all

TARGET ?= s32k312
BUILD ?= debug
ifeq ($(BUILD),debug)
BUILD_SUFFIX :=
OPTFLAGS := -Og -g3
else ifeq ($(BUILD),release)
BUILD_SUFFIX := /release
OPTFLAGS := -O2 -g3 -DNDEBUG
else
$(error Unsupported BUILD=$(BUILD); use BUILD=debug or BUILD=release)
endif
ifeq ($(TARGET),s32k312)
OBJDIR := obj$(BUILD_SUFFIX)
BINDIR := bin$(BUILD_SUFFIX)
else
OBJDIR := obj/$(TARGET)$(BUILD_SUFFIX)
BINDIR := bin/$(TARGET)$(BUILD_SUFFIX)
endif

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
PYTHON ?= py
ifeq ($(TARGET),s32k312)
PLATFORM_DIR := platform/s32k312
CPUFLAGS := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
ARCHFLAGS := -DJRT_ARCH_FPU_CONTEXT=1 -DJRT_ARCH_HAS_MPU=1 -DJRT_ARCH_HAS_DWT_CYCCNT=1
STARTUP_SOURCE := $(PLATFORM_DIR)/startup_cm7.s
VECTOR_SOURCE := $(PLATFORM_DIR)/Vector_Table.s
SYSTEM_SOURCE := $(PLATFORM_DIR)/system.c
LINKER_SCRIPT := $(PLATFORM_DIR)/linker_flash_s32k312.ld
else ifeq ($(TARGET),qemu-mps2-an385)
PLATFORM_DIR := platform/qemu_mps2_an385
CPUFLAGS := -mcpu=cortex-m3 -mthumb
ARCHFLAGS := -DJRT_ARCH_FPU_CONTEXT=0 -DJRT_ARCH_HAS_MPU=0 -DJRT_ARCH_HAS_DWT_CYCCNT=0 -DJRT_CORE_CLOCK_HZ=25000000UL
STARTUP_SOURCE := $(PLATFORM_DIR)/startup_cm3.s
VECTOR_SOURCE := $(PLATFORM_DIR)/Vector_Table.s
SYSTEM_SOURCE := $(PLATFORM_DIR)/system.c
LINKER_SCRIPT := $(PLATFORM_DIR)/linker.ld
else
$(error Unsupported TARGET=$(TARGET); use TARGET=s32k312 or TARGET=qemu-mps2-an385)
endif
CFLAGS := $(CPUFLAGS) $(ARCHFLAGS) $(OPTFLAGS) -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra -I. -Iarch -I$(PLATFORM_DIR)
TEST ?= simple
# Keep objects separate when changing test-specific kernel definitions.
OBJDIR := $(OBJDIR)/$(TEST)
include tests/tests.mk
ASFLAGS := $(CPUFLAGS) $(ARCHFLAGS) $(OPTFLAGS) -x assembler-with-cpp
LDFLAGS := $(CPUFLAGS) -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BINDIR)/$(PROJECT).map -T $(LINKER_SCRIPT)
OBJS := $(addprefix $(OBJDIR)/,startup.o Vector_Table.o system.o kernel/task.o kernel/benchmark.o arch/cortex_m/port_cm7.o arch/cortex_m/svc_stubs_cm7.o arch/cortex_m/svc_cm7.o arch/cortex_m/fault.o kernel/fatal.o kernel/sync.o kernel/timer.o kernel/mempool.o board/board.o) $(APP_OBJS)

$(OBJS): JRTConfig.h arch/cortex_m/port_contract.h Makefile tests/tests.mk

all: $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex

$(BINDIR)/$(PROJECT).elf: $(OBJS) $(LINKER_SCRIPT) FORCE | $(BINDIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BINDIR)/$(PROJECT).bin: $(BINDIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

$(BINDIR)/$(PROJECT).hex: $(BINDIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@

$(OBJDIR)/startup.o: $(STARTUP_SOURCE) | $(OBJDIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJDIR)/Vector_Table.o: $(VECTOR_SOURCE) | $(OBJDIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJDIR)/system.o: $(SYSTEM_SOURCE) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Ikernel -Itests -MMD -MP -c $< -o $@

$(OBJDIR)/%.o: %.s
	mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

$(OBJDIR)/board/board.o: $(PLATFORM_DIR)/board/board.c $(PLATFORM_DIR)/board/board.h | $(OBJDIR)/board
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR) $(OBJDIR)/kernel $(OBJDIR)/arch/cortex_m $(OBJDIR)/examples $(OBJDIR)/tests $(OBJDIR)/board $(BINDIR):
	mkdir -p $@

clean:
	rm -f $(OBJS) $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex $(BINDIR)/$(PROJECT).map

auto-test:
	$(PYTHON) tools/run_tests.py --quiet-build --build $(BUILD) --timeout 20

qemu-test:
	$(PYTHON) tools/run_qemu_tests.py --quiet-build --build $(BUILD) --timeout 45

config-test:
	$(PYTHON) tools/test_config_builds.py

# Output paths remain stable for debugger scripts, so relink after profile switches.
FORCE:

.PHONY: all clean auto-test qemu-test config-test FORCE
