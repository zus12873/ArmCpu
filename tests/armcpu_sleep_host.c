// SPDX-License-Identifier: BSD-2-Clause-Patent
// Host tests of the platform sleep logic (ArmCpuSleep.h): which distributor enables stay on while the boot core waits, and what
// a pending interrupt means for the wait. Interrupt numbers are GIC INTIDs (SPI n is INTID 32 + n).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ArmCpuSleep.h"

static unsigned checks;
#define CHECK(x) do {if (!(x)) {fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1);} ++checks;} while (0)

static void set(uint32_t words[ARMCPU_SLEEP_SPI_WORDS], uint32_t intid) {words[intid / 32] |= 1u << (intid % 32);}
static int isset(const uint32_t words[ARMCPU_SLEEP_SPI_WORDS], uint32_t intid) {return (words[intid / 32] >> (intid % 32)) & 1u;}
static unsigned count(const uint32_t words[ARMCPU_SLEEP_SPI_WORDS])
{
    unsigned n = 0;
    for (unsigned i = 0; i < ARMCPU_SLEEP_SPI_WORDS; ++i) n += (unsigned)__builtin_popcount(words[i]);
    return n;
}

static void keep_only_enabled_wake_spis(void)
{
    uint32_t saved[ARMCPU_SLEEP_SPI_WORDS] = {0}, keep[ARMCPU_SLEEP_SPI_WORDS];
    set(saved, 240); set(saved, 513); set(saved, 332);
    const uint32_t wake[] = {240, 513, 532};
    memset(keep, 0xa5, sizeof keep);
    armcpu_sleep_keep(saved, wake, 3, keep);
    CHECK(isset(keep, 240) && isset(keep, 513));
    CHECK(!isset(keep, 332) && !isset(keep, 532));     // enabled but not a wake source; a wake source never enabled
    CHECK(count(keep) == 2);
}

static void keep_ignores_out_of_range(void)
{
    uint32_t saved[ARMCPU_SLEEP_SPI_WORDS], keep[ARMCPU_SLEEP_SPI_WORDS];
    memset(saved, 0xff, sizeof saved);
    const uint32_t wake[] = {31, 1020, 4096, 240};
    armcpu_sleep_keep(saved, wake, 4, keep);
    CHECK(count(keep) == 1 && isset(keep, 240));        // PPI 31 and INTIDs past 1019 are not SPIs
    CHECK(keep[0] == 0);                                // word 0 (SGIs, PPIs) is never touched
}

static void decide_board_wake(void) {CHECK(armcpu_sleep_decide(240, 1, 27) == ArmcpuWakeSource);}
static void decide_timer(void) {CHECK(armcpu_sleep_decide(27, 0, 27) == ArmcpuWakeTimer);}
static void decide_none_1023(void)
{
    CHECK(armcpu_sleep_decide(1023, 0, 27) == ArmcpuWakeNone);
    CHECK(armcpu_sleep_decide(240, 0, 27) == ArmcpuWakeNone);   // a wake source the board turned down
    CHECK(armcpu_sleep_decide(27, 0, ARMCPU_SLEEP_NO_TIMER) == ArmcpuWakeNone);
}
static void timer_and_wake_source_prefers_source(void) {CHECK(armcpu_sleep_decide(27, 1, 27) == ArmcpuWakeSource);}

static void two_rounds_same_result(void)
{
    uint32_t saved[ARMCPU_SLEEP_SPI_WORDS] = {0}, copy[ARMCPU_SLEEP_SPI_WORDS], a[ARMCPU_SLEEP_SPI_WORDS], b[ARMCPU_SLEEP_SPI_WORDS];
    set(saved, 240); set(saved, 513); set(saved, 615); set(saved, 1019);
    memcpy(copy, saved, sizeof saved);
    const uint32_t wake[] = {240, 513};
    armcpu_sleep_keep(saved, wake, 2, a);
    armcpu_sleep_keep(saved, wake, 2, b);
    CHECK(!memcmp(a, b, sizeof a) && !memcmp(saved, copy, sizeof saved));
}

int main(void)
{
    keep_only_enabled_wake_spis();
    keep_ignores_out_of_range();
    decide_board_wake();
    decide_timer();
    decide_none_1023();
    timer_and_wake_source_prefers_source();
    two_rounds_same_result();
    printf("checks %u\n", checks);
    return 0;
}
