#pragma once
#include <stddef.h>
#include <stdint.h>

enum {
    STK_OK = 0x10,
    STK_FAILED,
    STK_UNKNOWN,
    STK_NODEVICE,
    STK_INSYNC,
    STK_NOSYNC,
};

int isp_command(int ch, void* buffer, size_t length, intptr_t fd);
int isp_set_device(int devcode, size_t psz, size_t esz, size_t fsz, intptr_t fd);
int isp_load_address(uint32_t address, intptr_t fd);
int isp_read_page(void* buffer, size_t length, intptr_t fd);
int isp_prog_page(const void* buffer, size_t length, intptr_t fd);
int isp_universal(int b1, int b2, int b3, int b4, void* b4_out, intptr_t fd);
