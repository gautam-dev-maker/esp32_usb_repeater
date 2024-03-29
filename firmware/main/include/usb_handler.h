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
#include "esp_event.h"
#include "usbip_server.h"

ESP_EVENT_DECLARE_BASE(USBIP_EVENT_BASE);

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

typedef struct op_rep_devlist_t op_rep_devlist;
typedef struct op_rep_import_t op_rep_import;

typedef struct usbip_cmd_submit_t usbip_cmd_submit;
typedef struct usbip_header_basic_t usbip_header_basic;
typedef struct usbip_ret_unlink_t usbip_ret_unlink;

usb_device_info_t *get_dev_info();
const usb_device_desc_t *get_dev_desc();
const usb_config_desc_t *get_config_desc();

/* Fills the usbip_ret_submit struct with the required information */
void get_usbip_ret_submit(usbip_cmd_submit *dev, usbip_header_basic *header, int sock);

/* Fills the usbip_ret_unlink struct with the required information */
void init_unlink(uint32_t seqnum);

#endif