#ifndef __PC_IO_INTERRUPT_H__
#define __PC_IO_INTERRUPT_H__

#include <stdbool.h>
#include <esp_err.h>

typedef void (*pc_io_status_listener_t)(bool, void *);

void pc_io_interrupt_init();
esp_err_t pc_io_status_listen(pc_io_status_listener_t listener, void *args);
esp_err_t pc_io_status_unlisten(pc_io_status_listener_t listener, void *args); 

#endif