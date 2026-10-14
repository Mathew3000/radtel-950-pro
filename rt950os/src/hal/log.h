#ifndef RT950_LOG_H
#define RT950_LOG_H

#include <stddef.h>

void log_init(void);
void log_write(const char *s, size_t len);
void log_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
