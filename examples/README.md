# JustBoot Examples

This directory contains application-level tasks that exercise the kernel API
(`kernel/kernel.h`, `kernel/sync.h`, `kernel/timer.h`, `kernel/mempool.h`).
None of this code is part of the portable kernel or the Cortex-M/S32K312
port; it only calls the public API.

## Selecting an example

`main.c` selects one example at compile time via `JUSTBOOT_MAIN_PROFILE`
(set with `make -B MAIN_PROFILE=<n>`). The wired profiles are:

| Profile | Function | File | Description |
| --- | --- | --- | --- |
| 0 (default) | `heartbeat_example_start()` | `heartbeat.c` | Baseline heartbeat + activity task bring-up. |
| 1 | `isr_sync_paths_start()` | `isr_sync_paths.c` | ISR-driven semaphore/queue give and take. |
| 2 | `isr_sync_queue_full_start()` | `isr_sync_paths.c` | Same as above under sustained queue-full/drop pressure. |
| 3 | `isr_sync_soak_start()` | `isr_sync_paths.c` | Long-run soak: ISR sync plus mutex priority inheritance and LED heartbeat. |
| 6 | `heartbeat_unprivileged_led_start()` | `heartbeat.c` | Unprivileged task toggling the LED through the SVC gateway. |
| 7 | `heartbeat_periodic_delay_start()` | `heartbeat.c` | `task_delay_until()` periodic timing regression. |
| 8 | `heartbeat_timer_start()` | `heartbeat.c` | Software timer expiry (`kernel_timer_*`). |
| 9 | `heartbeat_timer_callback_start()` | `heartbeat.c` | Deferred timer callback dispatch. |
| 10 | `heartbeat_notification_start()` | `heartbeat.c` | Task notification producer/consumer. |
| 11 | `heartbeat_event_group_start()` | `heartbeat.c` | Basic event-group wait-all/clear-on-exit. |
| 12 | `heartbeat_mempool_start()` | `heartbeat.c` | Deterministic memory pool alloc/free. |
| 13 | `event_group_regression_start()` | `event_group_regression.c` | Full event-group regression: wait-any, wait-all, timeout, and ISR-set paths. |

The remaining files below are standalone examples that predate the profile
system and are not wired into `main.c`. To run one, call its `*_start()`
function from `main()` in place of the default and rebuild.

## `heartbeat.c`

Holds every profile-0/6/7/8/9/10/11/12 task definition and start function
listed in the table above. Counters: `g_periodic_delay_runs`,
`g_periodic_delay_last_tick`, `g_timer_expirations`, `g_timer_last_tick`,
`g_timer_callback_runs`, `g_timer_callback_last_tick`, `g_notification_sent`,
`g_notification_received`, `g_notification_error`, `g_event_group_waits`,
`g_event_group_error`, `g_mempool_allocated`, `g_mempool_reused`,
`g_mempool_error`.

## `isr_sync_paths.c`

Validates `semaphore_give_from_isr()` and `queue_send_from_isr()` using a
real SysTick tick-hook (`kernel_set_tick_hook()`). The interrupt periodically
gives a semaphore and enqueues increasing values; a consumer task blocks on
these objects and verifies monotonic queue data. `isr_sync_soak_start()`
additionally runs mutex priority-inheritance contention and an LED heartbeat
for extended soak testing.

Counters: `g_isr_sync_irq_give_count`, `g_isr_sync_irq_queue_sent`,
`g_isr_sync_irq_queue_dropped`, `g_isr_sync_sem_taken`,
`g_isr_sync_queue_received`, `g_isr_sync_error`, `g_isr_sync_done`
(sync mode); `g_isr_qfull_*` (queue-full mode); `g_isr_soak_*` (soak mode).
Expected pass behavior: the `*_done` flag becomes `1`, the corresponding
`*_error` stays `0`, drop counts stay bounded/expected, and sent counts
match received counts.

## `event_group_regression.c`

Exercises `event_group_wait_bits()`/`event_group_set_bits()` end to end:
wait-any and wait-all semantics with disjoint owned bits, clear-on-exit
correctness after both wait modes, finite timeout expiry on a bit that is
never set, and `event_group_set_bits_from_isr()` driven from the tick hook
with a waiter that verifies clear-on-exit after the ISR wake.

Counters: `g_event_regression_wait_any`, `g_event_regression_wait_all`,
`g_event_regression_timeouts`, `g_event_regression_isr_sets`,
`g_event_regression_isr_wakes`, `g_event_regression_clear_checks`,
`g_event_regression_error` (must stay `0`), `g_event_regression_done`
(latches to `1` once enough rounds complete cleanly).

## `sync_producer_consumer.c`

Selectable producer/consumer application. The producer sends incrementing
values into a bounded queue and gives a semaphore after each successful
send. The consumer takes the semaphore, receives from the queue, and records
FIFO mismatches in `g_sync_error`. `g_sync_producer_value` and
`g_sync_consumer_value` expose producer and consumer progress.

Call `sync_producer_consumer_start()` from `main()` to run it.

## `semaphore_event.c`

Demonstrates semaphore-only event notification. The event source gives a
binary semaphore every 100 ms. The worker blocks on `semaphore_take()` and
increments its received counter when the event arrives. Counters:
`g_semaphore_events_sent`, `g_semaphore_events_received`,
`g_semaphore_event_error`.

Call `semaphore_event_start()` from `main()` to run it.

## `mutex_contention.c`

Demonstrates mutex ownership and contention. The owner and contender update
a shared counter only while holding the mutex. Counters:
`g_mutex_owner_operations`, `g_mutex_contender_operations`, `g_mutex_error`,
`g_mutex_contender_state`, `g_mutex_contender_state_after_unlock`,
`g_mutex_contender_stack_used`. The two state values show the contender
blocked while the mutex is held and ready immediately after the owner wakes
it. A nonzero error indicates failed ownership, timeout, or inspection
behavior.

Call `mutex_contention_start()` from `main()` to run it.

## `mutex_priority_inheritance.c`

Starts a low-priority mutex owner, a medium-priority CPU task, and a
high-priority waiter. When the waiter blocks, the owner inherits the
waiter's effective priority and runs ahead of the medium task until it
unlocks the mutex. Counters: `g_inheritance_low_priority`,
`g_inheritance_high_state`, `g_inheritance_low_operations`,
`g_inheritance_high_operations`, `g_inheritance_medium_operations`,
`g_inheritance_error`.

Call `mutex_priority_inheritance_start()` from `main()` to run it.

## `mutex_edge_cases.c`

Verifies recursive lock/unlock behavior and rejects unlock attempts by a
non-owner. Counters: `g_mutex_recursive_first_lock`,
`g_mutex_recursive_second_lock`, `g_mutex_recursive_first_unlock`,
`g_mutex_recursive_second_unlock`, `g_mutex_non_owner_unlock`,
`g_mutex_edge_error`. All lock/unlock results should be `1` except
`g_mutex_non_owner_unlock`, which should be `0`; the error value should
remain `0`.

Call `mutex_edge_cases_start()` from `main()` to run it.

## `waiter_priority_wake.c`

Validates wake ordering when two tasks block on the same semaphore. The
first give must wake the higher-priority waiter and the second give must
wake the lower-priority waiter. Counters: `g_waiter_wake_order[0]`,
`g_waiter_wake_order[1]`, `g_waiter_wake_count`, `g_waiter_wake_error`,
`g_waiter_wake_done`. Expected pass: `g_waiter_wake_done == 1`,
`g_waiter_wake_error == 0`, `g_waiter_wake_order[0] == 0xA1`,
`g_waiter_wake_order[1] == 0xB2`.

Call `waiter_priority_wake_start()` from `main()` to run it.

## `waiter_timeout_wake.c`

Validates timeout interaction with wake selection. A high-priority task
blocks with a finite timeout and must time out, then gives the semaphore
once while two lower-priority waiters remain blocked; the wake must select
the highest-priority remaining waiter first. Counters:
`g_waiter_timeout_flag`, `g_waiter_timeout_wake_order[0]`,
`g_waiter_timeout_wake_count`, `g_waiter_timeout_error`,
`g_waiter_timeout_done`. Expected pass: `g_waiter_timeout_flag == 1`,
`g_waiter_timeout_done == 1`, `g_waiter_timeout_error == 0`,
`g_waiter_timeout_wake_order[0] == 0xC3`.

Call `waiter_timeout_wake_start()` from `main()` to run it.

## `mutex_multi_restore.c`

Validates that priority-inheritance restore is recalculated across all
currently owned mutexes. The owner task takes two mutexes, a high-priority
waiter blocks on one mutex, and a medium-priority waiter blocks on the
other. After the first unlock, the owner priority must drop from high to
medium; after the second unlock, it must drop to base. Counters:
`g_multi_restore_owner_priority_before_release`,
`g_multi_restore_owner_priority_after_first_release`,
`g_multi_restore_owner_priority_after_second_release`,
`g_multi_restore_high_waiter_acquired`, `g_multi_restore_mid_waiter_acquired`,
`g_multi_restore_error`, `g_multi_restore_done`. Expected pass: `3`, `2`, `1`
for the three priority snapshots, both acquired flags `1`,
`g_multi_restore_error == 0`, `g_multi_restore_done == 1`.

Call `mutex_multi_restore_start()` from `main()` to run it.

## `mutex_chain_inheritance.c`

Validates transitive inheritance through a wait chain. A low-priority owner
holds `mutex_1`, a medium-priority bridge holds `mutex_2` and blocks on
`mutex_1`, and a high-priority task blocks on `mutex_2`. The owner must
inherit the high priority through the bridge task. Counters:
`g_chain_owner_priority_after_chain`, `g_chain_bridge_blocked`,
`g_chain_high_blocked`, `g_chain_bridge_acquired_mutex_1`,
`g_chain_high_acquired_mutex_2`, `g_chain_error`, `g_chain_done`. Expected
pass: owner priority `3`, all block/acquire flags `1`, `g_chain_error == 0`,
`g_chain_done == 1`.

Call `mutex_chain_inheritance_start()` from `main()` to run it.

## `mutex_timeout_restore.c`

Validates that a waiter timeout propagates priority recalculation up the
ownership chain. A low-priority owner holds `mutex_1`; a medium-priority
bridge holds `mutex_2` and blocks on `mutex_1` with no timeout; a
high-priority task blocks on `mutex_2` with a finite timeout. While the full
chain exists the owner must be boosted to high priority; after the
high-priority waiter times out, the owner must drop back to medium (the
bridge still blocks on `mutex_1`) — not all the way to base. Counters:
`g_timeout_restore_owner_priority_full_chain`,
`g_timeout_restore_owner_priority_after_timeout`,
`g_timeout_restore_bridge_still_blocked`, `g_timeout_restore_error`,
`g_timeout_restore_done`. Expected pass: `3`, `2`, bridge flag `1`, error
`0`, done `1`.

Call `mutex_timeout_restore_start()` from `main()` to run it.
