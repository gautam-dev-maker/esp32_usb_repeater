#include "usbip_server.h"

#define TAG "USB/IP SERVER"

ESP_EVENT_DEFINE_BASE(USBIP_EVENT_BASE);

TaskHandle_t *tcp_server_task = NULL;

esp_event_loop_handle_t loop_handle = NULL;

static void _usb_ip_event_handler_1(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    tcp_data *recv_data = (tcp_data *)event_data;
    switch (event_id)
    {
    case OP_REQ_DEVLIST:
    {
        op_rep_devlist dev;

        dev.usbip_version = htons(USBIP_VERSION);
        dev.reply_code = htons(OP_REP_DEVLIST);
        dev.status = htonl(0x00000000);
        dev.no_of_device = htonl(0x00000001);

        memset(dev.path, 0, sizeof(dev.path));
        strcpy(dev.path, "/sys/devices/pci0000:00/0000:00:1d.1/usb2/3-2");

        memset(dev.bus_id, 0, sizeof(dev.bus_id));
        strcpy(dev.bus_id, BUS_ID);

        /* TO-DO: Not sure about these */
        dev.busnum = htonl(3);
        dev.devnum = htonl(2);

        /* TO-DO: Verify fields */
        dev.speed = htonl(get_dev_info()->speed + 1); // currently sendng 1 usb low speed wireless 0x00000005 dev_info.speed
        dev.id_vendor = htons(get_dev_desc()->idVendor);
        dev.id_product = htons(get_dev_desc()->idProduct);
        dev.bcd_device = htons(get_dev_desc()->bcdDevice);

        dev.b_device_class = get_dev_desc()->bDeviceClass;
        dev.b_device_sub_class = get_dev_desc()->bDeviceSubClass;
        dev.b_device_protocol = get_dev_desc()->bDeviceProtocol;

        dev.b_configuration_value = get_config_desc()->bConfigurationValue;
        dev.b_num_configurations = get_dev_desc()->bNumConfigurations;
        dev.b_num_interfaces = get_config_desc()->bNumInterfaces;

        int offset = 0;
        for (size_t n = 0; n < get_config_desc()->bNumInterfaces; n++) // usb_net_recv failed usbip_usb_intf[1]
        {
            const usb_intf_desc_t *intf = usb_parse_interface_descriptor(get_config_desc(), n, 0, &offset);
            dev.intfs[n].bInterfaceClass = intf->bInterfaceClass;
            dev.intfs[n].bInterfaceSubClass = intf->bInterfaceSubClass;
            dev.intfs[n].bInterfaceProtocol = intf->bInterfaceProtocol;
            dev.intfs[n].padding = 0;
        }

        send(recv_data->sock, &dev, sizeof(op_rep_devlist), MSG_DONTWAIT);
        break;
    }
    case OP_REQ_IMPORT:
    {
        /* TODO: perform error check as perfomed above after receiving */
        op_req_import dev_import;
        recv(recv_data->sock, dev_import.bus_id, sizeof(op_req_import) - recv_data->len, 0);
        op_rep_import rep_import;

        if (!strcmp(BUS_ID, dev_import.bus_id))
        {
            ESP_LOGI(TAG, "BUS-ID matches for requested import device");

            rep_import.usbip_version = htons(USBIP_VERSION);
            rep_import.reply_code = htons(OP_REP_IMPORT);
            rep_import.status = htonl(0x00000000);

            memset(rep_import.path, 0, sizeof(rep_import.path));
            strcpy(rep_import.path, "/sys/rep_importices/pci0000:00/0000:00:1d.1/usb2/3-2");
            memset(rep_import.bus_id, 0, sizeof(rep_import.bus_id));
            strcpy(rep_import.bus_id, BUS_ID);

            /* Not sure about these */
            rep_import.busnum = htonl(3);
            rep_import.devnum = htonl(2);

            rep_import.speed = get_dev_info()->speed ? htonl(2) : htonl(1);
            rep_import.id_vendor = htons(get_dev_desc()->idVendor);
            rep_import.id_product = htons(get_dev_desc()->idProduct);
            rep_import.bcd_device = htons(get_dev_desc()->bcdDevice);

            rep_import.b_device_class = get_dev_desc()->bDeviceClass;
            rep_import.b_device_sub_class = get_dev_desc()->bDeviceSubClass;
            rep_import.b_device_protocol = get_dev_desc()->bDeviceProtocol;

            rep_import.b_configuration_value = get_config_desc()->bConfigurationValue;
            rep_import.b_num_configurations = get_dev_desc()->bNumConfigurations;
            rep_import.b_num_interfaces = get_config_desc()->bNumInterfaces;
        }
        else
        {
            memset(&rep_import, 0, sizeof(rep_import));
            rep_import.usbip_version = htons(USBIP_VERSION);
            rep_import.reply_code = htons(OP_REP_IMPORT);
            rep_import.status = htonl(0x00000001);
            ESP_LOGE(TAG, "Received different BUS ID");
        }

        int len = send(recv_data->sock, &rep_import, sizeof(rep_import), MSG_DONTWAIT);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Error occurred during receiving");
        }
        else if (len == 0)
        {
            ESP_LOGW(TAG, "Connection closed");
        }
        else
        {
            device_busy = true;
        }
        break;
    }
    }
}

/* Initialises the server for USB IP */
esp_err_t usbip_server_init()
{
    SemaphoreHandle_t signaling_sem = xSemaphoreCreateBinary();

    esp_event_loop_args_t loop_args = {
        .queue_size = 100,
        .task_name = "usbip_events",
        .task_priority = 21,
        .task_stack_size = 4 * 1024,
        .task_core_id = 0};

    esp_event_loop_create(&loop_args, &loop_handle);

    esp_event_handler_register_with(loop_handle, USBIP_EVENT_BASE, OP_REQ_DEVLIST, _usb_ip_event_handler_1, NULL);
    esp_event_handler_register_with(loop_handle, USBIP_EVENT_BASE, OP_REQ_IMPORT, _usb_ip_event_handler_1, NULL);

    // Starts reading the USB Devices
    xTaskCreatePinnedToCore(usb_host_lib_daemon_task, "daemon usb task", 4096, (void *)signaling_sem, 2, usb_daemon_task_hdl, 0);
    xTaskCreatePinnedToCore(usb_class_driver_task, "class", 4096, (void *)signaling_sem, 3, usb_class_driver_task_hdl, 0);

    ESP_LOGI(TAG, "Initialised the USB/IP server successfully");
    return ESP_OK;
}

esp_err_t usbip_server_stop()
{
    /* TODO: Stop all the running tasks wrt USB/IP protocol */
    return ESP_OK;
}