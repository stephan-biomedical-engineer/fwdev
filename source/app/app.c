#include "hal.h"
#include "app.h"

static bool app_terminate_flag = false;

void app_terminate(void)
{
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Stopping app...\n");
    app_terminate_flag = true;
}

void app_init(void)
{
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Starting app...\n");
}

bool app_loop(void)
{
    return !app_terminate_flag;
}

void app_deinit(void)
{
    UTL_DBG_PRINTF(UTL_DBG_MOD_APP, "Stopping app...\n");
}
