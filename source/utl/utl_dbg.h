#pragma once

#ifdef _cplusplus
extern "C" {
#endif

#define XMACRO_DBG_MODULES  \
    X(UTL_DBG_MOD_APP, 0)   \
    X(UTL_DBG_MOD_SER, 1)   \
    X(UTL_DBG_MOD_ADC, 2)

typedef enum utl_dbg_modules_e
{
#define X(MOD, INDEX) MOD = INDEX,
    XMACRO_DBG_MODULES
#undef X
} utl_dbg_modules_t;

void utl_dbg_init(void);
void utl_dbg_mod_enable(utl_dbg_modules_t mod_idx);
void utl_dbg_mod_disable(utl_dbg_modules_t mod_idx);
bool utl_dbg_mod_is_enabled(utl_dbg_modules_t mod_idx);
void utl_dbg_printf(utl_dbg_modules_t mod_idx, const char* fmt, ...);
void utl_dbg_dump(char* stamp, uint8_t* data, size_t size);
uint8_t* utl_dbg_mod_name(utl_dbg_modules_t mod_idx);

#define UTL_LOG_HEADER(mod, fmt, file, line) "[%s][%s:%d] " fmt, (char*) utl_dbg_mod_name(mod), file, line

#define UTL_DBG_PRINTF(mod, fmt, ...)                     \
    do                                                    \
    {                                                     \
        if(utl_dbg_mod_is_enabled(mod))                   \
            printf(UTL_LOG_HEADER(mod, fmt, __FILE__,__LINE__), ##__VA_ARGS__); \
    } while(0)

#define UTL_DBG_DUMP(mod, data, size)      \
    do                                     \
    {                                      \
        if(utl_dbg_mod_is_enabled(mod))    \
            utl_dbg_dump("", data, size);  \
    } while(0)

#ifdef __cplusplus
}
#endif
