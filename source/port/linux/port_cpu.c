// source/port/linux/port_cpu.c
#include "hal_cpu.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static void linux_cpu_init(void)   {}
static void linux_cpu_deinit(void) {}
static void linux_cpu_watchdog_refresh(void) {}
static void linux_cpu_id_get(uint8_t id[HAL_CPU_ID_SIZE]) {
    memset(id, 0, HAL_CPU_ID_SIZE);
}
static uint32_t linux_cpu_random_seed_get(void) {
    return (uint32_t)time(NULL);
}
static uint32_t linux_cpu_critical_section_enter(hal_cpu_cs_level_t lvl) {
    return 0;
}
static void linux_cpu_critical_section_leave(uint32_t lvl) {}
static void linux_cpu_low_power_enter(void) {}
static void linux_cpu_sleep_ms(uint32_t ms) {
    usleep(ms * 1000);
}
static uint32_t linux_cpu_time_get_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static const hal_cpu_driver_t linux_cpu_driver = {
    .init                   = linux_cpu_init,
    .deinit                 = linux_cpu_deinit,
    .watchdog_refresh       = linux_cpu_watchdog_refresh,
    .id_get                 = linux_cpu_id_get,
    .random_seed_get        = linux_cpu_random_seed_get,
    .critical_section_enter = linux_cpu_critical_section_enter,
    .critical_section_leave = linux_cpu_critical_section_leave,
    .low_power_enter        = linux_cpu_low_power_enter,
    .sleep_ms               = linux_cpu_sleep_ms,
    .time_get_ms            = linux_cpu_time_get_ms,
};

const hal_cpu_driver_t* HAL_CPU_DRIVER = &linux_cpu_driver;
