#ifndef __USB_HANDLER_H__
#define __USB_HANDLER_H__

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"

#include "tcp_connect.h"
#include "usbip_server.h"
#include "global.h"

typedef struct
{
    usb_host_client_handle_t client_hdl;
    uint8_t dev_addr;
    usb_device_handle_t dev_hdl;
    uint32_t actions;
} class_driver_t;

/* This function will keep checking for USB Devices */
void usb_host_lib_daemon_task(void *arg);
void usb_class_driver_task(void *arg);

/* Fills the ope_rep_devlist_t struct with the required information */
esp_err_t get_op_rep_devlist(op_rep_devlist_t *dev);

#endif