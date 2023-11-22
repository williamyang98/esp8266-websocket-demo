## Pre-requisites
1. Download msys2 and use MSYS environment (not mingw64, mingw32, ...) or use WSL.
2. Download RTOS SDK when cloning this repo recursively.
3. Install xtensa toolchain.
- ```./scripts/install_xtensa_toolchain.sh```.
4. Setup RTOS SDK python environment
- ```python -m venv venv```
- ```source ./venv/bin/activate``` or ```source ./venv/Scripts/activate``` depending on OS.
- ```SETUPTOOLS_USE_DISTUTILS=stdlib pip install -r vendor/esp8266-rtos-sdk/requirements.txt``` 
    - [fix_1](https://stackoverflow.com/a/76882830)

## Building binaries
1. ```bash```
2. ```source ./scripts/init_env.sh```
3. ```./scripts/cmake_configure.sh```
4. ```python ./scripts/generate_webpages.py```
4. ```ninja -C build```

## Flashing
1. Determine serial port from ```/dev/tty??```.
2. Set environment variables.
- ```export ESPPORT=/dev/tty??```
3. Hold flash button on ESP8266-12E board while running flash command.
- ```ninja -C build flash```
4. Running serial monitor.
- ```./scripts/serial_monitor.sh```

## Debugging website locally
To avoid reflashing while modifying the webpage run the website locally.
- ```./scripts/serve_local_website.sh```