#include "usbip_server.h"

#define TAG "USB/IP Server"

TaskHandle_t *tcp_server_task = NULL;

/*
1) Connect to Wifi
2) See if USB is attached
3) If USB is attached then start the tcp_server
*/

/* Initialises the server for USB IP */
esp_err_t usbip_server_init()
{
    SemaphoreHandle_t signaling_sem = xSemaphoreCreateBinary();

    // Starts reading the USB Devices
    xTaskCreatePinnedToCore(usb_host_lib_daemon_task, "daemon usb task", 4096, (void *)signaling_sem, 2, usb_daemon_task_hdl, 0);
    xTaskCreatePinnedToCore(usb_class_driver_task, "class", 4096, (void *)signaling_sem, 3, usb_class_driver_task_hdl, 0);

    ESP_LOGI(TAG, "Initialised the USB/IP server successfully");
    return ESP_OK;
}

esp_err_t usbip_server_stop()
{
    /* TODO: Stop all the running tasks wrt USB/IP protocol */
    return ESP_OK;
}