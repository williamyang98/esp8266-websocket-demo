set(SRC_FILES
    "src/httpd_main.c"
    "src/httpd_parse.c"
    "src/httpd_sess.c"
    "src/httpd_txrx.c"
    "src/httpd_uri.c"
    "src/httpd_ws.c"
    "src/util/ctrl_sock.c"
)
set(priv_req mbedtls lwip)
set(priv_inc_dir "include/httpd_server" "src/util" "src/port")
set(requires http_parser esp_event)

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS ${priv_inc_dir}
    REQUIRES ${requires}
    PRIV_REQUIRES ${priv_req}
)
