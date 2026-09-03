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
CFLAGS := $(CPUFLAGS) $(ARCHFLAGS) $(OPTFLAGS) -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra -I. -Iarch -I$(PLATFORM_DIR)
TEST ?= simple
ifeq ($(TEST),simple)
else ifeq ($(TEST),boot)
CFLAGS += -DJUSTRT_TEST_BOOT=1
else ifeq ($(TEST),config_runtime)
CFLAGS += -DJUSTRT_TEST_CONFIG_RUNTIME=1 -DJRT_TICK_RATE_HZ=1024UL
else ifeq ($(TEST),fatal_hook)
CFLAGS += -DJUSTRT_TEST_FATAL_HOOK=1
else ifeq ($(TEST),fatal_hook_return)
CFLAGS += -DJUSTRT_TEST_FATAL_HOOK_RETURN=1
else ifeq ($(TEST),sync)
CFLAGS += -DJUSTRT_TEST_SYNC=1 -DJRT_ENABLE_TEST_HOOKS=1
else ifeq ($(TEST),mutex)
CFLAGS += -DJUSTRT_TEST_MUTEX=1
else ifeq ($(TEST),fpu)
ifneq ($(TARGET),s32k312)
$(error TEST=fpu requires TARGET=s32k312)
endif
CFLAGS += -DJUSTRT_TEST_FPU=1
else ifeq ($(TEST),race)
CFLAGS += -DJUSTRT_TEST_RACE=1 -DJRT_ENABLE_TEST_HOOKS=1
else ifeq ($(TEST),stress)
CFLAGS += -DJUSTRT_TEST_RACE=1 -DJUSTRT_TEST_STRESS=1 -DJRT_ENABLE_TEST_HOOKS=1
else ifeq ($(TEST),timer_service)
CFLAGS += -DJUSTRT_TEST_TIMER_SERVICE=1
else ifeq ($(TEST),task_capacity)
CFLAGS += -DJUSTRT_TEST_TASK_CAPACITY=1
else ifeq ($(TEST),stack_guard)
ifeq ($(TARGET),qemu-mps2-an385)
$(error TEST=stack_guard requires an MPU-enabled target)
endif
CFLAGS += -DJUSTRT_TEST_STACK_GUARD=1
else ifeq ($(TEST),private_config)
CFLAGS += -DJUSTRT_TEST_PRIVATE_CONFIG=1
else ifeq ($(TEST),mpu_isolation_read)
ifneq ($(TARGET),s32k312)
$(error TEST=mpu_isolation_read requires TARGET=s32k312)
endif
CFLAGS += -DJUSTRT_TEST_MPU_ISOLATION_READ=1
else ifeq ($(TEST),mpu_isolation_write)
ifneq ($(TARGET),s32k312)
$(error TEST=mpu_isolation_write requires TARGET=s32k312)
endif
CFLAGS += -DJUSTRT_TEST_MPU_ISOLATION_WRITE=1
else ifeq ($(TEST),task_suspension)
CFLAGS += -DJUSTRT_TEST_TASK_SUSPENSION=1 -DJRT_ENABLE_TEST_HOOKS=1
else ifeq ($(TEST),task_suspension_mpu)
ifneq ($(TARGET),s32k312)
$(error TEST=task_suspension_mpu requires TARGET=s32k312)
endif
CFLAGS += -DJUSTRT_TEST_TASK_SUSPENSION_MPU=1
else
$(error Unsupported TEST=$(TEST); use TEST=simple, TEST=boot, TEST=config_runtime, TEST=fatal_hook, TEST=fatal_hook_return, TEST=sync, TEST=mutex, TEST=fpu, TEST=race, TEST=stress, TEST=timer_service, TEST=task_capacity, TEST=stack_guard, TEST=private_config, TEST=mpu_isolation_read, TEST=mpu_isolation_write, TEST=task_suspension, or TEST=task_suspension_mpu)
endif
ASFLAGS := $(CPUFLAGS) $(ARCHFLAGS) $(OPTFLAGS) -x assembler-with-cpp
LDFLAGS := $(CPUFLAGS) -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BINDIR)/$(PROJECT).map -T $(LINKER_SCRIPT)
OBJS := $(addprefix $(OBJDIR)/,startup.o Vector_Table.o system.o main.o tests/test_boot_and_privilege.o tests/test_config_runtime.o tests/test_fatal_hook.o tests/test_synchronization.o tests/test_mutex.o tests/test_race.o tests/test_timer_service.o tests/test_task_capacity.o tests/test_stack_guard.o tests/test_private_config.o tests/test_mpu_isolation.o tests/test_task_suspension.o examples/simple.o kernel/task.o kernel/benchmark.o kernel/port_cm7.o kernel/svc_stubs_cm7.o kernel/svc_cm7.o kernel/fault.o kernel/fatal.o kernel/sync.o kernel/timer.o kernel/mempool.o board/board.o) $(FPU_OBJS)

$(OBJS): JRTConfig.h

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

$(OBJDIR)/tests/test_config_runtime.o: tests/test_config_runtime.c tests/test_config_runtime.h tests/test_common.h kernel/kernel.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/tests/test_fatal_hook.o: tests/test_fatal_hook.c tests/test_fatal_hook.h tests/test_common.h kernel/kernel.h | $(OBJDIR)/tests
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

$(OBJDIR)/tests/test_timer_service.o: tests/test_timer_service.c tests/test_timer_service.h tests/test_common.h kernel/kernel.h kernel/timer.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/tests/test_task_capacity.o: tests/test_task_capacity.c tests/test_task_capacity.h tests/test_common.h kernel/kernel.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/tests/test_stack_guard.o: tests/test_stack_guard.c tests/test_stack_guard.h tests/test_common.h kernel/kernel.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/tests/test_private_config.o: tests/test_private_config.c tests/test_private_config.h tests/test_common.h kernel/kernel.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/tests/test_mpu_isolation.o: tests/test_mpu_isolation.c tests/test_mpu_isolation.h tests/test_common.h kernel/kernel.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/tests/test_task_suspension.o: tests/test_task_suspension.c tests/test_task_suspension.h tests/test_common.h kernel/kernel.h | $(OBJDIR)/tests
	$(CC) $(CFLAGS) -Ikernel -Itests -c $< -o $@

$(OBJDIR)/examples/simple.o: examples/simple.c examples/simple.h kernel/kernel.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/kernel/task.o: kernel/task.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/benchmark.o: kernel/benchmark.c kernel/benchmark.h kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/port_cm7.o: kernel/port_cm7.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/svc_stubs_cm7.o: kernel/svc_stubs_cm7.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/svc_cm7.o: kernel/svc_cm7.s | $(OBJDIR)/kernel
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJDIR)/kernel/fault.o: kernel/fault.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/fatal.o: kernel/fatal.c kernel/kernel.h | $(OBJDIR)/kernel
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
	$(PYTHON) tools/run_tests.py --quiet-build --timeout 20

qemu-test:
	$(PYTHON) tools/run_qemu_tests.py --quiet-build

qemu-release-test:
	$(PYTHON) tools/run_qemu_tests.py --quiet-build --build release --timeout 45 --test sync --test stress --test timer_service --test task_suspension

auto-release-test:
	$(PYTHON) tools/run_tests.py --quiet-build --build release --timeout 20 --test sync --test stress --test timer_service --test task_suspension --test stack_guard --test task_suspension_mpu

config-test:
	$(PYTHON) tools/test_config_builds.py

.PHONY: all clean auto-test qemu-test qemu-release-test auto-release-test config-test
