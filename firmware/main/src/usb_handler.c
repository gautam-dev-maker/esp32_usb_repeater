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
static class_driver_t driver_obj;
const usb_ep_desc_t *ep;
static int skt;

esp_event_loop_handle_t loop_handle2 = NULL;

usb_device_info_t *get_dev_info()
{
    return &dev_info;
}

const usb_device_desc_t *get_dev_desc()
{
    return dev_desc;
}
const usb_config_desc_t *get_config_desc()
{
    return config_desc;
}

class_driver_t *get_driver_obj()
{
    return &driver_obj;
}

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

static void transfer_cb_ctrl(usb_transfer_t *transfer)
{
    ESP_LOGI(TAG, "Transfer status %d, actual number of bytes transferred %d\n", transfer->status, transfer->actual_num_bytes);
    ret_submit.actual_length = htonl(transfer->actual_num_bytes - 8);
    memcpy(&ret_submit.transfer_buffer[0], transfer->data_buffer + 8, transfer->actual_num_bytes - 8);
    // printf("%s\n", ret_submit.transfer_buffer);
    int len = send(skt, &ret_submit, sizeof(usbip_ret_submit) - 1024 + transfer->actual_num_bytes - 8, 0);
    ESP_LOGI(TAG, "Submitted ret_submit header for transfer_ctrl_submit %d", len);
    usb_host_transfer_free(transfer);
}

static void transfer_cb(usb_transfer_t *transfer)
{
    ESP_LOGI(TAG, "Transfer status %d, actual number of bytes transferred %d\n", transfer->status, transfer->actual_num_bytes);
    // usbip_ret_submit *ret = (usbip_ret_submit *)transfer->context;

    memcpy(&ret_submit.transfer_buffer[0], transfer->data_buffer, ntohl(ret_submit.actual_length)); // ep->wMaxPacketSize
    int len = send(skt, &ret_submit, sizeof(usbip_ret_submit) - 1024 + ntohl(ret_submit.actual_length), 0);
    ESP_LOGI(TAG, "Submitted ret_submit header for transfer_submit %d", len);
    usb_host_transfer_free(transfer);
}

// void get_usbip_ret_submit(usbip_cmd_submit *dev, usbip_header_basic *header, int sock)
static void _usb_ip_event_handler_2(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    submit *recv_submit = (submit *)event_data;
    ret_submit.base.command = htonl(USBIP_RET_SUBMIT);
    ret_submit.base.seqnum = recv_submit->header.seqnum; // Add Seqnum
    ret_submit.base.devid = htonl(0x00000000);
    ret_submit.base.direction = htonl(0x00000000);
    ret_submit.base.ep = htonl(0x00000000);

    ret_submit.status = htonl(0x00000000);
    ret_submit.start_frame = htonl(0x00000000);
    ret_submit.number_of_packets = htonl(0x00000000);
    ret_submit.error_count = htonl(0x00000000);
    ret_submit.actual_length = recv_submit->cmd_submit.transfer_buffer_length;

    memset(ret_submit.padding, 0, sizeof(ret_submit.padding));

    skt = recv_submit->sock;
    usb_transfer_t *transfer = NULL;
    esp_err_t err = usb_host_transfer_alloc(1000, 0, &transfer);

    ESP_LOGI("Host_Allocation", "Return Value %x", err);
    err = usb_host_interface_claim(driver_obj.client_hdl, driver_obj.dev_hdl, 0, 0);
    transfer->context = &ret_submit;

    printf("Transfer Buffer Length: %lx\n", ntohl(recv_submit->cmd_submit.transfer_buffer_length));
    memcpy(transfer->data_buffer, (void *)&recv_submit->cmd_submit.setup, 8);

    transfer->device_handle = driver_obj.dev_hdl;
    if (recv_submit->header.ep == 0)
    {
        // memcpy(transfer->data_buffer, (void *)&recv_submit.setup, ntohl(dev->transfer_buffer_length));
        printf(" %x\n", *transfer->data_buffer);
        printf(" %x\n", *(transfer->data_buffer + 1));
        printf(" %x\n", *(transfer->data_buffer + 2));
        printf(" %x\n", *(transfer->data_buffer + 3));
        printf(" %x\n", *(transfer->data_buffer + 4));
        printf(" %x\n", *(transfer->data_buffer + 5));
        printf(" %x\n", *(transfer->data_buffer + 6));
        printf(" %x\n", *(transfer->data_buffer + 7));
        transfer->callback = transfer_cb_ctrl;
        transfer->bEndpointAddress = (ntohl(recv_submit->header.ep) | (ntohl(recv_submit->header.direction) << 7));
        transfer->num_bytes = ntohl(recv_submit->cmd_submit.transfer_buffer_length) + sizeof(usb_setup_packet_t);
        err = usb_host_transfer_submit_control(driver_obj.client_hdl, transfer);
    }
    else
    {
        transfer->callback = transfer_cb;
        transfer->bEndpointAddress = (ntohl(recv_submit->header.ep) | ntohl(recv_submit->header.direction << 7)); // ep->bEndpointAddress;
        transfer->num_bytes = ntohl(recv_submit->cmd_submit.transfer_buffer_length);
        err = usb_host_transfer_submit(transfer);
        printf("Transfer Submit%d\n", err);
    }
    // ESP_LOGI(TAG, "num_bytes %d wlength %d", transfer->num_bytes, recv_submit->cmd_submit.wLength);
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

        esp_event_loop_args_t loop_args = {
            .queue_size = 100,
            .task_name = "usbip_events",
            .task_priority = 21,
            .task_stack_size = 4 * 1024,
            .task_core_id = 0};

        esp_event_loop_create(&loop_args, &loop_handle2);

        esp_event_handler_register_with(loop_handle2, USBIP_EVENT_BASE, USBIP_CMD_SUBMIT, _usb_ip_event_handler_2, NULL);

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

                /* Starting the TCP server on Device Detection */
                xTaskCreatePinnedToCore(tcp_server_start, "TCP Server Start", 4096, NULL, 4, tcp_server_task, 1);
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