#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "utl_printf.h"
#include "utl_dbg.h"

const uint8_t* utl_log_mod_name[] = {
#define X(MOD, INDEX) (uint8_t*) #MOD,
    XMACRO_DBG_MODULES
#undef X
};

static uint32_t utl_dbg_mods_activated = 0;

const char* utl_dbg_base_name_get(const char* full_path)
{
    const char* base_name = full_path;

    while(*full_path)
    {
        if(*full_path == '/' || *full_path == '\\')
            base_name = full_path + 1;

        full_path++;
    }

    return base_name;
}

const uint8_t* utl_dbg_mod_name_get(utl_dbg_modules_t mod_idx)
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

bool utl_dbg_mod_enabled(utl_dbg_modules_t mod_idx)
{
    return (utl_dbg_mods_activated & (1 << mod_idx)) > 0;
}

void utl_dbg_dump(char* stamp, uint8_t* data, size_t size)
{
    uint8_t* ptr = data;

    utl_printf("%s", stamp);

    for(size_t pos = 0; pos < size; pos++)
    {
        if(pos && (pos % 32 == 0))
            utl_printf("\n%s", stamp);

        if(pos % 32 == 0)
            utl_printf("%04X ", (unsigned int) pos);

        utl_printf("%02X", *ptr++);
    }
    utl_printf("\n");
}

void utl_dbg_init(void)
{
}
