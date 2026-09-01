# CPU Scheduler & Real-OS Executor

An implementation of classical CPU scheduling algorithms, benchmarked against real OS-level process execution on Linux comparing theoretical scheduling behavior to what the kernel actually does.

---

## What This Does

**Simulated Scheduling Engines** — Three classical scheduling policies run over a custom task queue:
- SJF (Shortest Job First, non-preemptive)
- Priority Scheduling (non-preemptive)
- Round Robin (preemptive, time quantum = 2)

**Real OS Executor** — Tasks are also run as actual OS processes, not just simulated:
- Spawned via `fork()`, synchronized with `waitpid()`
- Pinned to CPU Core 0 via `sched_setaffinity()` (Linux) to eliminate multi-core cache migration noise
- Escalated to real-time scheduling via `sched_setscheduler()` with `SCHED_FIFO` (Linux)
- Real context-switch counts (voluntary/non-voluntary) extracted by parsing `/proc/[pid]/status`, with a `getrusage()` fallback on macOS
- Cross-platform via `#ifdef __linux__` — compiles cleanly on macOS, enables kernel-level features on Linux

Both paths write to a common CSV schema, so simulated scheduler behavior can be directly compared against real OS execution.

---

## Why

Scheduling theory says SJF minimizes average wait time and Round Robin is fair but theory doesn't tell us how many *real* context switches Round Robin causes on an actual core or whether pinning a process to a core actually changes its behavior versus letting the Linux scheduler decide freely. This project was built to answer that with real measurements instead of assumptions.

---

## Architecture
task_queue.c → Task struct, singly linked list queue (O(1) enqueue/dequeue), synthetic task generation
scheduler_simulations.c → SJF / Priority / Round Robin simulations, event trace logging
performance_metrics.c → Turnaround time, waiting time, CPU utilization calculations + formatted reporting
os_executor.c → Real process execution: fork/waitpid, CPU affinity, SCHED_FIFO, context-switch tracking via /proc
scheduler_tool.c → Unified entry point — runs all simulated policies + the real OS executor, writes standardized output
│
▼
scheduler_visualization.csv
(timestamp, task_id, policy_name, event_type, cpu_time_used, context_switches_so_far)


---

## Build & Run

```bash
gcc -std=c11 -o scheduler task_queue.c scheduler_simulations.c performance.c os_execute.c scheduler_tool.c
./scheduler
```

Requires Linux for full functionality (`sched_setaffinity`, `SCHED_FIFO`, `/proc` parsing); falls back to POSIX equivalents on macOS.

---

---

## Notes

- Built to explore the gap between theoretical scheduling algorithms and real Linux kernel scheduling behavior like process management, CPU affinity, real-time priority classes, and context-switch overhead.
