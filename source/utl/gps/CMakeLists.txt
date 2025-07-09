add_library(util_gps OBJECT gps.c)
target_include_directories(util_gps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
