cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include($ENV{PICO_SDK_PATH}/tools/CMakeLists.txt)
include(FreeRTOS_Kernel_import.cmake)

project(pico_w_artika_sconce C CXX ASM)
set(CMAKE_C_STANDARD 11)

add_executable(${PROJECT_NAME})

pico_sdk_init()

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_add_extra_outputs(${PROJECT_NAME} 1)

pico_add_extra_outputs(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
    pico_cyw43_arch_lwip_sys_freertos
    pico_stdlib
    hardware_uart
    pico_lwip_iperf
    pico_lwip_mqtt
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
)

target_sources(${PROJECT_NAME} PRIVATE
   main.c
   ${PICO_LWIP_CONTRIB_PATH}/apps/ping/ping.c
   ${PROJECT_SOURCE_DIR}/heartbeat/heartbeat.c
   ${PROJECT_SOURCE_DIR}/terminal/terminal.c
   ${PROJECT_SOURCE_DIR}/networking/asMqtt.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/heartbeat
    ${PROJECT_SOURCE_DIR}/terminal
    ${PROJECT_SOURCE_DIR}/networking
    ${PROJECT_SOURCE_DIR}/ws2812
    ${PICO_LWIP_CONTRIB_PATH}/apps/ping
)
