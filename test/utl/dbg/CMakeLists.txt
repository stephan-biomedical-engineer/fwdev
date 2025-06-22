cmake_minimum_required(VERSION 3.10)
project(app C)

set(CMAKE_C_STANDARD 11)

set(SOURCES
    main.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_dbg.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf/utl_printf.c
    ${CMAKE_SOURCE_DIR}/../../../source/port/common/port_stdou.c
)

if(WIN32)

elseif(APPLE)

elseif(UNIX)
endif()

add_executable(app ${SOURCES})

target_include_directories(app PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../source/utl/
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf/
)
