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

#include "tcp_connect.h"

#define USBIP_VERSION 0x0111
#define BUS_ID "3-2"

typedef struct
{
    uint16_t usbip_verson;
    uint16_t reply_code;
    uint32_t status;
    uint32_t no_of_device;
    char path[256];
    char bus_id[32];
    uint32_t busnum;
    uint32_t devnum;
    uint32_t speed;
    uint16_t id_vendor;
    uint16_t id_product;
    uint16_t bcd_device;
    uint8_t b_device_class;
    uint8_t b_device_sub_class;
    uint8_t b_device_protocol;
    uint8_t b_configuration_value;
    uint8_t b_num_configurations;
    uint8_t b_num_interfaces;
    uint8_t b_interface_class;
    uint8_t b_interface_sub_class;
    uint8_t b_interface_protocol;
    uint8_t padding;
} op_rep_devlist_t;

esp_err_t usbip_server_init();

#endif