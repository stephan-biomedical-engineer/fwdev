#include "hal.h"
#include "app.h"

static uint8_t dev_id[HAL_CPU_ID_SIZE + 1];

void app_init(void)
{
    utl_dbg_mod_enable(UTL_DBG_MOD_APP);
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Initalizing app...\n");

    hal_cpu_id_get(dev_id);
    dev_id[HAL_CPU_ID_SIZE] = '\0'; 
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Device ID: %s\n", dev_id);
}

bool app_loop(void)
{
    
    return !app_terminate_get();
}
