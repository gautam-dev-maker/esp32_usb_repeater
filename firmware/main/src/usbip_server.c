#include "usbip_server.h"

#define TAG "USB/IP Server"

/* Initialises the server for USB IP*/
esp_err_t usbip_server_init()
{
    tcp_server_init();
    xTaskCreate(tcp_server_start, "TCP Server Start", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Initialised the server successfully");
    return ESP_OK;
}