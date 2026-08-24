PROJECT := justboot
OBJDIR := obj
BINDIR := bin

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
CPUFLAGS := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
DEBUGFLAGS := -Og -g3
CFLAGS := $(CPUFLAGS) $(DEBUGFLAGS) -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra
MAIN_PROFILE ?= 0
CFLAGS += -DJUSTBOOT_MAIN_PROFILE=$(MAIN_PROFILE)
ASFLAGS := $(CPUFLAGS) $(DEBUGFLAGS) -x assembler-with-cpp
LDFLAGS := $(CPUFLAGS) -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BINDIR)/$(PROJECT).map -T linker_flash_s32k312.ld
OBJS := $(addprefix $(OBJDIR)/,startup_cm7.o Vector_Table.o system.o main.o examples/heartbeat.o examples/sync_producer_consumer.o examples/semaphore_event.o examples/mutex_contention.o examples/mutex_priority_inheritance.o examples/mutex_edge_cases.o examples/waiter_priority_wake.o examples/waiter_timeout_wake.o examples/mutex_multi_restore.o examples/mutex_chain_inheritance.o examples/mutex_timeout_restore.o examples/isr_sync_paths.o examples/event_group_regression.o kernel/task.o kernel/port_cm7.o kernel/svc_cm7.o kernel/fault.o kernel/sync.o kernel/timer.o kernel/mempool.o board/board.o)

all: $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex

$(BINDIR)/$(PROJECT).elf: $(OBJS) linker_flash_s32k312.ld | $(BINDIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BINDIR)/$(PROJECT).bin: $(BINDIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

$(BINDIR)/$(PROJECT).hex: $(BINDIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@

$(OBJDIR)/startup_cm7.o: startup_cm7.s | $(OBJDIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJDIR)/Vector_Table.o: Vector_Table.s | $(OBJDIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(OBJDIR)/system.o: system.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/main.o: main.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/examples/heartbeat.o: examples/heartbeat.c examples/heartbeat.h kernel/kernel.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/sync_producer_consumer.o: examples/sync_producer_consumer.c examples/sync_producer_consumer.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/semaphore_event.o: examples/semaphore_event.c examples/semaphore_event.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/mutex_contention.o: examples/mutex_contention.c examples/mutex_contention.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/mutex_priority_inheritance.o: examples/mutex_priority_inheritance.c examples/mutex_priority_inheritance.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/mutex_edge_cases.o: examples/mutex_edge_cases.c examples/mutex_edge_cases.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/waiter_priority_wake.o: examples/waiter_priority_wake.c examples/waiter_priority_wake.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/waiter_timeout_wake.o: examples/waiter_timeout_wake.c examples/waiter_timeout_wake.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/mutex_multi_restore.o: examples/mutex_multi_restore.c examples/mutex_multi_restore.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/mutex_chain_inheritance.o: examples/mutex_chain_inheritance.c examples/mutex_chain_inheritance.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/mutex_timeout_restore.o: examples/mutex_timeout_restore.c examples/mutex_timeout_restore.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/isr_sync_paths.o: examples/isr_sync_paths.c examples/isr_sync_paths.h kernel/kernel.h kernel/sync.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/examples/event_group_regression.o: examples/event_group_regression.c examples/event_group_regression.h kernel/kernel.h | $(OBJDIR)/examples
	$(CC) $(CFLAGS) -Ikernel -Iexamples -c $< -o $@

$(OBJDIR)/kernel/task.o: kernel/task.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/port_cm7.o: kernel/port_cm7.c kernel/kernel.h | $(OBJDIR)/kernel
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

$(OBJDIR)/board/board.o: board/board.c board/board.h | $(OBJDIR)/board
	$(CC) $(CFLAGS) -Iboard -c $< -o $@

$(OBJDIR) $(OBJDIR)/kernel $(OBJDIR)/examples $(OBJDIR)/board $(BINDIR):
	mkdir -p $@

clean:
	rm -f $(OBJS) $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex $(BINDIR)/$(PROJECT).map

.PHONY: all clean
