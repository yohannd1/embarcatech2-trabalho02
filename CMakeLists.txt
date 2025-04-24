cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)

set(PROJECT et2_trabalho02)
project(${PROJECT} C CXX ASM)

pico_sdk_init()

add_executable(${PROJECT}
    src/main.c
    src/ssd1306.c
    src/ws2812b_matrix.c
)

target_include_directories(${PROJECT} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(${PROJECT}
    pico_stdlib
    hardware_adc
    hardware_i2c
    hardware_pio
)

pico_generate_pio_header(${PROJECT} ${CMAKE_CURRENT_LIST_DIR}/src/ws2812b.pio)

pico_set_program_name(${PROJECT} "${PROJECT}")
pico_set_program_version(${PROJECT} "0.1")

pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 1)
pico_add_extra_outputs(${PROJECT})
