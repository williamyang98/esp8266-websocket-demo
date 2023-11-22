Port of esp_http_server which supports websockets. 

Taken from esp-idf from [here](https://github.com/espressif/esp-idf/tree/release/v5.2/components/esp_http_server).

Changes:
- replace ```src/port/osal.h``` for ```esp8266```. 
- ```util/ctrl_sock.*``` uses older version which doesn't support IPv6.
