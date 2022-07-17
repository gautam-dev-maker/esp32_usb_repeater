#include "tcp_connect.h"

#define TAG "TCP_CONNECT"

/* TODO: Make the variable "device_busy" false if the usb is unbound */
bool device_busy = false;

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
        op_req_devlist dev_recv;
        len = recv(sock, &dev_recv, sizeof(op_req_devlist), 0);
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
                recv(sock, dev_import.bus_id, sizeof(op_req_import) - sizeof(op_req_devlist), 0);
                if (!strcmp(BUS_ID, dev_import.bus_id))
                {
                    ESP_LOGI(TAG, "BUS-ID matches for requested import device");

                    /* TODO: send a "op_rep_import" struct as a reply */
                    op_rep_import rep_import;
                    get_op_rep_import(&rep_import);
                    send(sock, &rep_import, sizeof(op_rep_import), 0);

                    /* TODO: if send is successful set device_busy true */
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
        if (ntohs(header.usbip_version) == USBIP_VERSION)
        {
            switch (ntohl(header.command_code))
            {
            case USBIP_CMD_SUBMIT:;
                /* TODO: REPLY with USBIP_CMD_REP */
                break;

            case USBIP_CMD_UNLINK:;
                /* TODO: REPLY with */
                /* TODO: change device_busy variable to free */
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

        do_recv(sock);

        shutdown(sock, 0);
        close(sock);
    }
CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}