cmake_minimum_required(VERSION 3.10)
project(test_gps C)

set(CMAKE_C_STANDARD 11)

# Threads
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# Fontes principais do teste
set(SOURCES
    test_gps.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/gps/gps.c

    # dependências da porta UART
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_cbf.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_dbg.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf/utl_printf.c

    # camada HAL principal
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal_cpu.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal_uart.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal_gps.c

    # porta Linux
    ${CMAKE_SOURCE_DIR}/../../../source/port/linux/port_cpu.c
    ${CMAKE_SOURCE_DIR}/../../../source/port/linux/port_uart.c
)

add_executable(test_gps ${SOURCES})

# Threads
target_link_libraries(test_gps PRIVATE Threads::Threads)

# Includes
target_include_directories(test_gps PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../source/utl/
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf/
    ${CMAKE_SOURCE_DIR}/../../../source/utl/gps/
    ${CMAKE_SOURCE_DIR}/../../../source/hal/
)

# Alternativa para outras plataformas
if(APPLE)
  target_sources(test_gps PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../source/port/mac/port_cpu.c
    ${CMAKE_SOURCE_DIR}/../../../source/port/mac/port_uart.c
  )
endif()
