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

extern struct class_driver_t driver_obj;

#endif