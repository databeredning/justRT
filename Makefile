PROJECT := justboot
OBJDIR := obj
BINDIR := bin

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
CPUFLAGS := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard
DEBUGFLAGS := -Og -g3
CFLAGS := $(CPUFLAGS) $(DEBUGFLAGS) -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra
ASFLAGS := $(CPUFLAGS) $(DEBUGFLAGS) -x assembler-with-cpp
LDFLAGS := $(CPUFLAGS) -nostdlib -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BINDIR)/$(PROJECT).map -T linker_flash_s32k312.ld
OBJS := $(addprefix $(OBJDIR)/,startup_cm7.o Vector_Table.o system.o main.o kernel/task.o kernel/port_cm7.o kernel/fault.o)

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

$(OBJDIR)/kernel/task.o: kernel/task.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/port_cm7.o: kernel/port_cm7.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR)/kernel/fault.o: kernel/fault.c kernel/kernel.h | $(OBJDIR)/kernel
	$(CC) $(CFLAGS) -Ikernel -c $< -o $@

$(OBJDIR) $(OBJDIR)/kernel $(BINDIR):
	mkdir -p $@

clean:
	rm -f $(OBJS) $(BINDIR)/$(PROJECT).elf $(BINDIR)/$(PROJECT).bin $(BINDIR)/$(PROJECT).hex $(BINDIR)/$(PROJECT).map

.PHONY: all clean
