#include "hal.h"
#include "app.h"

static bool app_terminate_flag = false;

bool app_terminate_get(void)
{
    return app_terminate_flag;
}

void app_terminate_set(void)
{
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Stopping app...\n");
    app_terminate_flag = true;
}

__WEAK void app_init(void)
{
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Initializing app...\n");
}

__WEAK bool app_loop(void)
{
    return !app_terminate_get();
}

void app_deinit(void)
{
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Deinitializing app...\n");
}
