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
int isp_command(int ch, intptr_t fd)
{
    ucomm_putc(fd, ch);
    return exec(NULL, 0, fd);
}

// STK_SET_DEVICE
int isp_set_device(int devcode, size_t fsz, size_t psz, intptr_t fd)
{
    uint8_t cmd[] = { 'B', devcode, 0, 0, 1, 1, 1, 1, 3, 0xff, 0xff, 0xff, 0xff,
        psz >> 8, psz, fsz >> 12, fsz >> 4, fsz >> 24, fsz >> 16, fsz >> 8, fsz };
    ucomm_write(fd, cmd, sizeof(cmd));
    return exec(NULL, 0, fd);
}

// STK_READ_SIGN
int isp_read_sign(uint32_t* sig, intptr_t fd)
{
    uint8_t b_out[3];
    ucomm_putc(fd, 'u');
    int resp = exec(b_out, sizeof(b_out), fd);
    if (resp == STK_OK) {
        *sig = (b_out[0] << 16) | (b_out[1] << 8) | b_out[2];
        if (*sig == 0 || *sig == 0x00ffffff)
            resp = STK_FAILED;
    }
    return resp;
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
int isp_universal(int b1, int b2, int b3, int b4, void* b_out, intptr_t fd)
{
    uint8_t cmd[] = { 'V', b1, b2, b3, b4 };
    ucomm_write(fd, cmd, sizeof(cmd));
    return exec(b_out, 1, fd);
}
