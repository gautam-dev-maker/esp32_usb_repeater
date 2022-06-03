set(srcs "src/main.c" "src/usbip_server.c" "src/usb_handler.c")
set(include_dirs include)
idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES lwip usb driver efuse esp32 esp_adc_cal esp_common esp_event 
                                esp_rom freertos heap log soc spiffs)