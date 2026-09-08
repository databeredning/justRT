# Test profile selection; the default build is the simple example.
ifeq ($(TEST),simple)
APP_OBJS := $(OBJDIR)/examples/main.o $(OBJDIR)/examples/simple.o
else ifeq ($(TEST),boot)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_boot_and_privilege.o
CFLAGS += -DJUSTRT_TEST_BOOT=1
else ifeq ($(TEST),benchmark)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_benchmark.o
ifneq ($(TARGET),s32k312)
$(error TEST=benchmark requires TARGET=s32k312)
endif
CFLAGS += -DJUSTRT_TEST_BENCHMARK=1 -DJRT_ENABLE_TASK_BENCHMARK=1
else ifeq ($(TEST),config_runtime)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_config_runtime.o
CFLAGS += -DJUSTRT_TEST_CONFIG_RUNTIME=1 -DJRT_TICK_RATE_HZ=1024UL
else ifeq ($(TEST),fatal_hook)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_fatal_hook.o
CFLAGS += -DJUSTRT_TEST_FATAL_HOOK=1
else ifeq ($(TEST),fatal_hook_return)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_fatal_hook.o
CFLAGS += -DJUSTRT_TEST_FATAL_HOOK_RETURN=1
else ifeq ($(TEST),sync)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_synchronization.o
CFLAGS += -DJUSTRT_TEST_SYNC=1 -DJRT_ENABLE_TEST_HOOKS=1
else ifeq ($(TEST),mutex)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_mutex.o
CFLAGS += -DJUSTRT_TEST_MUTEX=1
else ifeq ($(TEST),fpu)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_fpu.o
ifneq ($(TARGET),s32k312)
$(error TEST=fpu requires TARGET=s32k312)
endif
APP_OBJS += $(OBJDIR)/tests/test_fpu_registers.o
CFLAGS += -DJUSTRT_TEST_FPU=1
else ifeq ($(TEST),race)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_race.o
CFLAGS += -DJUSTRT_TEST_RACE=1 -DJRT_ENABLE_TEST_HOOKS=1
else ifeq ($(TEST),stress)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_race.o
CFLAGS += -DJUSTRT_TEST_RACE=1 -DJUSTRT_TEST_STRESS=1 -DJRT_ENABLE_TEST_HOOKS=1
else ifeq ($(TEST),timer_service)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_timer_service.o
CFLAGS += -DJUSTRT_TEST_TIMER_SERVICE=1
else ifeq ($(TEST),task_capacity)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_task_capacity.o
CFLAGS += -DJUSTRT_TEST_TASK_CAPACITY=1
else ifeq ($(TEST),stack_guard)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_stack_guard.o
ifeq ($(TARGET),qemu-mps2-an385)
$(error TEST=stack_guard requires an MPU-enabled target)
endif
CFLAGS += -DJUSTRT_TEST_STACK_GUARD=1
else ifeq ($(TEST),private_config)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_private_config.o
CFLAGS += -DJUSTRT_TEST_PRIVATE_CONFIG=1
else ifeq ($(TEST),mpu_isolation_read)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_mpu_isolation.o
ifneq ($(TARGET),s32k312)
$(error TEST=mpu_isolation_read requires TARGET=s32k312)
endif
CFLAGS += -DJUSTRT_TEST_MPU_ISOLATION_READ=1
else ifeq ($(TEST),mpu_isolation_write)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_mpu_isolation.o
ifneq ($(TARGET),s32k312)
$(error TEST=mpu_isolation_write requires TARGET=s32k312)
endif
CFLAGS += -DJUSTRT_TEST_MPU_ISOLATION_WRITE=1
else ifeq ($(TEST),task_suspension)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_task_suspension.o
CFLAGS += -DJUSTRT_TEST_TASK_SUSPENSION=1 -DJRT_ENABLE_TEST_HOOKS=1
else ifeq ($(TEST),task_suspension_mpu)
APP_OBJS := $(OBJDIR)/tests/main.o $(OBJDIR)/tests/test_task_suspension.o
ifneq ($(TARGET),s32k312)
$(error TEST=task_suspension_mpu requires TARGET=s32k312)
endif
CFLAGS += -DJUSTRT_TEST_TASK_SUSPENSION_MPU=1
else
$(error Unsupported TEST=$(TEST); use TEST=simple, TEST=boot, TEST=benchmark, TEST=config_runtime, TEST=fatal_hook, TEST=fatal_hook_return, TEST=sync, TEST=mutex, TEST=fpu, TEST=race, TEST=stress, TEST=timer_service, TEST=task_capacity, TEST=stack_guard, TEST=private_config, TEST=mpu_isolation_read, TEST=mpu_isolation_write, TEST=task_suspension, or TEST=task_suspension_mpu)
endif
