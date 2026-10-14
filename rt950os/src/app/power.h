#ifndef RT950_POWER_H
#define RT950_POWER_H

/*
 * Switch the radio off when the power switch is turned off. Call every few
 * milliseconds from the main loop.
 */
void power_poll(void);

#endif
