#include "tcp_connect.h"
// #include <errno.h>

#define TAG "TCP_CONNECT"

/* TODO: Make the variable "device_busy" false if the usb is unbound */
bool device_busy = false;
static int sock;
static submit recv_submit;
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

static void do_recv()
{
    int len;
    usbip_header_common dev_recv;
    while (1)
    {
        if (!device_busy)
        {
            // usbip_header_common dev_recv;
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
                tcp_data buffer;
                buffer.sock = sock;
                buffer.len = sizeof(usbip_header_common);
                buffer.rx_buffer = (uint8_t *)&dev_recv;
                esp_event_post_to(loop_handle, USBIP_EVENT_BASE, ntohs(dev_recv.command_code), (void *)&buffer, sizeof(tcp_data), 10);
            }
        }
        if (device_busy)
        {
            /* TODO : Start dealing with URB command codes */
            usbip_header_basic header;
            len = 0;
            while (1)
            {
                len = recv(sock, &header, sizeof(usbip_header_basic), 0);

                if (len > 0)
                {
                    switch (ntohl(header.command))
                    {
                    case USBIP_CMD_SUBMIT:
                    {
                        /* TODO: REPLY with USBIP_RET_SUBMIT */
                        usbip_cmd_submit cmd_submit;
                        len = recv(sock, &cmd_submit, sizeof(usbip_cmd_submit) - 1024, MSG_DONTWAIT);
                        if (ntohl(header.direction) == 0)
                            len += recv(sock, &cmd_submit.transfer_buffer[0], ntohl(cmd_submit.transfer_buffer_length), MSG_DONTWAIT);
                        // get_usbip_ret_submit(&cmd_submit, &header, sock);
                        recv_submit.header = header;
                        recv_submit.cmd_submit = cmd_submit;
                        recv_submit.sock = sock;
                        esp_event_post_to(loop_handle2, USBIP_EVENT_BASE, USBIP_CMD_SUBMIT, (void *)&recv_submit, sizeof(submit), 10);
                        ESP_LOGI(TAG, "--------------------------");
                        break;
                    }

                    case USBIP_CMD_UNLINK:
                    {
                        // ESP_LOGI(TAG, "------In Unlink-------");
                        // ESP_LOGI(TAG, "usbip header basic seqnum: %u", ntohl(header.seqnum));
                        // ESP_LOGI(TAG, "usbip header basic devid: %u", ntohl(header.devid));
                        // ESP_LOGI(TAG, "usbip header basic direcn: %u", ntohl(header.direction));
                        // ESP_LOGI(TAG, "usbip header basic ep: %u", ntohl(header.ep));

                        usbip_cmd_unlink cmd_unlink;
                        len = recv(sock, &cmd_unlink, sizeof(usbip_cmd_unlink), 0);
                        // ESP_LOGI(TAG, "usbip header basic ep: %u", ntohl(cmd_unlink.unlink_seqnum));
                        // ESP_LOGI(TAG, "--------------------------");

                        /* TODO: REPLY with RET_UNLINK after error check*/
                        // usbip_ret_unlink ret_unlink;
                        // get_usbip_ret_unlink(&ret_unlink);
                        // len = send(sock, &ret_unlink, sizeof(usbip_ret_unlink), 0);
                        // ESP_LOGI(TAG, "Submitted ret_unlink %u", len);
                        // ESP_LOGI(TAG, "--------------------------");/
                        device_busy = false;
                        break;
                    }
                    default:
                        printf("Failure\n");
                        break;
                    }
                }
            }
        }

        if (!device_busy)
            break;
    }
    vTaskDelete(NULL);
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
        sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, 4, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, 5, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, 6, &keepCount, sizeof(int));

        // Convert ipv4 address to string
        if (source_addr.ss_family == PF_INET)
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }

        xTaskCreatePinnedToCore(do_recv, "tcp_recv", 2 * 4096, NULL, tskIDLE_PRIORITY, NULL, 1);

        // shutdown(sock, 0);
        // close(sock);
    }
CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}