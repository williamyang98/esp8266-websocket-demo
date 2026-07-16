set(SRC_FILES
    "src/webserver.c"
)
idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES httpd_server spiffs
)