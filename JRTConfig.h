/*
 * Application configuration for justRT.
 *
 * This bundled file supplies defaults for the repository examples and tests;
 * it is not a universal configuration for applications using the kernel.
 * Each application should own its JRTConfig.h and select its clock, tick rate,
 * task capacity, priorities, stack sizes, and optional diagnostics explicitly.
 *
 * Integration: copy this file into the application's configuration directory
 * and put that directory before the justRT root in the compiler include path
 * for ALL kernel, architecture, and application C sources. Use the same
 * configuration consistently throughout the image. The repository Makefile
 * uses this bundled file; an application's build must set its include order.
 *
 * The #ifndef defaults also allow individual compiler -D overrides. Stack
 * defaults are example values: size application stacks from measured usage
 * plus margin. Architecture capabilities remain the port's responsibility.
 * See README.md (Application configuration) for integration details.
 */

#ifndef JUSTRT_CONFIG_H
#define JUSTRT_CONFIG_H

/* Target clock and kernel tick frequency. */
#ifndef JRT_CORE_CLOCK_HZ
#define JRT_CORE_CLOCK_HZ 120000000UL
#endif

#ifndef JRT_TICK_RATE_HZ
#define JRT_TICK_RATE_HZ 7500UL
#endif

/* Application-visible task capacity and default static stack sizes. */
#ifndef JRT_MAX_APPLICATION_TASKS
#define JRT_MAX_APPLICATION_TASKS 7U
#endif

#ifndef JRT_DEFAULT_TASK_STACK_WORDS
#define JRT_DEFAULT_TASK_STACK_WORDS 128U
#endif

#ifndef JRT_IDLE_STACK_WORDS
#define JRT_IDLE_STACK_WORDS JRT_DEFAULT_TASK_STACK_WORDS
#endif

#ifndef JRT_TIMER_SERVICE_STACK_WORDS
#define JRT_TIMER_SERVICE_STACK_WORDS JRT_DEFAULT_TASK_STACK_WORDS
#endif

#ifndef JRT_MAX_TASK_PRIORITY
#define JRT_MAX_TASK_PRIORITY 31U
#endif

#ifndef JRT_TIMER_SERVICE_PRIORITY
#define JRT_TIMER_SERVICE_PRIORITY 1U
#endif

#ifndef JRT_ENABLE_TEST_HOOKS
#define JRT_ENABLE_TEST_HOOKS 0U
#endif

#ifndef JRT_ENABLE_TASK_BENCHMARK
#define JRT_ENABLE_TASK_BENCHMARK 0U
#endif

#endif
