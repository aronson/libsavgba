#include <stddef.h>

#include <tonc.h>

#include "err_def.h"
#include "gba_flash.h"

#define MEM_FLASH 0x0E000000
#define FLASH_SIZE 0x10000
#define flash_mem ((vu8*)MEM_FLASH)

#define LOOP_CNT_PER_MILLI_SECOND 1000

const unsigned char erased_byte_value = 0xFF;

u8 device;
u8 manufacturer;

IWRAM_CODE
static void flash_memcpy(volatile unsigned char *dst, const volatile unsigned char *src, size_t size) {
    for (;size > 0;--size) 
        *dst++ = *src++;
}

IWRAM_CODE
static unsigned int flash_absmemcmp(const volatile unsigned char *dst, const volatile unsigned char *src, size_t size) {
    while (size-- > 0) {
        unsigned int a = *dst++;
        unsigned int b = *src++;
        if (a != b) 
            return 1;
    }

    return 0;
}

// wait until timeout 
static void wait(int timeout) {
    for (vu32 i = 0; i < LOOP_CNT_PER_MILLI_SECOND * timeout; i++);
}

// wait until [E00xxxxh]=dat (or timeout)
static int wait_until(u32 addr, const u8 *data, int timeout) {
    for (vu32 i = 0; i < LOOP_CNT_PER_MILLI_SECOND * timeout && flash_absmemcmp(&flash_mem[addr], data, 1); i++);

    if (flash_absmemcmp(&flash_mem[addr], data, 1)) {
        // Terminate Command after Timeout (only Macronix devices, ID=1CC2h)
        if (manufacturer == FLASH_MFR_MACRONIX && device == FLASH_DEV_MX29L512)
            // force end of write/erase command
            flash_mem[0x5555] = 0xF0;

        return E_TIMEOUT;
    }

    return 0;
}

// Chip Identification (all device types)
int flash_init() {
    // Use 8 clk waitstates for initial detection (WAITCNT Bits 0,1 both set). After detection of certain device types smaller wait values may be used for write/erase, and even smaller wait values for raw reading, see Device Types table.
    REG_WAITCNT |= WS_SRAM_8;

    // enter ID mode
    flash_mem[0x5555] = 0xAA;
    flash_mem[0x2AAA] = 0x55;
    flash_mem[0x5555] = 0x90;

    // one minor thing the atmel docs say: you have to wait 20ms when entering or exiting ID mode.
    wait(20);

    // get device & manufacturer
    flash_memcpy(&device, &flash_mem[1], 1);
    flash_memcpy(&manufacturer, &flash_mem[0], 1);

    // terminate ID mode
    flash_mem[0x5555] = 0xAA;
    flash_mem[0x2AAA] = 0x55;
    flash_mem[0x5555] = 0xF0;

    // one minor thing the atmel docs say: you have to wait 20ms when entering or exiting ID mode.
    wait(20);

    // 128K sanyo flash needs to have the "exit ID mode" written TWICE to work. If you only write it once, it will not exit ID mode.
    // 64K sanyo flash has the same device/manufacturer ID as the SST part.
    if (manufacturer == FLASH_MFR_SANYO)
        flash_mem[0x5555] = 0xF0;

    //if (!((manufacturer == FLASH_MFR_ATMEL && device == FLASH_DEV_AT29LV512) || 
    //        (manufacturer == FLASH_MFR_PANASONIC && device == FLASH_DEV_MN63F805MNP) || 
    //        (manufacturer == FLASH_MFR_SANYO && device == FLASH_DEV_LE26FV10N1TS) || 
    //        (manufacturer == FLASH_MFR_SST && device == FLASH_DEV_LE39FW512) || 
    //        (manufacturer == FLASH_MFR_MACRONIX && (device == FLASH_DEV_MX29L512 || device == FLASH_DEV_MX29L010))))
    //    return E_UNSUPPORTED_DEVICE;

    return 0;
}

// Erase Entire Chip (all device types)
int flash_reset() {
    // erase command
    flash_mem[0x5555] = 0xAA;
    flash_mem[0x2AAA] = 0x55;
    flash_mem[0x5555] = 0x80;

    // erase entire chip
    flash_mem[0x5555] = 0xAA;
    flash_mem[0x2AAA] = 0x55;
    flash_mem[0x5555] = 0x10;

    // wait until [E000000h]=FFh (or timeout)
    return wait_until(0, &erased_byte_value, 20);
}

// Erase 4Kbyte Sector (all device types, except Atmel)
int flash_erase(u32 addr) {
    // sector size: 4KB
    addr &= 0xF000;

    // erase command
    flash_mem[0x5555] = 0xAA;
    flash_mem[0x2AAA] = 0x55;
    flash_mem[0x5555] = 0x80;

    // erase sector n
    flash_mem[0x5555] = 0xAA;
    flash_mem[0x2AAA] = 0x55;
    flash_mem[addr] = 0x30;

    // wait until [E00n000h]=FFh (or timeout)
    return wait_until(addr, &erased_byte_value, 20);
}

// Reading Data Bytes (all device types)
int flash_read(u32 addr, u8 *data, size_t size) {
    if (data == NULL)
        return E_INVALID_PARAM;

    if (addr > MEM_FLASH)
        addr -= MEM_FLASH;

    if (addr > FLASH_SIZE)
        return E_OUT_OF_RANGE;

    flash_memcpy(data, &flash_mem[addr], size);
    
    return 0;
}

// Write Single Data Byte (all device types, except Atmel)
int flash_write_byte(u32 addr, u8 data) {
    // write byte command
    flash_mem[0x5555] = 0xAA;
    flash_mem[0x2AAA] = 0x55;
    flash_mem[0x5555] = 0xA0;

    // write byte to address xxxx
    flash_mem[addr] = data;

    // wait until [E00xxxxh]=dat (or timeout)
    return wait_until(addr, &data, 20);
}

int flash_write(u32 addr, u8 *data, size_t size) {
    int err;

    if (data == NULL)
        return E_INVALID_PARAM;

    if (addr > MEM_FLASH)
        addr -= MEM_FLASH;

    if (addr > FLASH_SIZE)
        return E_OUT_OF_RANGE;

    err = flash_erase(addr);
    if (err)
        return err;

    for(int i = 0; i < size; i++) {
        err = flash_write_byte(addr + i, data[i]);

        if (err)
            return err;
    }

    if (flash_absmemcmp(&flash_mem[addr], data, size))
        return E_VERIFY_FAIL;

    return 0;
}