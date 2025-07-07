cmake_minimum_required(VERSION 3.10)

project(app C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

set(SOURCES
    test.c
    ${CMAKE_SOURCE_DIR}/../../../source/app/app.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_dbg.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf/utl_printf.c
    ${CMAKE_SOURCE_DIR}/../../../source/utl/utl_cbf.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal_cpu.c
    ${CMAKE_SOURCE_DIR}/../../../source/hal/hal_uart.c
    ${CMAKE_SOURCE_DIR}/../../../source/port/common/port_stdout.c
    ${CMAKE_SOURCE_DIR}/../../../source/port/common/main.c
)

# Cria o executável primeiro
add_executable(app ${SOURCES})

if(WIN32)
    # Adicione drivers para Windows aqui se necessário

elseif(APPLE)
    target_sources(app PRIVATE
        ${CMAKE_SOURCE_DIR}/../../../source/port/mac/port_cpu.c
        ${CMAKE_SOURCE_DIR}/../../../source/port/mac/port_uart.c
    )

elseif(UNIX)
    target_sources(app PRIVATE
        ${CMAKE_SOURCE_DIR}/../../../source/port/linux/port_uart.c
        ${CMAKE_SOURCE_DIR}/../../../source/port/linux/port_cpu.c
    )
endif()

# Link com threads
target_link_libraries(app PRIVATE Threads::Threads)

# Inclui os headers
target_include_directories(app PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../source/utl/
    ${CMAKE_SOURCE_DIR}/../../../source/app/
    ${CMAKE_SOURCE_DIR}/../../../source/utl/printf/
    ${CMAKE_SOURCE_DIR}/../../../source/hal/
)
