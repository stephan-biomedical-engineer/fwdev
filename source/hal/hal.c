#include "hal.h"

void hal_deinit(void)
{
    hal_cpu_deinit();
}

void hal_init(void)
{
    utl_dbg_init();
    hal_cpu_init();

    // init C random seed
    srand(hal_cpu_random_seed_get());
}
