// SPDX-License-Identifier: BSD-2-Clause-Patent
// Platform sleep (suspend to idle) for boards without Apple's sleep hardware, the logic ArmCpu.cpp applies while the system sleeps:
// the boot core waits with interrupts masked for one of the board's wake sources, the other cores are off (PSCI CPU_OFF). While it
// waits only the wake sources stay enabled at the distributor; a pending interrupt is a wake when the board says so (should_wake) or
// when it is the wake-after timer. Interrupt numbers are GIC INTIDs (SPI n is INTID 32 + n). Host-tested by tests/armcpu_sleep_host.c.
#ifndef ARMCPU_SLEEP_H
#define ARMCPU_SLEEP_H
#include <stdint.h>
#include <string.h>

// GICD_ISENABLER<n> words: word 0 (SGIs and PPIs, banked per core) is left alone, words 1..31 cover INTIDs 32..1023.
#define ARMCPU_SLEEP_SPI_WORDS 32u
#define ARMCPU_SLEEP_NO_TIMER 0xffffffffu

// What a board gives ArmCpu (armcpu_sleep_register): prepare and restore run on the boot core with interrupts masked, before and after
// the wait; should_wake is asked about a pending wake source and may clear that source's own status when it answers 0.
struct armcpu_sleep_board {
    void (*prepare)(void);
    int (*should_wake)(uint32_t intid);
    void (*restore)(void);
    const uint32_t *wake_spis;
    uint32_t wake_spi_count;
};

enum armcpu_wake_reason {ArmcpuWakeNone, ArmcpuWakeSource, ArmcpuWakeTimer};

// phase: 1 in the hook, 2 the other cores off, 3 waiting, 4 woken, 5 restored (the last one reached shows where a hang stopped).
struct armcpu_sleep_stats {
    uint32_t phase, entries, spurious, last_intid, last_reason, cpu_off_failures;
    uint64_t asleep_ticks;
};

// The distributor enables to keep while asleep: those of the wake sources that were enabled before.
static inline void armcpu_sleep_keep(const uint32_t saved[ARMCPU_SLEEP_SPI_WORDS], const uint32_t *wake_spis, uint32_t count,
                                     uint32_t keep[ARMCPU_SLEEP_SPI_WORDS])
{
    memset(keep, 0, ARMCPU_SLEEP_SPI_WORDS * sizeof *keep);
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t intid = wake_spis[i];
        if (intid < 32 || intid > 1019) continue;
        const uint32_t bit = 1u << (intid % 32);
        keep[intid / 32] |= saved[intid / 32] & bit;
    }
}

// What a pending interrupt means for the wait: the board's yes first, then the wake-after timer, else keep waiting.
static inline enum armcpu_wake_reason armcpu_sleep_decide(uint32_t intid, int board_says_wake, uint32_t timer_intid)
{
    if (board_says_wake) return ArmcpuWakeSource;
    if (timer_intid != ARMCPU_SLEEP_NO_TIMER && intid == timer_intid) return ArmcpuWakeTimer;
    return ArmcpuWakeNone;
}

#endif
