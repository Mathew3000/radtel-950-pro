#ifndef RT950_UPDATE_H
#define RT950_UPDATE_H

/*
 * Answer the OEM updater's "Upgrade Mode" handshake on the programming cable
 * and restart into the OEM bootloader, like the stock firmware does. Call
 * from the main loop; it returns only if no update was requested.
 */
void update_init(void);
void update_poll(void);

#endif
