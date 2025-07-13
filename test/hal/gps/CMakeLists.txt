cmake_minimum_required(VERSION 3.10)
project(test_hal_gps C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# Todas as fontes da HAL e do port necessário
set(SOURCES
    test_gps.c
    # HAL core
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal_cpu.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal_uart.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal_gps.c

    # Parser interno (gps.c) e UTL usados pelo HAL-GPS
    ${CMAKE_SOURCE_DIR}/../../../source/utl/gps/gps.c

    # para portar UART no Linux
    ${CMAKE_SOURCE_DIR}/../../../source/port/linux/port_cpu.c
    ${CMAKE_SOURCE_DIR}/../../../source/port/linux/port_uart.c

    # utilitários necessários pela UART
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_cbf.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_dbg.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf/utl_printf.c
)

add_executable(test_gps ${SOURCES})

# Link com pthreads, se o port_uart usar threads
target_link_libraries(test_gps PRIVATE Threads::Threads)

# Includes
target_include_directories(test_gps PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../source/hal
    ${CMAKE_SOURCE_DIR}/../../../source/utl/gps
    ${CMAKE_SOURCE_DIR}/../../../source/utl
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf
)
