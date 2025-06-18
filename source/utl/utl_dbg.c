#include <stdint.h>
#include <stdbool.h>

#include "hal_dbg.h"

const uint8_t* utl_log_mod_name[] = {
#define X(MOD, INDEX) (uint8_t*) #MOD,
    XMACRO_DBG_MODULES
#undef X
};

static uint32_t utl_dbg_mods_activated = 0;

const uint8_t* utl_dbg_mod_name(utl_dbg_modules_t mod_idx)
{
    return (const uint8_t*) utl_log_mod_name[mod_idx];
}

void utl_dbg_mod_enable(utl_dbg_modules_t mod_idx)
{
    utl_dbg_mods_activated |= 1 << mod_idx;
}

void utl_dbg_mod_disable(utl_dbg_modules_t mod_idx)
{
    utl_dbg_mods_activated &= ~((uint32_t) (1 << mod_idx));
}

bool utl_dbg_mod_is_enabled(utl_dbg_modules_t mod_idx)
{
    return (utl_dbg_mods_activated & (1 << mod_idx)) > 0;
}

void utl_dbg_dump_lines(char* stamp, uint8_t* data, size_t size)
{
    uint8_t* ptr = data;

    printf("%s", stamp);

    for(size_t pos = 0; pos < size; pos++)
    {
        if(pos && (pos % 32 == 0))
            printf("\n%s", stamp);

        if(pos % 32 == 0)
            printf("%04X ", (unsigned int) pos);

        printf("%02X", *ptr++);
    }
    printf("\n");
}

void utl_dbg_init(void)
{
    utl_dbg_mod_enable(UTL_DBG_LEVEL_APP);
}
