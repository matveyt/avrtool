//
// uComm
// Minimalist cross-platform serial port library
//
// https://github.com/matveyt/ucomm
//

#if !defined(UCOMM_H)
#define UCOMM_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define UCOMM_DEFAULT_TIMEOUT 300

// open port (blocking write only)
intptr_t ucomm_open(const char* port, unsigned baud, unsigned config);
// // 115200 bps 8-N-1
// intptr_t fd = ucomm_open("/dev/ttyUSB0", 115200, 0x801);

// close port
int ucomm_close(intptr_t fd);

// reset port configuration (also, discard I/O buffers)
int ucomm_reset(intptr_t fd, unsigned baud, unsigned config);

// discard I/O buffers
int ucomm_purge(intptr_t fd);

// set timeout (0 for immediate return)
// note: on __unix__ timeout is rounded up to 100 ms
int ucomm_timeout(intptr_t fd, unsigned ms);

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

// get ports list (in ucomm_ports.c)
size_t ucomm_ports(char*** ports);
// char** ports;
// size_t n = ucomm_ports(&ports);
// if (n > 0) {
//     for (size_t i = 0; i < n; ++i)
//         puts(ports[i]);
//     assert(ports[n] == NULL);
//     free(ports);
// } else
//     assert(ports == NULL);

#if defined(__cplusplus)
}
#endif

#endif // UCOMM_H
