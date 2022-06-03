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
    uint16_t usbip_version;
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

typedef struct
{
    uint32_t command;
    uint32_t seqnum;
    uint32_t devid;
    uint32_t direction;
    uint32_t ep;
} usbip_header_basic_t;

typedef struct
{
    uint16_t usbip_version;
    uint32_t command_code;
    uint32_t status;
    char bus_id[32]; 
} op_req_import_t;

typedef struct
{
    uint16_t usbip_version;
    uint32_t reply_code;
    uint32_t status;
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

} op_rep_import_t;

typedef struct
{
    uint32_t transfer_flags;
    uint32_t transfer_buffer_length;

    uint32_t start_frame;
    uint32_t number_of_packets;
    uint32_t interval;

    unsigned char setup[8];
} usbip_cmd_submit_t;

typedef struct
{
    uint32_t status;
    uint32_t actual_length;
    uint32_t start_frame;
    uint32_t number_of_packets;
    uint32_t error_count;
} usbip_ret_submit_t;

typedef struct 
{
   uint32_t unlink_seqnum;
} usbip_cmd_unlink_t;

typedef struct
{
    uint32_t status;
} usbip_ret_unlink_t;

esp_err_t usbip_server_init();

#endif