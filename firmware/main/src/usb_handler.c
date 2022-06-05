#include "usb_handler.h"
#include "usbip_server.h"

#define CLIENT_NUM_EVENT_MSG 5

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

SemaphoreHandle_t signaling_sem;

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

/* Fills the dev struct with all the required information */
esp_err_t get_op_rep_devlist(op_rep_devlist_t *dev)
{
    dev->usbip_version = USBIP_VERSION;
    dev->reply_code = 0x0005;
    dev->status = 0x00000000;
    dev->no_of_device = 0x00000001;
    strcpy(dev->path, "/sys/devices/pci0000:00/0000:00:1d.1/usb2/3-2");
    strcpy(dev->bus_id, BUS_ID);

    /* Not sure about these */
    dev->busnum = 1;
    dev->devnum = 1;

    dev->speed = dev_info.speed;
    dev->id_vendor = dev_desc->idVendor;
    dev->id_product = dev_desc->idProduct;
    dev->bcd_device = dev_desc->bcdDevice;
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

    return ESP_OK;
}

void usb_class_driver_task(void *arg)
{
    /* Stores all the information with regards to the USB */
    class_driver_t driver_obj;
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
                /* Starting the TCP server on Device Detection */
                xTaskCreate(tcp_server_start, "TCP Server Start", 4096, NULL, 5, tcp_server_task);
                vTaskDelay(10);

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
                    break;
                }
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
    SemaphoreHandle_t signaling_sem = xSemaphoreCreateBinary();
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