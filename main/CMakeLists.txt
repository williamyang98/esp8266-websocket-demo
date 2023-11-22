set(SRC_FILES
    "main.c"
    "websocket_handler.c"
    "wifi_sta.c"
)

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
)
