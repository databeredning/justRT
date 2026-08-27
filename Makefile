PROJECT := justrt
OBJDIR := obj
BINDIR := bin

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
PYTHON ?= py
CPUFLAGS := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard
ARCHFLAGS := -DJRT_ARCH_FPU_CONTEXT=1
DEBUGFLAGS := -Og -g3
CFLAGS := $(CPUFLAGS) $(ARCHFLAGS) $(DEBUGFLAGS) -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra -Iarch -Iplatform/s32k312
TEST ?= simple
ifeq ($(TEST),simple)
else ifeq ($(TEST),boot)
CFLAGS += -DJUSTRT_TEST_BOOT=1
else ifeq ($(TEST),sync)
CFLAGS += -DJUSTRT_TEST_SYNC=1
else ifeq ($(TEST),mutex)
CFLAGS += -DJUSTRT_TEST_MUTEX=1
else ifeq ($(TEST),fpu)
CFLAGS += -DJUSTRT_TEST_FPU=1
else
$(error Unsupported TEST=$(TEST); use TEST=simple, TEST=boot, TEST=sync, TEST=mutex, or TEST=fpu)
endif
ASFLAGS := $(CPUFLAGS) $(ARCHFLAGS) $(DEBUGFLAGS) -x assembler-with-cpp
LDFLAGS := $(CPUFLAGS) -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BINDIR)/$(PROJECT).map -T platform/s32k312/linker_flash_s32k312.ld
OBJS := $(addprefix $(OBJDIR)/,startup_cm7.o Vector_Table.o system.o main.o tests/test_boot_and_privilege.o tests/test_synchronization.o tests/test_mutex.o tests/test_fpu.o tests/test_fpu_registers.o examples/simple.o kernel/task.o kernel/port_cm7.o kernel/svc_stubs_cm7.o kernel/svc_cm7.o kernel/fault.o kernel/sync.o kernel/timer.o kernel/mempool.o board/board.o)

all: $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex

$(BINDIR)/$(PROJECT).elf: $(OBJS) platform/s32k312/linker_flash_s32k312.ld | $(BINDIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BINDIR)/$(PROJECT).bin: $(BINDIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

$(BINDIR)/$(PROJECT).hex: $(BINDIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@

$(OBJDIR)/startup_cm7.o: platform/s32k312/startup_cm7.s | $(OBJDIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJDIR)/Vector_Table.o: platform/s32k312/Vector_Table.s | $(OBJDIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJDIR)/system.o: platform/s32k312/system.c | $(OBJDIR)
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

$(OBJDIR)/board/board.o: platform/s32k312/board/board.c platform/s32k312/board/board.h | $(OBJDIR)/board
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR) $(OBJDIR)/kernel $(OBJDIR)/examples $(OBJDIR)/tests $(OBJDIR)/board $(BINDIR):
	mkdir -p $@

clean:
	rm -f $(OBJS) $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex $(BINDIR)/$(PROJECT).map

auto-test:
	$(PYTHON) tools/run_tests.py --quiet-build

.PHONY: all clean auto-test
