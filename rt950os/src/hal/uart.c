#include "uart.h"
#include "at32f403a_407.h"

#define RX_SIZE 64U /* power of two */

static volatile uint8_t rx_buf[RX_SIZE];
static volatile uint32_t rx_head, rx_tail;

void UART4_IRQHandler(void)
{
    /* Reading STS then DT clears both RDBF and an overrun */
    if (UART4->sts & (USART_RDBF_FLAG | USART_ROERR_FLAG)) {
        uint8_t c = (uint8_t)UART4->dt;
        if (rx_head - rx_tail < RX_SIZE) {
            rx_buf[rx_head % RX_SIZE] = c;
            rx_head++;
        }
    }
}

void uart_init(void)
{
    gpio_init_type cfg;

    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_UART4_PERIPH_CLOCK, TRUE);

    gpio_default_para_init(&cfg);
    cfg.gpio_pins = GPIO_PINS_10;
    cfg.gpio_mode = GPIO_MODE_MUX;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(GPIOC, &cfg);

    gpio_default_para_init(&cfg);
    cfg.gpio_pins = GPIO_PINS_11;
    cfg.gpio_mode = GPIO_MODE_INPUT;
    cfg.gpio_pull = GPIO_PULL_UP;
    gpio_init(GPIOC, &cfg);

    usart_init(UART4, 115200, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_transmitter_enable(UART4, TRUE);
    usart_receiver_enable(UART4, TRUE);
    usart_interrupt_enable(UART4, USART_RDBF_INT, TRUE);
    NVIC_SetPriority(UART4_IRQn, 2);
    NVIC_EnableIRQ(UART4_IRQn);
    usart_enable(UART4, TRUE);
}

bool uart_getc(uint8_t *c)
{
    if (rx_tail == rx_head) {
        return false;
    }
    *c = rx_buf[rx_tail % RX_SIZE];
    rx_tail++;
    return true;
}

void uart_putc(uint8_t c)
{
    while (!(UART4->sts & USART_TDBE_FLAG)) {
    }
    UART4->dt = c;
}
