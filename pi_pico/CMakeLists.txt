cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(tresor_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(tresor
    tresor.c
)

pico_enable_stdio_usb(tresor 1)
pico_enable_stdio_uart(tresor 1)

pico_add_extra_outputs(tresor)

target_link_libraries(tresor pico_stdlib hardware_pwm hardware_irq hardware_uart pico_multicore)
