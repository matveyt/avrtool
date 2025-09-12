#include "isp.h"
#include "ucomm.h"

// AVRISP/STK500 execute command and read response
static int exec(void* buffer, size_t length, intptr_t fd)
{
    ucomm_putc(fd, ' ');
    int resp = ucomm_getc(fd);
    if (resp != STK_INSYNC)
        return resp;
    if (ucomm_read(fd, buffer, length) != (ssize_t)length)
        return STK_NOSYNC;
    return ucomm_getc(fd);
}

// AVRISP/STK500 generic command w/o parameters
int isp_command(int ch, void* buffer, size_t length, intptr_t fd)
{
    ucomm_putc(fd, ch);
    return exec(buffer, length, fd);
}

// STK_SET_DEVICE
int isp_set_device(int devcode, size_t psz, size_t esz, size_t fsz, intptr_t fd)
{
    uint8_t cmd[] = { 'B', devcode, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        psz >> 8, psz, esz >> 8, esz, fsz >> 24, fsz >> 16, fsz >> 8, fsz };
    ucomm_write(fd, cmd, sizeof(cmd));
    return exec(NULL, 0, fd);
}

// STK_LOAD_ADDRESS
int isp_load_address(uint32_t address, intptr_t fd)
{
    uint8_t cmd[] = { 'U', address >> 1, address >> 9 };
    ucomm_write(fd, cmd, sizeof(cmd));
    return exec(NULL, 0, fd);
}

// STK_READ_PAGE
int isp_read_page(void* buffer, size_t length, intptr_t fd)
{
    uint8_t cmd[] = { 't', length >> 8, length, 'F' };
    ucomm_write(fd, cmd, sizeof(cmd));
    return exec(buffer, length, fd);
}

// STK_PROG_PAGE
int isp_prog_page(const void* buffer, size_t length, intptr_t fd)
{
    uint8_t cmd[] = { 'd', length >> 8, length, 'F' };
    ucomm_write(fd, cmd, sizeof(cmd));
    ucomm_write(fd, buffer, length);
    return exec(NULL, 0, fd);
}

// STK_UNIVERSAL
int isp_universal(int b1, int b2, int b3, int b4, void* b4_out, intptr_t fd)
{
    uint8_t cmd[] = { 'V', b1, b2, b3, b4 };
    ucomm_write(fd, cmd, sizeof(cmd));
    return exec(b4_out, 1, fd);
}
