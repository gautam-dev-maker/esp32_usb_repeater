#ifndef __TCP_CONNECT_H__
#define __TCP_CONNECT_H__

#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <arpa/inet.h>

#include "usb_handler.h"

#define PORT                        3240
#define KEEPALIVE_IDLE              100
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             3

esp_err_t tcp_server_init(void);
void tcp_server_start(void *pvParameters);
#endif