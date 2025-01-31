# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(open_sesame C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(open_sesame
        open_sesame.c
        ws_client.c)

pico_set_program_name(open_sesame "open_sesame")
pico_set_program_version(open_sesame "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(open_sesame 0)
# Allow picotool to reload firmware without unplugging
pico_enable_stdio_usb(open_sesame 1)

# Add the standard library to the build
target_link_libraries(open_sesame
        pico_stdlib
        hardware_adc
        pico_cyw43_arch_lwip_threadsafe_background)

if(NOT WIFI_SSID OR NOT WIFI_PASSWORD)
    message(FATAL_ERROR "WIFI_SSID or WIFI_PASSWORD is undefined.")
endif()

if(NOT SERVER_IP)
    message(FATAL_ERROR "SERVER_IP is undefined")
endif()

target_compile_definitions(open_sesame PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        SERVER_IP=\"${SERVER_IP}\")

# All warnings as errors
add_compile_options(-Werror)

# Add the standard include files to the build
target_include_directories(open_sesame PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(open_sesame)

