cmake_minimum_required(VERSION 3.13)
set(PROJECT pic-usb2sd)
set(PICO_PIO_USB_DIR ${CMAKE_CURRENT_LIST_DIR}/Pico-PIO-USB)
set(PICO_BOARD adafruit_feather_rp2040_usb_host)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(${PROJECT} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${PROJECT})
target_sources(${PROJECT} PRIVATE
    main.c
)
target_include_directories(${PROJECT} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

add_subdirectory(${PICO_PIO_USB_DIR} pico_pio_usb)

# use tinyusb implementation
target_compile_definitions(${PROJECT} PRIVATE PIO_USB_USE_TINYUSB)



#pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 1)

target_link_libraries(${PROJECT}
    pico_stdlib
    pico_pio_usb
    tinyusb_host
    tinyusb_device
)

pico_add_extra_outputs(${PROJECT})