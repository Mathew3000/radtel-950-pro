#ifndef RT950_INPUT_H
#define RT950_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#define INPUT_KEY_NONE 0xFF

/*
 * Raw input state for bring-up. Key codes are the OEM firmware's: 0 and 1
 * for the direct keys on PE5 and PA12, 0x02-0x15 from the matrix (see
 * input.c). Which label each code belongs to is to be confirmed on hardware.
 */
typedef struct {
    uint8_t key;
    bool ptt;       /* PE13, active low (assumed) */
    bool ptt_ext;   /* PE12, active low (assumed) */
    uint8_t enc_ab; /* bit 1 = PB4 (A), bit 0 = PB5 (B), raw levels */
} input_raw_t;

void input_init(void);
void input_read(input_raw_t *out);

/* Call often (every loop or from a 1 kHz tick). Returns the steps since the last call. */
int32_t input_encoder_poll(void);

#endif
