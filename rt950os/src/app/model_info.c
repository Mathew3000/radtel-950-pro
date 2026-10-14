/*
 * The OEM updater (RT-950_EnUPDATE, BootHelper.cs) reads 32 bytes at image
 * offset 0x3E0 (992) and sends them with CMD 0x02 so the radio can check the
 * model. The V0.18 image has "RT-950" padded with spaces and four zero bytes,
 * followed by 16 bytes that happen to be code in the OEM image. We do not know
 * yet how many of the 32 bytes the radio compares, so all of them are copied
 * verbatim; here the last 16 are plain data and never executed. The linker
 * script places this block at 0x080003E0.
 */
#include <stdint.h>

__attribute__((section(".model_info"), used))
const uint8_t rt950_model_info[32] = {
    'R',  'T',  '-',  '9',  '5',  '0',  ' ',  ' ',
    ' ',  ' ',  ' ',  ' ',  0x00, 0x00, 0x00, 0x00,
    0x0f, 0xb4, 0x04, 0x49, 0x10, 0xb5, 0x03, 0xaa,
    0x02, 0x98, 0x00, 0xf0, 0xe9, 0xfe, 0x10, 0xbc,
};
