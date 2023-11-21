# Pre-requisites
1. Download msys2 and use MSYS environment (not mingw64, mingw32, ...) or use WSL

2. Downloading RTOS SDK
- ```git clone https://github.com/espressif/ESP8266_RTOS_SDK.git esp8266-rtos-sdk --recurse-submodule```

3. Setup RTOS SDK python environment
- ```cd esp8266-rtos-sdk```
- ```python -m venv venv```
- ```source ./venv/bin/activate```
- ```SETUPTOOLS_USE_DISTUTILS=stdlib pip install -r requirements``` 
    - [fix_1](https://stackoverflow.com/a/76882830)

4. Downloading xtensa-lx106 toolchain
- [Download from here](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/linux-setup.html)
- ```tar -xzf xtensa-lx106-*.tar.gz```  

# Build
1. ```bash```
2. ```source setup_esp8266_environment.sh```
3. ```cmake_configure.sh```
4. ```ninja -C build```

# Flashing
1. Determine serial port from ```/dev/tty??```
2. Hold flash button on ESP8266-12E board while running next command
3. ```ESPPORT=/dev/tty?? ninja -C build flash```
