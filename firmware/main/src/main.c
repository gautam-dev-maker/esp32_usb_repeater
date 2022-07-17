#include "usbip_server.h"
int app_main(void)
{
    tcp_server_init();
    usbip_server_init();
    return 0;
}