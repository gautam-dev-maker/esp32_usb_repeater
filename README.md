# esp32_usb_repeater
A Over the Wifi ESP32 USB Repeater

<!-- ABOUT THE PROJECT -->
## About The Project

* The goal of this project is to use the USBIP protocol to share USB devices from the microcontroller esp32s2 to a PC over IP.
* All types of USB transfers should be supported.

### File Structure
     .
    ├── Test                          # Contains captured TCP packets for usbip.
    ├── firmware                      # Contains files of specific library of functions or Hardware used.
    │    ├── main                     # Source files of project
    │        ├──include               # Header files of the code.
    │        │     ├──global.h        # Variables used globally in our code.
    │        │     ├──tcp_connect.h   
    │        │     ├──usb_handler.h
    │        │     ├──usbip_server.h
    │        ├──src                   # Code
    │        │     ├──main.c
    │        │     ├──tcp_connect.c   # Handles TCP connection
    │        │     ├──usb_handler.c   # Handles esp32s2 usb_host_lib(transfers for devices) and sends ret_submit.
    │        │     ├──usbip_server.c  # Handles Usbip server.
    │        ├──CMakeLists.txt        # To include source code files in esp-idf.
    │    ├──CMakeLists.txt            # To include this component in a esp-idf.
    ├── assets                        # Contains flowchart.
    ├── LICENSE
    └── README.md 
    
    
<!-- GETTING STARTED -->
## Getting Started

### Prerequisites

* **ESP-IDF v5.0 and above**
  You can visit the [ESP-IDF Programmming Guide]
  (https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s2/get-started/linux-macos-setup.html) for guided installation.
  
### Installation
Clone the repo
```ssh
git clone git@github.com:gautam-dev-maker/esp32_usb_repeater.git
```

<!-- USAGE EXAMPLES -->
## Server side setup.
### Configuration
```
idf.py menuconfig
```
* `Example configuration`
  * Enter ssid and password.
  
### Build
```
idf.py build
```
### Flash and Monitor
* Connect the esp32s2 to your computer and run the following command.
```
idf.py -p /dev/ttyUSB0 flash monitor
```

<!-- Client side setup -->
## Client side setup.
### To list the device
```
sudo usbip list -r <insert server IP>
```
### To attach/import the device.
```
sudo usbip attach -r <insert server IP> -b 3-2
```
### To view the attached/imported device.
```
sudo usbip port
```
### To detach the device.
```
sudo ubsip detach -p <insert port number here>
```

<!-- Result -->
## Result till date.
### Mouse and keyboard
* Devices of the HID class, such as the mouse and keyboard, were successfully connected and tested.
* The result .pcapng files, for tcp connection between esp32s2 and pc, can be found in test directory of the repository.


<!-- Debugging -->
## Method used for debugging:
* The wireshark files generated for the USBIP connection between the esp32s2 and the PC were compared to the wireshark files generated for the USBIP connection between two computers. The TCP capture is uploaded in the test folder.


<!-- Explaining the code -->
## Code:
### main.c
* calls `tcp_server_init()` - begin the tcp server.
* calls `usbip_server_init()` - register the event control loops and create task handling the host library.
### tcp_connect.c
* `tcp_server_init()` - initializes nvs flash, netif, example connect and returns ESP_OK.
* `tcp_server_start()` - connects to the wifi and binds the socket. Creates task `do_recv` with idle priority.
* `do_recv` - This task runs in the background indefinitely and listens for messages from the client over IP. If device_busy is set to false, no device is connected, and the client can request a list of devices or import a device. If device_busy is true, the connection has already been established. This task sends the received data to two separate event loops, one for op_rep_import and one for usbip_ret_submit.
### usb_handler.c
* `usb_host_lib_daemon_task()` - installs host library and deletes it when there are no devices connected. Continuosly checks whether the devices are connected or not.
* `usb_class_driver_task()` - registers client and event control loop for ret_submit, opens device and creates task `tcp_server_task()`.
* `_usb_ip_event_handler_2()` - event loop to submit control and non control transfers to the device.
* `transfer_cb()` - call back function registered for non-control transfers.
* `transfer_cb_ctrl()` - call back function registered for control transfers.
### usbip_server.c
* `usbip_server_init()` - register the event control loops and create task handling the host library.
* `_usb_ip_event_handler_1()` - event loop to send responses for op_req_devlist and op_req_import.

## FLowchart:
![ Flowchart ](https://github.com/gautam-dev-maker/esp32_usb_repeater/blob/dev_viraj/assets/flow_of_code.png)

<p>
<img src = "https://github.com/gautam-dev-maker/esp32_usb_repeater/blob/dev_viraj/assets/flow_of_code.png" alt = "Flowchart" width = "550" height = "600"/>
</p>
