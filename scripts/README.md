## Pre-requisites
1. Setup development environment
    - Option 1 (preferred): Use WSL2
    - Option 2 (annoying): Download msys2 and use MSYS environment (not mingw64, mingw32, ...)
2. Download RTOS SDK by cloning this repo recursively: ```git clone <URL> --recurse-submodules``` or afterwards with ```git submodule update --init --recursive```
3. Install xtensa toolchain: ```./scripts/install_xtensa_toolchain.sh```.
4. Create RTOS SDK virtual python environment: ```python -m venv venv```
5. Activate python environment: ```source ./venv/bin/activate``` or ```source ./venv/Scripts/activate``` depending on OS.
6. Install python packages: ```SETUPTOOLS_USE_DISTUTILS=stdlib pip install -r vendor/esp8266-rtos-sdk/requirements.txt``` 
    - [fix_1](https://stackoverflow.com/a/76882830)
    - ```cryptography``` library in python may not install correctly on msys2. Just comment it out in ```vendor/esp8266-rtos-sdk/requirements.txt```.
    - You need to comment it out in ```vendor/esp8266-rtos-sdk/requirements.txt``` because a python script called by their cmake configure setup checks if the requirements are installed
    - If you just avoid installing the cryptography module cmake configure fails with a message about missing python dependencies

## Programming
### 1. Building binaries
1. Start development shell: ```bash```
2. Initialise ESP environment: ```source ./scripts/init_env.sh```
3. Configure cmake: ```./scripts/cmake_configure.sh```
4. Create wifi credentials file: ```printf "#pragma once\n#define WIFI_SSID \"example_ssid\"\n#define WIFI_PASS \"example_pass\"\n" > ./main/wifi_sta_config.h```
5. Build program binary: ```cmake --build build```

### 2. Flashing program
1. Determine serial port from ```/dev/tty??```.
2. Set COM port variable: ```export ESPPORT=/dev/tty??```
3. Build program: ```cmake --build build```
4. Hold flash button on ESP8266-12E board while flashing binaries
5. Flash program: ```./scripts/flash_program.sh```
6. Running serial monitor: ```./scripts/serial_monitor.sh```

### 3. Flashing static webserver files
1. Determine serial port from ```/dev/tty??```.
2. Set COM port variable: ```export ESPPORT=/dev/tty??```
3. Indexing webserver files and creating spiffs partition: ```./scripts/create_server_files.sh```
4. Hold flash button on ESP8266-12E board while flashing binaries
5. Flash spiffs partition with static webserver files: ```./scripts/flash_server_files.sh```
6. Rerun steps 3 to 5 whenever you want to update the static webserver files in the SPIFFS partition

### 4. Additional scripts
- To avoid reflashing while modifying the webpage run the website locally: ```./scripts/serve_local_website.sh```
- To edit ```./sdkconfig``` more conveniently via a terminal UI: ```cmake --build build --target menuconfig```

## Sharing USB COM ports with WSL2
### 1. Instructions
1. Install usbipd and restart computer (powershell+admin): ```winget install usbipd```
2. List devices: ```usbipd list```
    - For the common ```CH340``` USB to UART bridge IC it shares the same hardware id ```<VID:PID>```
    - The only way to disambiguate this is with the bus id
3. Bind device (admin): ```usbipd bind --busid <BUS_ID>```
4. Start up WSL2 instance
5. Attach device: ```usbipd attach --wsl --auto-attach --busid <BUS_ID>```
6. List USB devices within WSL2 shell: ```lsusb```
7. List TTY USB devices within WSL2 shell: ```ls /dev/ttyUSB*```
7. Detach device: ```usbipd detach --busid <BUS_ID>```

**NOTE**: On the next boot simply start up a WSL2 instance, and then in Windows run: ```usbipd attach --wsl --auto-attach --busid <BUS_ID>```

### 2. Fixing VBoxUSBMon (Optional)
- You may get a warning or error about ```VBoxUSBMon``` not being installed or running
1. Check the status of VBoxUSBMon (admin): ```sc query VBoxUsbMon```
2. Try reinstalling usbipd and restarting computer (powershell+admin): ```winget uninstall usbipd``` then ```winget install usbipd```

