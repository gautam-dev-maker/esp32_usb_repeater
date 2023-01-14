set(srcs "src/main.c" "src/usbip_server.c" "src/usb_handler.c" "src/tcp_connect.c")
set(include_dirs include)


idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES lwip usb nvs_flash driver efuse esp_wifi esp_adc esp_common esp_event 
                                esp_rom freertos heap log soc spiffs protocol_examples_common)