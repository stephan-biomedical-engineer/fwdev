add_library(util_gps OBJECT gps.c)
target_include_directories(util_gps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(app PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../source/utl/
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf/
    #include "utl_dbg.h" // Assuming you need this to use UTL_DBG_PRINTF
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_dbg.h
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_print.h
)
