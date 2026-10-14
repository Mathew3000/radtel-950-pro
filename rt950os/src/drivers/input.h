#ifndef RT950_INPUT_H
#define RT950_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#define INPUT_KEY_NONE 0xFF

/*
 * Raw input state for bring-up. Key codes are positions, not functions:
 *   0       PE5 pulled low (direct key)
 *   1       PA12 pulled low (direct key)
 *   2..21   matrix step * 4 + row + 2, step 0-3 = PC0-PC3 driven low one at
 *           a time, step 4 = no column driven; row 0-3 = PD4-PD7 read low
 * The mapping from codes to labelled keys is to be filled in on hardware.
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
