#include "sb.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static void * oom(void * p) {
    if (!p) {
        perror("Out of Memory");
        abort();
    }
    return p;
}

void sb_init(struct sb * b) {
    b->count = 0;
    b->capacity = 256;
    b->data = oom(malloc(b->capacity));
    if (b->data) { b->data[0] = '\0'; }
}

void sb_put(struct sb * b, const char * d, int bytes) {
    if (b->data && b->count + (size_t)bytes + 1 > b->capacity) {
        b->capacity = (b->capacity + (size_t)bytes + 1) * 2;
        b->data = oom(realloc(b->data, b->capacity));
    }
    if (b->data) {
        memcpy(b->data + b->count, d, (size_t)bytes);
        b->count += (size_t)bytes;
        b->data[b->count] = '\0';
    }
}

void sb_puts(struct sb * b, const char * s) {
    sb_put(b, s, (int)strlen(s));
}

void sb_putc(struct sb * b, char c) {
    sb_put(b, &c, 1);
}

void sb_printf(struct sb * b, const char * f, ...) {
    va_list ap;
    va_start(ap, f);
    int n = vsnprintf(NULL, 0, f, ap);
    va_end(ap);
    if (n > 0) {
        if (b->data && b->count + (size_t)n + 1 > b->capacity) {
            b->capacity = (b->capacity + (size_t)n + 1) * 2;
            b->data = oom(realloc(b->data, b->capacity));
        }
        if (b->data) {
            va_start(ap, f);
            vsnprintf(b->data + b->count, (size_t)n + 1, f, ap);
            va_end(ap);
            b->count += (size_t)n;
        }
    }
}

void sb_free(struct sb * b) {
    free(b->data);
    b->data = NULL;
    b->count = 0;
    b->capacity = 0;
}
