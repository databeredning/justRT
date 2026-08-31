PROJECT := justrt
TARGET ?= s32k312
ifeq ($(TARGET),s32k312)
OBJDIR := obj
BINDIR := bin
else
OBJDIR := obj/$(TARGET)
BINDIR := bin/$(TARGET)
endif

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
PYTHON ?= py
ifeq ($(TARGET),s32k312)
PLATFORM_DIR := platform/s32k312
CPUFLAGS := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
ARCHFLAGS := -DJRT_ARCH_FPU_CONTEXT=1 -DJRT_ARCH_HAS_MPU=1
STARTUP_SOURCE := $(PLATFORM_DIR)/startup_cm7.s
VECTOR_SOURCE := $(PLATFORM_DIR)/Vector_Table.s
SYSTEM_SOURCE := $(PLATFORM_DIR)/system.c
LINKER_SCRIPT := $(PLATFORM_DIR)/linker_flash_s32k312.ld
FPU_OBJS := $(OBJDIR)/tests/test_fpu.o $(OBJDIR)/tests/test_fpu_registers.o
else ifeq ($(TARGET),qemu-mps2-an385)
PLATFORM_DIR := platform/qemu_mps2_an385
CPUFLAGS := -mcpu=cortex-m3 -mthumb
ARCHFLAGS := -DJRT_ARCH_FPU_CONTEXT=0 -DJRT_ARCH_HAS_MPU=0 -DJRT_CORE_CLOCK_HZ=25000000UL
STARTUP_SOURCE := $(PLATFORM_DIR)/startup_cm3.s
VECTOR_SOURCE := $(PLATFORM_DIR)/Vector_Table.s
SYSTEM_SOURCE := $(PLATFORM_DIR)/system.c
LINKER_SCRIPT := $(PLATFORM_DIR)/linker.ld
FPU_OBJS :=
else
$(error Unsupported TARGET=$(TARGET); use TARGET=s32k312 or TARGET=qemu-mps2-an385)
endif
DEBUGFLAGS := -Og -g3
CFLAGS := $(CPUFLAGS) $(ARCHFLAGS) $(DEBUGFLAGS) -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra -Iarch -I$(PLATFORM_DIR)
TEST ?= simple
ifeq ($(TEST),simple)
else ifeq ($(TEST),boot)
CFLAGS += -DJUSTRT_TEST_BOOT=1
else ifeq ($(TEST),sync)
CFLAGS += -DJUSTRT_TEST_SYNC=1
else ifeq ($(TEST),mutex)
CFLAGS += -DJUSTRT_TEST_MUTEX=1
else ifeq ($(TEST),fpu)
ifneq ($(TARGET),s32k312)
$(error TEST=fpu requires TARGET=s32k312)
endif
CFLAGS += -DJUSTRT_TEST_FPU=1
else ifeq ($(TEST),race)
CFLAGS += -DJUSTRT_TEST_RACE=1
else
$(error Unsupported TEST=$(TEST); use TEST=simple, TEST=boot, TEST=sync, TEST=mutex, TEST=fpu, or TEST=race)
endif
ASFLAGS := $(CPUFLAGS) $(ARCHFLAGS) $(DEBUGFLAGS) -x assembler-with-cpp
LDFLAGS := $(CPUFLAGS) -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BINDIR)/$(PROJECT).map -T $(LINKER_SCRIPT)
OBJS := $(addprefix $(OBJDIR)/,startup.o Vector_Table.o system.o main.o tests/test_boot_and_privilege.o tests/test_synchronization.o tests/test_mutex.o tests/test_race.o examples/simple.o kernel/task.o kernel/port_cm7.o kernel/svc_stubs_cm7.o kernel/svc_cm7.o kernel/fault.o kernel/sync.o kernel/timer.o kernel/mempool.o board/board.o) $(FPU_OBJS)

all: $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex

$(BINDIR)/$(PROJECT).elf: $(OBJS) $(LINKER_SCRIPT) | $(BINDIR)
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

$(OBJDIR)/main.o: main.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/tests/test_boot_and_privilege.o: tests/test_boot_and_privilege.c tests/test_boot_and_privilege.h tests/test_common.h kernel/kernel.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/tests/test_synchronization.o: tests/test_synchronization.c tests/test_synchronization.h tests/test_common.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/tests/test_mutex.o: tests/test_mutex.c tests/test_mutex.h tests/test_common.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/tests/test_fpu.o: tests/test_fpu.c tests/test_fpu.h tests/test_common.h kernel/kernel.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/tests/test_fpu_registers.o: tests/test_fpu_registers.s | $(OBJDIR)/tests
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJDIR)/tests/test_race.o: tests/test_race.c tests/test_race.h tests/test_common.h kernel/kernel.h kernel/sync.h kernel/timer.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/examples/simple.o: examples/simple.c examples/simple.h kernel/kernel.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/kernel/task.o: kernel/task.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/port_cm7.o: kernel/port_cm7.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/svc_stubs_cm7.o: kernel/svc_stubs_cm7.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/svc_cm7.o: kernel/svc_cm7.s | $(OBJDIR)/kernel
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJDIR)/kernel/fault.o: kernel/fault.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/sync.o: kernel/sync.c kernel/sync.h kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/timer.o: kernel/timer.c kernel/timer.h kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/mempool.o: kernel/mempool.c kernel/mempool.h kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/board/board.o: $(PLATFORM_DIR)/board/board.c $(PLATFORM_DIR)/board/board.h | $(OBJDIR)/board
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR) $(OBJDIR)/kernel $(OBJDIR)/examples $(OBJDIR)/tests $(OBJDIR)/board $(BINDIR):
	mkdir -p $@

clean:
	rm -f $(OBJS) $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex $(BINDIR)/$(PROJECT).map

auto-test:
	$(PYTHON) tools/run_tests.py --quiet-build

qemu-test:
	$(PYTHON) tools/run_qemu_tests.py --quiet-build

.PHONY: all clean auto-test qemu-test
