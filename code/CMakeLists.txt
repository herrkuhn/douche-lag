cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_STANDARD 11)

set(PICO_PLATFORM rp2040)
set(BOARD pico_sdk)

include(pico_sdk_import.cmake)

# Run from SRAM
set(PICO_DEFAULT_BINARY_TYPE copy_to_ram)

set(PROJECT DOUCHELAG)

project(${PROJECT})

pico_sdk_init()

# Default to Release so production firmware builds with -Ofast and LOG_LEVEL=0.
# Debug builds automatically set LOG_LEVEL=3. Override either with -DLOG_LEVEL=N.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# LOG_LEVEL propagates to C as a preprocessor define consumed by debug.h.
# Defaults: Release=0 (errors only), Debug=3 (all messages). Override with -DLOG_LEVEL=N.
if(NOT DEFINED LOG_LEVEL)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(LOG_LEVEL 3)
    else()
        set(LOG_LEVEL 0)
    endif()
endif()

add_executable(${PROJECT})

# Generator expressions scope flags to this target only, avoiding interference
# with Pico SDK internals that also use target_compile_options.
target_compile_options(${PROJECT} PRIVATE
    $<$<CONFIG:Release>:-Ofast>
    $<$<CONFIG:Debug>:-O0 -g>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)

target_compile_definitions(${PROJECT} PRIVATE
    LOG_LEVEL=${LOG_LEVEL}
)

pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 1)        

pico_add_extra_outputs(${PROJECT})

target_sources(${PROJECT} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/main.c ${CMAKE_CURRENT_SOURCE_DIR}/ssd1306.c ${CMAKE_CURRENT_SOURCE_DIR}/measure.c ${CMAKE_CURRENT_SOURCE_DIR}/output.c)

target_link_libraries(${PROJECT} pico_stdlib hardware_i2c pico_multicore)
