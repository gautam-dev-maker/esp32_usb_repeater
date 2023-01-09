# esp32_usb_repeater
A Over the Wifi ESP32 USB Repeater

<!-- ABOUT THE PROJECT -->
## About The Project

* The goal of this project is to use the USBIP protocol to share USB devices from the microcontroller esp32s2 to a PC over IP.
* All types of USB transfers should be supported.

### File Structure
     .
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
    ├── Test
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
git@github.com:gautam-dev-maker/esp32_usb_repeater.git
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
* The result .pcapng files can be found in test directory of the repository.
