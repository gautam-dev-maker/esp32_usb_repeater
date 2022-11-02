#include "usb_handler.h"

#define CLIENT_NUM_EVENT_MSG 15

#define ACTION_OPEN_DEV 0x01
#define ACTION_GET_DEV_INFO 0x02
#define ACTION_GET_DEV_DESC 0x04
#define ACTION_GET_CONFIG_DESC 0x08
#define ACTION_GET_STR_DESC 0x10
#define ACTION_CLOSE_DEV 0x20
#define ACTION_EXIT 0x40

#define TAG "USB_HANDLER"

TaskHandle_t *usb_class_driver_task_hdl = NULL;
TaskHandle_t *usb_daemon_task_hdl = NULL;

static usb_device_info_t dev_info;
static const usb_device_desc_t *dev_desc;
static const usb_config_desc_t *config_desc;
static const usb_intf_desc_t *interface_desc;
static usbip_ret_submit ret_submit;
static submit recv_submit;
static class_driver_t driver_obj;
usb_transfer_t *transfer = NULL;
const usb_ep_desc_t *ep;
static int skt;

static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    class_driver_t *driver_obj = (class_driver_t *)arg;
    switch (event_msg->event)
    {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
        if (driver_obj->dev_addr == 0)
        {
            driver_obj->dev_addr = event_msg->new_dev.address;
            // Open the device next
            driver_obj->actions |= ACTION_OPEN_DEV;
        }
        break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        if (driver_obj->dev_hdl != NULL)
        {
            // Cancel any other actions and close the device next
            driver_obj->actions = ACTION_CLOSE_DEV;
        }
        break;
    default:
        // Should never occur
        abort();
    }
}

static void action_open_dev(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_addr != 0);
    ESP_LOGI(TAG, "Opening device at address %d", driver_obj->dev_addr);
    ESP_ERROR_CHECK(usb_host_device_open(driver_obj->client_hdl, driver_obj->dev_addr, &driver_obj->dev_hdl));
    // Get the device's information next
    driver_obj->actions &= ~ACTION_OPEN_DEV;
    driver_obj->actions |= ACTION_GET_DEV_INFO;
}

static void action_get_info(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    ESP_LOGI(TAG, "Getting device information");
    // usb_device_info_t dev_info;
    ESP_ERROR_CHECK(usb_host_device_info(driver_obj->dev_hdl, &dev_info));
    ESP_LOGI(TAG, "\t%s speed", (dev_info.speed == USB_SPEED_LOW) ? "Low" : "Full");
    ESP_LOGI(TAG, "\tbConfigurationValue %d", dev_info.bConfigurationValue);

    // Get the device descriptor next
    driver_obj->actions &= ~ACTION_GET_DEV_INFO;
    driver_obj->actions |= ACTION_GET_DEV_DESC;
}

static void action_get_dev_desc(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    ESP_LOGI(TAG, "Getting device descriptor");
    // const usb_device_desc_t *dev_desc;
    ESP_ERROR_CHECK(usb_host_get_device_descriptor(driver_obj->dev_hdl, &dev_desc));
    usb_print_device_descriptor(dev_desc);
    // Get the device's config descriptor next
    driver_obj->actions &= ~ACTION_GET_DEV_DESC;
    driver_obj->actions |= ACTION_GET_CONFIG_DESC;
}

static void action_get_config_desc(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    ESP_LOGI(TAG, "Getting config descriptor");
    // const usb_config_desc_t *config_desc;
    ESP_ERROR_CHECK(usb_host_get_active_config_descriptor(driver_obj->dev_hdl, &config_desc));
    usb_print_config_descriptor(config_desc, NULL);
    int offset = 0;
    const usb_intf_desc_t *intf = usb_parse_interface_descriptor(config_desc, 0, 0, &offset);
    ep = usb_parse_endpoint_descriptor_by_index(intf, 0, config_desc->wTotalLength, &offset);
    // Get the device's string descriptors next
    driver_obj->actions &= ~ACTION_GET_CONFIG_DESC;
    driver_obj->actions |= ACTION_GET_STR_DESC;
}

static void action_get_str_desc(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    // usb_device_info_t dev_info;
    // ESP_ERROR_CHECK(usb_host_device_info(driver_obj->dev_hdl, &dev_info));
    if (dev_info.str_desc_manufacturer)
    {
        ESP_LOGI(TAG, "Getting Manufacturer string descriptor");
    }
    if (dev_info.str_desc_product)
    {
        ESP_LOGI(TAG, "Getting Product string descriptor");
    }
    if (dev_info.str_desc_serial_num)
    {
        ESP_LOGI(TAG, "Getting Serial Number string descriptor");
    }
    // Nothing to do until the device disconnects
    int offset;
    interface_desc = usb_parse_interface_descriptor(config_desc, 0, usb_parse_interface_number_of_alternate(config_desc, 0), &offset);
    driver_obj->actions &= ~ACTION_GET_STR_DESC;
}

static void aciton_close_dev(class_driver_t *driver_obj)
{
    ESP_ERROR_CHECK(usb_host_device_close(driver_obj->client_hdl, driver_obj->dev_hdl));
    driver_obj->dev_hdl = NULL;
    driver_obj->dev_addr = 0;
    // We need to exit the event handler loop
    driver_obj->actions &= ~ACTION_CLOSE_DEV;
    driver_obj->actions |= ACTION_EXIT;
}

/* Fills the op_rep_devlist struct with all the required information */
void get_op_rep_devlist(op_rep_devlist *dev)
{
    dev->usbip_version = htons(USBIP_VERSION);
    dev->reply_code = htons(OP_REP_DEVLIST);
    dev->status = htonl(0x00000000);
    dev->no_of_device = htonl(0x00000001);

    memset(dev->path, 0, sizeof(dev->path));
    strcpy(dev->path, "/sys/devices/pci0000:00/0000:00:1d.1/usb2/3-2");

    memset(dev->bus_id, 0, sizeof(dev->bus_id));
    strcpy(dev->bus_id, BUS_ID);

    /* TO-DO: Not sure about these */
    dev->busnum = htonl(3);
    dev->devnum = htonl(2);

    /* TO-DO: Verify fields */
    dev->speed = htonl(dev_info.speed + 1); // currently sendng 1 usb low speed wireless 0x00000005 dev_info.speed
    dev->id_vendor = htons(dev_desc->idVendor);
    dev->id_product = htons(dev_desc->idProduct);
    dev->bcd_device = htons(dev_desc->bcdDevice);

    dev->b_device_class = dev_desc->bDeviceClass;
    dev->b_device_sub_class = dev_desc->bDeviceSubClass;
    dev->b_device_protocol = dev_desc->bDeviceProtocol;

    dev->b_configuration_value = dev_info.bConfigurationValue;
    dev->b_num_configurations = dev_desc->bNumConfigurations;
    dev->b_num_interfaces = config_desc->bNumInterfaces;

    dev->b_interface_class = interface_desc->bInterfaceClass;
    dev->b_interface_sub_class = interface_desc->bInterfaceSubClass;
    dev->b_interface_protocol = interface_desc->bInterfaceProtocol;
    dev->padding = 0x00;
}

void get_op_rep_import(op_rep_import *dev)
{
    dev->usbip_version = htons(USBIP_VERSION);
    dev->reply_code = htons(OP_REP_IMPORT);
    dev->status = htonl(0x00000000);

    memset(dev->path, 0, sizeof(dev->path));
    strcpy(dev->path, "/sys/devices/pci0000:00/0000:00:1d.1/usb2/3-2");
    memset(dev->bus_id, 0, sizeof(dev->bus_id));
    strcpy(dev->bus_id, BUS_ID);

    /* Not sure about these */
    dev->busnum = htonl(3);
    dev->devnum = htonl(2);

    dev->speed = htonl(dev_info.speed + 1);
    dev->id_vendor = htons(dev_desc->idVendor);
    dev->id_product = htons(dev_desc->idProduct);
    dev->bcd_device = htons(dev_desc->bcdDevice);

    dev->b_device_class = dev_desc->bDeviceClass;
    dev->b_device_sub_class = dev_desc->bDeviceSubClass;
    dev->b_device_protocol = dev_desc->bDeviceProtocol;

    dev->b_configuration_value = dev_info.bConfigurationValue;
    dev->b_num_configurations = dev_desc->bNumConfigurations;
    dev->b_num_interfaces = config_desc->bNumInterfaces;
}

static void transfer_cb(usb_transfer_t *transfer)
{
    // This is function is called from within usb_host_client_handle_events(). Don't block and try to keep it short
    // struct class_driver_t *driver_obj = (struct class_driver_t *)transfer->context;
    // usb_host_transfer_submit(transfer);
    // insert_end();
    // transfer->data_buffer
    ESP_LOGI(TAG, "Transfer status %d, actual number of bytes transferred %d\n", transfer->status, transfer->actual_num_bytes);
    ret_submit.actual_length = htonl(transfer->actual_num_bytes - 8);
    memcpy(&ret_submit.transfer_buffer[0], transfer->data_buffer + 8, transfer->actual_num_bytes - 8);
    int len = send(skt, &ret_submit, sizeof(usbip_ret_submit), 0);
    ESP_LOGI(TAG, "Submitted ret_submit header %d", len);
    // if (start != NULL)
    //     send_ret_submit(start->seqnum, &transfer);
    // usb_host_transfer_submit(transfer);
    // struct Node *ptr = (struct Node *)malloc(sizeof(struct Node));
    // ptr = start;
    // if (start->next != NULL)
    //     start = start->next;
    // free(ptr);
    // driver_obj->actions |= CLASS_DRIVER_ACTION_CLOSE_DEV;
}

void get_usbip_ret_submit(usbip_cmd_submit *dev, usbip_header_basic *header, int sock)
{
    ret_submit.base.command = htonl(USBIP_RET_SUBMIT);
    ret_submit.base.seqnum = htonl(header->seqnum); // Add Seqnum
    ret_submit.base.devid = htonl(0x00000000);
    ret_submit.base.direction = htonl(0x00000000);
    ret_submit.base.ep = htonl(0x00000000);

    ret_submit.status = htonl(0x00000000);
    //(66-20)
    ret_submit.start_frame = htonl(0x00000000);
    ret_submit.number_of_packets = htonl(0xffffffff);
    ret_submit.error_count = htonl(0x00000000);

    memset(ret_submit.padding, 0, sizeof(ret_submit.padding));
    // ret_submit.transfer_buffer.bLength = dev_desc->bLength;
    // ret_submit.transfer_buffer.bDescriptorType = dev_desc->bDescriptorType;
    // ret_submit.transfer_buffer.bcdUSB = htons(dev_desc->bcdUSB); //htons(0x2030);
    // ret_submit.transfer_buffer.bDeviceClass = dev_desc->bDeviceClass;
    // ret_submit.transfer_buffer.bDeviceSubClass = dev_desc->bDeviceSubClass;
    // ret_submit.transfer_buffer.bDeviceProtocol = dev_desc->bDeviceProtocol;
    // ret_submit.transfer_buffer.bMaxPacketSize0 = dev_desc->bMaxPacketSize0;

    recv_submit.base.command = ntohl(header->command);
    recv_submit.base.seqnum = ntohl(header->seqnum);
    recv_submit.base.devid = ntohl(header->devid);
    recv_submit.base.direction = ntohl(header->direction);
    recv_submit.base.ep = ntohl(header->ep);

    recv_submit.transfer_flags = ntohl(dev->transfer_flags);
    recv_submit.transfer_buffer_length = ntohl(dev->transfer_buffer_length);
    recv_submit.start_frame = ntohl(dev->start_frame);
    recv_submit.interval = ntohl(dev->interval);
    recv_submit.setup = dev->setup;
    skt = sock;
    esp_err_t err = usb_host_transfer_alloc(1000, 0, &transfer);
    // ESP_LOGI("Host_Allocation", "Return Value %x", err);
    err = usb_host_interface_claim(driver_obj.client_hdl, driver_obj.dev_hdl, 0, 0);
    transfer->callback = transfer_cb;
    transfer->context = &recv_submit;
    transfer->bEndpointAddress = ep->bEndpointAddress; //(ntohl(header->ep) | (ntohl(header->direction) << 7));
    // size_t n = ntohl(dev->transfer_buffer_length);
    memset(transfer->data_buffer, 0x00, 1000);
    transfer->device_handle = driver_obj.dev_hdl;
    transfer->num_bytes = sizeof(usb_setup_packet_t) + ntohl(dev->transfer_buffer_length); // ep->wMaxPacketSize;
    err = usb_host_transfer_submit(transfer);
    ESP_LOGI(TAG, "Error %x", err);
}

void get_usbip_ret_unlink(usbip_ret_unlink *dev)
{
    // hardcoded
    dev->base.command = htonl(USBIP_RET_UNLINK);
    dev->base.seqnum = htonl(0x00000002);
    dev->base.devid = htonl(0x00000000);
    dev->base.direction = htonl(0x00000000);
    dev->base.ep = htonl(0x00000000);

    dev->status = htonl(-ECONNRESET);

    memset(dev->padding, 0, sizeof(dev->padding));
}

void usb_class_driver_task(void *arg)
{
    SemaphoreHandle_t signaling_sem = (SemaphoreHandle_t)arg;

    /* Stores all the information with regards to the USB */

    while (1)
    {
        memset(&driver_obj, 0, sizeof(class_driver_t));

        // Wait until daemon task has installed USB Host Library
        xSemaphoreTake(signaling_sem, portMAX_DELAY);

        ESP_LOGI(TAG, "Registering Client");
        usb_host_client_config_t client_config = {
            .is_synchronous = false, // Synchronous clients currently not supported. Set this to false
            .max_num_event_msg = CLIENT_NUM_EVENT_MSG,
            .async = {
                .client_event_callback = client_event_cb,
                .callback_arg = (void *)&driver_obj,
            },
        };
        ESP_ERROR_CHECK(usb_host_client_register(&client_config, &driver_obj.client_hdl));

        while (1)
        {
            if (driver_obj.actions == 0)
            {
                usb_host_client_handle_events(driver_obj.client_hdl, portMAX_DELAY);
            }
            else
            {
                if (driver_obj.actions & ACTION_OPEN_DEV)
                {
                    action_open_dev(&driver_obj);
                }
                if (driver_obj.actions & ACTION_GET_DEV_INFO)
                {
                    action_get_info(&driver_obj);
                }
                if (driver_obj.actions & ACTION_GET_DEV_DESC)
                {
                    action_get_dev_desc(&driver_obj);
                }
                if (driver_obj.actions & ACTION_GET_CONFIG_DESC)
                {
                    action_get_config_desc(&driver_obj);
                }
                if (driver_obj.actions & ACTION_GET_STR_DESC)
                {
                    action_get_str_desc(&driver_obj);
                }
                if (driver_obj.actions & ACTION_CLOSE_DEV)
                {
                    aciton_close_dev(&driver_obj);
                }
                if (driver_obj.actions & ACTION_EXIT)
                {
                    /* TODO : Unbind the tcp socket and close the socket to prevent any error on client pc */
                    /* TODO : Delete the TCP SERVER TASK and free up the resource */
                    device_busy = false;
                    break;
                }

                // get_op_rep_devlist_function(&dev);

                /* Starting the TCP server on Device Detection */
                xTaskCreate(tcp_server_start, "TCP Server Start", 4096, NULL, 5, tcp_server_task);
                vTaskDelay(10);
            }
        }

        ESP_LOGI(TAG, "Deregistering Client");
        ESP_ERROR_CHECK(usb_host_client_deregister(driver_obj.client_hdl));
        xSemaphoreGive(signaling_sem);
        vTaskDelay(10);
    }
}

void usb_host_lib_daemon_task(void *arg)
{
    SemaphoreHandle_t signaling_sem = (SemaphoreHandle_t)arg;
    /* This will keep on looking for USB Devices */
    while (1)
    {
        ESP_LOGI(TAG, "Installing USB Host Library");
        usb_host_config_t host_config = {
            .skip_phy_setup = false,
            .intr_flags = ESP_INTR_FLAG_LEVEL1,
        };
        ESP_ERROR_CHECK(usb_host_install(&host_config));

        // Signal to the class driver task that the host library is installed
        xSemaphoreGive(signaling_sem);
        vTaskDelay(10); // Short delay to let client task spin up

        bool has_clients = true;
        bool has_devices = true;

        /* This loop will continue till the USB Device is connected */
        while (has_clients || has_devices)
        {
            uint32_t event_flags;
            ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
            {
                has_clients = false;
            }
            if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
            {
                has_devices = false;
            }
        }
        /* After USB Device is disconnected we need to free the resources */
        ESP_LOGI(TAG, "No more clients and devices");

        // Uninstall the USB Host Library
        ESP_ERROR_CHECK(usb_host_uninstall());

        xSemaphoreTake(signaling_sem, portMAX_DELAY);
        vTaskDelay(10);
    }
}