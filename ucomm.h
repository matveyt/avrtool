//
// uComm
// A very minimal cross-platform serial port library
//
// https://github.com/matveyt/ucomm
//

#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

// open port
// note: blocking I/O only (300 ms default timeout)
intptr_t ucomm_open(const char* port, unsigned baud, unsigned config);
// // 115200 bps 8-N-1
// intptr_t fd = ucomm_open("/dev/ttyUSB0", 115200, 0x801);

// close port
int ucomm_close(intptr_t fd);

// set timeout (0 for immediate return)
// note: on __unix__ timeout is rounded up to 100 ms
int ucomm_timeout(intptr_t fd, unsigned ms);

// discard I/O buffers
int ucomm_purge(intptr_t fd);

// set DTR and RTS (Cf. "set" means pulldown)
int ucomm_dtr(intptr_t fd, int pulldown);
int ucomm_rts(intptr_t fd, int pulldown);

// get number of bytes in the input buffer
ssize_t ucomm_available(intptr_t fd);

// read/write one byte
int ucomm_getc(intptr_t fd);
int ucomm_putc(intptr_t fd, int ch);

// read/write data buffer
ssize_t ucomm_read(intptr_t fd, void* buffer, size_t length);
ssize_t ucomm_write(intptr_t fd, const void* buffer, size_t length);

// get ports list (ucomm_ports.c)
char** ucomm_ports(void);
// char** ports = ucomm_ports();
// if (ports != NULL) {
//     for (size_t i = 0; ports[i] != NULL; ++i)
//         puts(ports[i]);
//     free(ports);
// }
