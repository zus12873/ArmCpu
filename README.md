# ArmCpu

ARM generic interrupt controller and timer.

starts the CPU, GIC and timer on sheng and on the QEMU virt machine.

`armcpu_spi_attach` registers an exclusive source. `armcpu_spi_share` registers
a level source whose handlers acknowledge their own peripheral status.
Registration is serialized; handlers remain installed for the platform lifetime.
IRQ dispatch reads the interrupt's route mask and visits only its handlers.
`armcpu_spi_mask` masks or enables a registered line from primary IRQ context.

Cross-core signals are GIC SGI 0, or with `armcpu-spi-ipi=1` software-pended
SPIs 1008 to 1015 (one per logical core). A deferred signal goes out at once
and a cancel takes nothing back. Idle is the kernel's WFI; no PSCI
CPU_SUSPEND state is entered. `armcpu-edge=1` calls `ml_cpu_init_completed`
after the last core started, which lets the Edge scheduler build its cluster
matrix; `armcpu_cpu_init_completed` reports it.

`armcpu_stats_row(index, row, size)` formats wake and idle statistics for a
board log. Row 0 is always present: IPIs sent and taken, deferred and cancel
requests, the scheduler state and the firmware's PSCI answers. With `diag=1`
the driver registers the kernel's idle callback, times every IPI and asks the
firmware for PSCI_VERSION and PSCI_FEATURES once; one row per core then gives
its idle periods by length (under 100 us, under 1 ms, under 8 ms, longer) and
the time in each, what ended them (timer, IPI, device interrupt, no interrupt),
the cost of a send, and the delay from a send to the target's handler (under
50 us, under 200 us, under 1 ms, longer) for an awake and for an idle target.
The start-up IPI a core sends itself (`armcpu-self-ipi=1`) is counted as sent
and taken but is in neither the send cost nor the delays.
After the core rows comes one row per core with its timer interrupts by the
delay after their deadline (same ranges), again awake and out of idle. The
timer is one per core and one-shot; the delay out of idle is the time the
core takes to leave idle.

`platform-sleep=1` lets a board without Apple's sleep hardware sleep to idle.
With a PSCI conduit in `/psci`, the driver installs `arm64_platform_sleep_hook`
(defined by the board's XNU patch in `ml_arm_sleep`). `haltCPU` records the
core as stopped. `quiesceCPU`, which `cpu_sleep` calls on each core, enters
`ml_arm_sleep` while the system quiesces. A core other than the boot core then
turns itself off with PSCI CPU_OFF and comes back through CPU_ON and the reset
vector, set up by `initCPU(false)` as at its first start.

The boot core waits with interrupts masked. Only the board's wake sources stay
enabled at the distributor (`armcpu_sleep_register`, `ArmCpuSleep.h`), and WFI
ends on a pending one. A source the board's `should_wake` accepts stays pending
for its driver. Every other interrupt is acknowledged and ended. A line that
keeps firing after the board turned it down is turned off for the rest of the
sleep; when that leaves nothing to wake the system, it wakes.

The virtual timer (an absolute `CNTV_CVAL`) carries two deadlines:
- the wake-after deadline of `platform-sleep-wake-after=<seconds>`;
- the board's `tick`, every `tick_s` seconds while asleep (the watchdog ping on
  sheng).

When the timer fires, a wake source pending behind it is asked about first.
With no wake source enabled and no wake-after deadline, the wait returns at
once. The board's `prepare` and `restore` run around the wait, and the enables
are put back. `initCPU(false)` turns the boot core's CPU interface and timer on
again. It also sends the core a self-IPI, which `processor_cpu_reinit` waits
for; a resuming secondary core sends one too.

`armcpu_sleep_stats` records:
- the phase reached;
- the sleeps;
- spurious wakes, masked lines and ticks;
- sleeps that returned at once;
- the waking INTID and reason;
- the time asleep.

`tools/check-sleep-host.py` tests the logic on the host.

`armcpu-idle-latency-us=N` registers every core with that time as its power
gate latency (default 0: none). The kernel then spins through a delay shorter
than N instead of blocking the thread on a timer, sets an idle core's timer N
ahead of the deadline, and keeps a core out of WFI for a deadline closer than
N. Row 0 shows the value.

Counter sampling orders `ISB` before reading `CNTVCT_EL0`. GIC control, SGI and EOI sequences retain their synchronization barriers. Multi-instruction inline assembly uses newline separators.

The sheng `scripts/check-bluetooth-shared.py` fixture checks the production
registration, dispatch, masking and concurrent registration paths. The source
also compiles with the selected iPadOS 27 arm64e kernel command.

Build and interface details: [documentation](https://unixnu.com/reference/components). Component sources and dependencies: [catalog](https://unixnu.com/reference/components).
