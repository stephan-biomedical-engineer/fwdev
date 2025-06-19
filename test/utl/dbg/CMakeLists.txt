cmake_minimum_required(VERSION 3.10)
project(app C)

set(CMAKE_C_STANDARD 11)

set(SOURCES
    main.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_dbg.c
)

add_executable(app ${SOURCES})

target_include_directories(app PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../source/utl/
)
