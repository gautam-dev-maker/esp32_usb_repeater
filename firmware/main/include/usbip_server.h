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
#include "esp_event.h"

#include "tcp_connect.h"
#include "usb_handler.h"
#include "global.h"

ESP_EVENT_DECLARE_BASE(USBIP_EVENT_BASE);

#define USBIP_VERSION 0x0111
#define BUS_ID "3-2"

/* Command codes */
#define OP_REQ_DEVLIST 0x8005
#define OP_REQ_IMPORT 0x8003

#define USBIP_CMD_SUBMIT 0x00000001
#define USBIP_RET_SUBMIT 0x00000003

#define USBIP_CMD_UNLINK 0x00000002
#define USBIP_RET_UNLINK 0x00000004

/* Reply Codes */
#define OP_REP_DEVLIST 0x0005
#define OP_REP_IMPORT 0x0003

/* When UNLINK is successful, status is -ECONNRESET */
#define ECONNRESET 104

typedef struct tcp_data_t
{
    int sock;
    int len;
    uint8_t *rx_buffer;
} __attribute__((packed)) tcp_data;

typedef struct usbip_header_common_t
{
    uint16_t usbip_version;
    uint16_t command_code;
    uint32_t status;
} __attribute__((packed)) usbip_header_common;

typedef struct
{
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t padding;
} usbip_interface_t;

typedef struct op_rep_devlist_t
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

    usbip_interface_t intfs[10];
} __attribute__((packed)) op_rep_devlist;

typedef struct op_req_import_t
{
    uint16_t usbip_version;
    uint16_t command_code;
    uint32_t status;
    char bus_id[32];
} __attribute__((packed)) op_req_import;

typedef struct op_rep_import_t
{
    uint16_t usbip_version;
    uint16_t reply_code;
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

} __attribute__((packed)) op_rep_import;

typedef struct usbip_header_basic_t
{
    uint32_t command;
    uint32_t seqnum;
    uint32_t devid;
    uint32_t direction;
    uint32_t ep;
} __attribute__((packed)) usbip_header_basic;

typedef struct device_desc_t
{

    uint8_t bLength;         /**< Size of the descriptor in bytes */
    uint8_t bDescriptorType; /**< DEVICE Descriptor Type */
    uint16_t bcdUSB;         /**< USB Specification Release Number in Binary-Coded Decimal (i.e., 2.10 is 210H) */
    uint8_t bDeviceClass;    /**< Class code (assigned by the USB-IF) */
    uint8_t bDeviceSubClass; /**< Subclass code (assigned by the USB-IF) */
    uint8_t bDeviceProtocol; /**< Protocol code (assigned by the USB-IF) */
    uint8_t bMaxPacketSize0; /**< Maximum packet size for endpoint zero (only 8, 16, 32, or 64 are valid) */

    // uint16_t idVendor;                  /**< Vendor ID (assigned by the USB-IF) */
    // uint16_t idProduct;                 /**< Product ID (assigned by the manufacturer) */
    // uint16_t bcdDevice;                 /**< Device release number in binary-coded decimal */
    // uint8_t iManufacturer;              /**< Index of string descriptor describing manufacturer */
    // uint8_t iProduct;                   /**< Index of string descriptor describing product */
    // uint8_t iSerialNumber;              /**< Index of string descriptor describing the device’s serial number */
    // uint8_t bNumConfigurations;         /**< Number of possible configurations */
} __attribute__((packed)) device_desc;

typedef struct usbip_cmd_submit_t
{
    uint32_t transfer_flags;
    uint32_t transfer_buffer_length;

    uint32_t start_frame;
    uint32_t number_of_packets;
    uint32_t interval;

    usb_setup_packet_t setup;
    uint8_t transfer_buffer[1024];
} __attribute__((packed)) usbip_cmd_submit;

typedef struct usbip_submit_t
{
    // usbip_header_basic base;
    // uint32_t transfer_flags;
    // uint32_t transfer_buffer_length;

    // uint32_t start_frame;
    // uint32_t number_of_packets;
    // uint32_t interval;

    // usb_setup_packet_t setup;
    // uint8_t transfer_buffer[1024];
    usbip_header_basic header;
    usbip_cmd_submit cmd_submit;
    int sock;
} __attribute__((packed)) submit;

typedef struct usbip_ret_submit_t
{
    usbip_header_basic base;
    uint32_t status;
    uint32_t actual_length;
    uint32_t start_frame;
    uint32_t number_of_packets;
    uint32_t error_count;

    unsigned char padding[8];
    // device_desc transfer_buffer;
    uint8_t transfer_buffer[1024];
} __attribute__((packed)) usbip_ret_submit;

typedef struct usbip_cmd_unlink_t
{
    uint32_t unlink_seqnum;
    unsigned char padding[24];
} __attribute__((packed)) usbip_cmd_unlink;

typedef struct usbip_ret_unlink_t
{
    usbip_header_basic base;
    uint32_t status;
    unsigned char padding[24];
} __attribute__((packed)) usbip_ret_unlink;

/* Initialise USB/IP Server */
esp_err_t usbip_server_init();

/* Stop the USB/IP Server and deletes all the tasks */
esp_err_t usbip_server_stop();

#endif