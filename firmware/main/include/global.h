#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "usb_handler.h"
#include "usbip_server.h"

extern TaskHandle_t *tcp_server_task;
extern TaskHandle_t *usb_daemon_task_hdl;
extern TaskHandle_t *usb_class_driver_task_hdl;

extern esp_event_loop_handle_t loop_handle;
extern esp_event_loop_handle_t loop_handle2;

// extern struct class_driver_t driver_obj;

/* This variable is used to determine whether the USB device is bind to any of the network */
extern bool device_busy;

#endif