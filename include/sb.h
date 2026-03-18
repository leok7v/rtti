#ifndef SB_H
#define SB_H
#include <stddef.h>

struct sb { // String Builder
    char * data;
    size_t count;
    size_t capacity;
};

void sb_init(struct sb * b);
void sb_put(struct sb * b, const char * d, int bytes);
void sb_puts(struct sb * b, const char * s);
void sb_putc(struct sb * b, char c);
void sb_printf(struct sb * b, const char * f, ...);
void sb_free(struct sb * b);

#endif
