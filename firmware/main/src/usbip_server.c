#include "usbip_server.h"

#define TAG "USB/IP Server"

/* Initialises the server for USB IP*/
esp_err_t usbip_server_init()
{
    ESP_LOGI(TAG, "Initialised the server successfully");
    return ESP_OK;
}