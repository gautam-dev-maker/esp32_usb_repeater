#include "tcp_connect.h"
// #include <errno.h>

#define TAG "TCP_CONNECT"

/* TODO: Make the variable "device_busy" false if the usb is unbound */
bool device_busy = false;
// static ssize_t size;
// static char rx_buffer[128];

esp_err_t tcp_server_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    return ESP_OK;
}

static void do_recv(const int sock)
{
    int len;
    if (!device_busy)
    {
        usbip_header_common dev_recv;
        len = recv(sock, &dev_recv, sizeof(usbip_header_common), MSG_DONTWAIT);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        }
        else if (len == 0)
        {
            ESP_LOGW(TAG, "Connection closed");
        }
        else if (ntohs(dev_recv.usbip_version) == USBIP_VERSION)
        {
            switch (ntohs(dev_recv.command_code))
            {
            case OP_REQ_DEVLIST:;
                op_rep_devlist dev_tcp;
                get_op_rep_devlist(&dev_tcp);
                send(sock, &dev_tcp, sizeof(op_rep_devlist), 0);
                break;

            case OP_REQ_IMPORT:;
                /* TODO: perform error check as perfomed above after receiving */
                op_req_import dev_import;
                recv(sock, dev_import.bus_id, sizeof(op_req_import) - sizeof(usbip_header_common), 0);
                if (!strcmp(BUS_ID, dev_import.bus_id))
                {
                    ESP_LOGI(TAG, "BUS-ID matches for requested import device");

                    /* TODO: send a "op_rep_import" struct as a reply */
                    op_rep_import rep_import;
                    get_op_rep_import(&rep_import);
                    send(sock, &rep_import, sizeof(op_rep_import), 0);

                    /* TODO: if send is successful set device_busy true */
                    device_busy = true;
                }
                else
                {
                    ESP_LOGE(TAG, "Received different BUS ID");
                    /* TODO: reply with "1" status to show error in connection*/
                }
                break;
            }
        }
    }
    else
    {
        /* TODO : Start dealing with URB command codes */
        usbip_header_basic header;
        len = recv(sock, &header, sizeof(usbip_header_basic), 0);
        if (len < 0)
        {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        }
        else if (len == 0)
        {
            ESP_LOGW(TAG, "Connection closed");
        }
        else
        {
            switch (ntohl(header.command))
            {
            case USBIP_CMD_SUBMIT:;
                /* TODO: REPLY with USBIP_RET_SUBMIT */
                usbip_cmd_submit cmd_submit;
                ESP_LOGI(TAG, "usbip header basic seqnum: %u", ntohl(header.seqnum));
                ESP_LOGI(TAG, "usbip header basic devid: %u", ntohl(header.devid));
                ESP_LOGI(TAG, "usbip header basic direcn: %u", ntohl(header.direction));
                ESP_LOGI(TAG, "usbip header basic ep: %u", ntohl(header.ep));
                ESP_LOGI(TAG, "--------------------------");
                len = recv(sock, &cmd_submit, sizeof(usbip_cmd_submit), 0);
                ESP_LOGI(TAG, "cmd_submit header transfer flag: %u", ntohl(cmd_submit.transfer_flags));
                ESP_LOGI(TAG, "cmd_submit header transfer_buffer_length: %u", ntohl(cmd_submit.transfer_buffer_length));
                ESP_LOGI(TAG, "cmd_submit header start_frame: %u", ntohl(cmd_submit.start_frame));
                ESP_LOGI(TAG, "cmd_submit header number_of_packets: %u", ntohl(cmd_submit.number_of_packets));
                ESP_LOGI(TAG, "cmd_submit header interval: %u", ntohl(cmd_submit.interval));
                ESP_LOGI(TAG, "--------------------------");
                ESP_LOGI(TAG, "----------Setup Packet in Hex--------");
                // len = recv(sock, &bmRequestType, sizeof(bmRequestType), 0);
                ESP_LOGI(TAG, "cmd_submit header bmRequestType: %x", (cmd_submit.setup.bmRequestType));
                // len = recv(sock, &bRequest, sizeof(bRequest),0);
                ESP_LOGI(TAG, "cmd_submit header bRequest: %x", cmd_submit.setup.bRequest);
                // len = recv(sock, &wValue, sizeof(wValue), 0);
                ESP_LOGI(TAG, "cmd_submit header wValue: %x", ntohs(cmd_submit.setup.wValue));
                // len = recv(sock, &wIndex, sizeof(wIndex), 0);
                ESP_LOGI(TAG, "cmd_submit header wIndex: %x", ntohs(cmd_submit.setup.wIndex));
                // len = recv(sock, &wLength, sizeof(wLength), 0);
                ESP_LOGI(TAG, "cmd_submit header wLength: %x", ntohs(cmd_submit.setup.wLength));
                ESP_LOGI(TAG, "length of cmd_submit %u", len);
                ESP_LOGI(TAG, "--------------------------");

                // usbip_ret_submit ret_submit;
                // device_desciptor dev_descrip;
                get_usbip_ret_submit(&cmd_submit, &header, sock);
                // len = send(sock, &ret_submit, sizeof(usbip_ret_submit), 0);
                // ESP_LOGI(TAG, "Submitted ret_submit header %d", len);
                // len = send(sock, &dev_descrip, sizeof(device_desciptor), 0);
                // ESP_LOGI(TAG, "Submitted ret_submit %d", len);
                ESP_LOGI(TAG, "--------------------------");
                
                // size = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                // if ((size) == -1)
                // {
                //     ESP_LOGI(TAG, "recv: %s (%d)\n", strerror(errno), errno);
                // }
                // rx_buffer[size] = 0; // Null-terminate whatever is received and treat it like a string
                // ESP_LOGI(TAG, "Received %d bytes: %s", size, rx_buffer);
                // for (int i = 0; i < size ; ++i) {
                //     printf("%u, ", rx_buffer[i]);
                // }
                break;

            case USBIP_CMD_UNLINK:;
                ESP_LOGI(TAG, "------In Unlink-------");
                ESP_LOGI(TAG, "usbip header basic seqnum: %u", ntohl(header.seqnum));
                ESP_LOGI(TAG, "usbip header basic devid: %u", ntohl(header.devid));
                ESP_LOGI(TAG, "usbip header basic direcn: %u", ntohl(header.direction));
                ESP_LOGI(TAG, "usbip header basic ep: %u", ntohl(header.ep));

                usbip_cmd_unlink cmd_unlink;
                len = recv(sock, &cmd_unlink, sizeof(usbip_cmd_unlink), 0);
                ESP_LOGI(TAG, "usbip header basic ep: %u", ntohl(cmd_unlink.unlink_seqnum));
                ESP_LOGI(TAG, "--------------------------");

                /* TODO: REPLY with RET_UNLINK after error check*/
                // usbip_ret_unlink ret_unlink;
                // get_usbip_ret_unlink(&ret_unlink);
                // len = send(sock, &ret_unlink, sizeof(usbip_ret_unlink), 0);
                // ESP_LOGI(TAG, "Submitted ret_unlink %u", len);
                // ESP_LOGI(TAG, "--------------------------");

                /* TODO: change device_busy variable to free */
                device_busy = false;
                break;
            }
        }
    }
}

void tcp_server_start(void *pvParameters)
{
    char addr_str[128];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(PORT);
    ip_protocol = IPPROTO_IP;

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0)
    {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }
    while (1)
    {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

        // Convert ipv4 address to string
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);

        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        do
        {
            do_recv(sock);
        } while (device_busy);

        shutdown(sock, 0);
        close(sock);
    }
CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}