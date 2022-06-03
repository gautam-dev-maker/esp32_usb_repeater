#ifndef __USBIP_SERVER_H__
#define __USBIP_SERVER_H__

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "usb_handler.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"

esp_err_t usbip_server_init();

#endif