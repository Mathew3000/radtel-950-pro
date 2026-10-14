cmake_minimum_required(VERSION 3.22)

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake)
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

project(rt950os C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Vendor AT32F403A/407 firmware library, kept in the repo
set(AT32_LIB ${CMAKE_CURRENT_SOURCE_DIR}/../artery_cortex-m4/AT32F403A_407_Firmware_Library/libraries
    CACHE PATH "AT32F403A/407 firmware library")

add_library(at32 STATIC
    ${AT32_LIB}/cmsis/cm4/device_support/system_at32f403a_407.c
    ${AT32_LIB}/drivers/src/at32f403a_407_crm.c
    ${AT32_LIB}/drivers/src/at32f403a_407_gpio.c
    ${AT32_LIB}/drivers/src/at32f403a_407_misc.c
    ${AT32_LIB}/drivers/src/at32f403a_407_flash.c
    ${AT32_LIB}/drivers/src/at32f403a_407_usart.c
)
target_include_directories(at32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${AT32_LIB}/cmsis/cm4/core_support
    ${AT32_LIB}/cmsis/cm4/device_support
    ${AT32_LIB}/drivers/inc
)
target_compile_definitions(at32 PUBLIC AT32F403AVGT7 USE_STDPERIPH_DRIVER)

add_executable(rt950os
    startup/startup_at32f403a_407.s
    src/app/main.c
    src/app/model_info.c
    src/hal/clock.c
    src/hal/log.c
    src/hal/syscalls.c
    src/drivers/backlight.c
    src/drivers/lcd.c
    src/drivers/font8x16.c
    src/drivers/input.c
    src/drivers/bk4829.c
    src/drivers/rf.c
    src/hal/uart.c
    src/app/update.c
    src/app/power.c
)
target_include_directories(rt950os PRIVATE src src/hal src/drivers src/app)
target_compile_options(rt950os PRIVATE $<$<COMPILE_LANGUAGE:C>:-Wall -Wextra>)
target_link_libraries(rt950os PRIVATE at32)

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/ld/rt950.ld)
target_link_options(rt950os PRIVATE -T${LINKER_SCRIPT} -Wl,-Map=$<TARGET_FILE_DIR:rt950os>/rt950os.map)
set_target_properties(rt950os PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

# rt950os.bin: plain image for SWD flashing at 0x08002C00
# rt950os.btf: fwcrypt-encrypted image for the OEM USB updater or radtel_flash.py
add_custom_command(TARGET rt950os POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:rt950os> rt950os.bin
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/tools/mkbtf.py rt950os.bin rt950os.btf
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:rt950os>
    WORKING_DIRECTORY $<TARGET_FILE_DIR:rt950os>
    BYPRODUCTS rt950os.bin rt950os.btf
)
