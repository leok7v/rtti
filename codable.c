#include "codable.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *skip_ws(char *p) {
    while (*p && isspace(*p)) { p++; }
    return p;
}

static char *parse_val(char *j, void *ptr, const struct type_info *meta);

static char *parse_str(char *j, const char **output) {
    char *r = j;
    if (*j == '"') {
        j++;
        *output = j;
        while (*j && *j != '"') {
            j += (*j == '\\' && *(j + 1)) ? 2 : 1;
        }
        if (*j == '"') {
            *j = '\0';
            j++;
        }
        r = skip_ws(j);
    }
    return r;
}

static char *skip_value(char *j) {
    int d = 0, in_str = 0, n = 0;
    while (*j && n < 1000) {
        n++;
        if (*j == '"') {
            in_str = !in_str;
        } else if (*j == '\\' && in_str) {
            j++;
        } else if (!in_str) {
            if (*j == '{' || *j == '[') {
                d++;
            } else if (*j == '}' || *j == ']') {
                d--;
                if (d < 0) { break; }
            } else if ((*j == ',' || *j == '}') && d == 0) {
                break;
            }
        }
        j++;
    }
    if (n >= 1000) { fprintf(stderr, "LOOP CAUGHT AT '%10s...'\n", j); }
    return j;
}

static char *parse_field(char *j, void *ptr, const struct type_info *m,
                         const char *k) {
    char *r = NULL;
    const struct type_info *f = m;
    while (f && f->name && !r) {
        if (strcmp(f->name, k) == 0) {
            r = parse_val(j, (char *)ptr + f->offset, f);
        }
        f++;
    }
    if (!r) { r = skip_value(j); }
    return r;
}

static char *parse_obj(char *j, void *ptr, const struct type_info *m) {
    char *r = j;
    if (*j == '{') {
        j = skip_ws(j + 1);
        while (*j && *j != '}') {
            if (*j == '"') {
                const char *k = NULL;
                j = parse_str(j, &k);
                j = skip_ws(j);
                if (*j == ':') {
                    j = skip_ws(j + 1);
                    j = parse_field(j, ptr, m, k);
                }
            }
            j = skip_ws(j);
            if (*j == ',') { j = skip_ws(j + 1); }
        }
        if (*j == '}') { j = skip_ws(j + 1); }
        r = j;
    }
    return r;
}

static char *parse_array(char *j, void **output, const struct type_info *m) {
    char *r = j;
    if (*j == '[') {
        j = skip_ws(j + 1);
        size_t cap = 4, n = 0, size = m->bytes;
        void *data = calloc(cap + 1, size);
        if (data) {
            struct type_info em = *m;
            em.is_array = 0;
            while (*j && *j != ']') {
                if (n >= cap) {
                    cap *= 2;
                    void *grown = realloc(data, (cap + 1) * size);
                    if (!grown) { free(data); data = NULL; break; }
                    data = grown;
                    memset((char *)data + n * size, 0, (cap + 1 - n) * size);
                }
                void *e = (char *)data + n * size;
                char *prev = j;
                j = parse_val(j, e, &em);
                if (j == prev) { j++; } else { n++; }
                j = skip_ws(j);
                if (*j == ',') { j = skip_ws(j + 1); }
            }
            if (data) {
                if (*j == ']') { j = skip_ws(j + 1); }
                void *fin = realloc(data, (n + 1) * size);
                if (fin) { data = fin; }
                memset((char *)data + n * size, 0, size);
                *output = data;
                r = j;
            }
        }
    }
    return r;
}

static char *parse_val(char *j, void *ptr, const struct type_info *m) {
    char *r = j;
    if (j && *j) {
        if (m->is_array) {
            r = parse_array(j, (void **)ptr, m);
        } else {
            char *prev = j;
            switch (m->kind) {
                case 's': {
                    if (strncmp(j, "null", 4) == 0) {
                        *(const char **)ptr = NULL;
                        j = skip_ws(j + 4);
                    } else {
                        const char *s = NULL;
                        j = parse_str(j, &s);
                        *(const char **)ptr = s;
                    }
                    break;
                }
                case 'i': {
                    long long v = strtoll(j, &j, 10);
                    if      (m->bytes == 1) { *(int8_t  *)ptr = (int8_t )v; }
                    else if (m->bytes == 2) { *(int16_t *)ptr = (int16_t)v; }
                    else if (m->bytes == 4) { *(int32_t *)ptr = (int32_t)v; }
                    else                    { *(int64_t *)ptr = (int64_t)v; }
                    j = skip_ws(j);
                    break;
                }
                case 'd': {
                    double v = strtod(j, &j);
                    if (m->bytes == sizeof(float)) {
                        *(float *)ptr = (float)v;
                    } else {
                        *(double *)ptr = v;
                    }
                    j = skip_ws(j);
                    break;
                }
                case 'b': {
                    if (strncmp(j, "true", 4) == 0) {
                        *(bool *)ptr = true;
                        j += 4;
                    } else if (strncmp(j, "false", 5) == 0) {
                        *(bool *)ptr = false;
                        j += 5;
                    }
                    j = skip_ws(j);
                    break;
                }
                case '{': {
                    j = parse_obj(j, ptr, m->meta);
                    break;
                }
            }
            if (j == prev) {
                while (*j && *j != ',' && *j != '}' && *j != ']') { j++; }
            }
            r = j;
        }
    }
    return r;
}

void *decode(void *ptr, void *pointer_to_rtti, char *json) {
    void *r = NULL;
    if (ptr && pointer_to_rtti && json) {
        char *copy = strdup(json);
        if (copy) {
            char *p = skip_ws(copy);
            if (*p == '{') {
                parse_obj(p, ptr, (const struct type_info *)pointer_to_rtti);
            }
            r = copy;
        }
    }
    return r;
}

struct str {
    char *data;
    size_t count;
    size_t capacity;
};

static void str_init(struct str *s) {
    s->capacity = 256;
    s->count = 0;
    s->data = malloc(s->capacity);
    if (s->data) { s->data[0] = '\0'; }
}

static void str_append(struct str *s, const char *text) {
    if (s->data && text) {
        size_t len = strlen(text);
        if (s->count + len + 1 > s->capacity) {
            s->capacity = (s->capacity + len + 1) * 2;
            s->data = realloc(s->data, s->capacity);
        }
        if (s->data) {
            strcpy(s->data + s->count, text);
            s->count += len;
        }
    }
}

static void str_append_char(struct str *s, char c) {
    char t[2] = {c, '\0'};
    str_append(s, t);
}

static void encode_val(struct str *s, void *val, const struct type_info *m);

static void encode_obj(struct str *s, void *ptr, const struct type_info *m) {
    str_append_char(s, '{');
    bool first = true;
    for (const struct type_info *f = m; f && f->name; f++) {
        void *fp = (char *)ptr + f->offset;
        bool skip = (f->kind == 's' || f->is_array) && *(void **)fp == NULL;
        if (!skip) {
            if (!first) { str_append_char(s, ','); }
            first = false;
            str_append_char(s, '"');
            str_append(s, f->name);
            str_append(s, "\":");
            encode_val(s, fp, f);
        }
    }
    str_append_char(s, '}');
}

static void encode_array(struct str *s, void *arr, const struct type_info *m) {
    str_append_char(s, '[');
    struct type_info em = *m;
    em.is_array = 0;
    size_t size = m->bytes;
    void *p = arr;
    bool first = true, done = false;
    while (!done) {
        bool is_null = true;
        if (m->kind == 's') {
            is_null = *(char **)p == NULL;
        } else {
            for (size_t i = 0; i < size && is_null; i++) {
                if (((char *)p)[i] != 0) { is_null = false; }
            }
        }
        if (is_null) {
            done = true;
        } else {
            if (!first) { str_append_char(s, ','); }
            first = false;
            encode_val(s, p, &em);
            p = (char *)p + size;
        }
    }
    str_append_char(s, ']');
}

static void encode_val(struct str *s, void *val, const struct type_info *m) {
    if (m->is_array) {
        encode_array(s, *(void **)val, m);
    } else {
        char buf[64];
        switch (m->kind) {
            case 's': {
                str_append_char(s, '"');
                str_append(s, *(const char **)val);
                str_append_char(s, '"');
                break;
            }
            case 'i': {
                if      (m->bytes == 1) {
                    snprintf(buf, sizeof(buf), "%d", *(int8_t *)val);
                } else if (m->bytes == 2) {
                    snprintf(buf, sizeof(buf), "%d", *(int16_t *)val);
                } else if (m->bytes == 4) {
                    snprintf(buf, sizeof(buf), "%d", *(int32_t *)val);
                } else {
                    snprintf(buf, sizeof(buf), "%lld", *(long long *)val);
                }
                str_append(s, buf);
                break;
            }
            case 'd': {
                if (m->bytes == sizeof(float)) {
                    snprintf(buf, sizeof(buf), "%g", *(float *)val);
                } else {
                    snprintf(buf, sizeof(buf), "%g", *(double *)val);
                }
                str_append(s, buf);
                break;
            }
            case 'b': {
                str_append(s, *(bool *)val ? "true" : "false");
                break;
            }
            case '{': {
                encode_obj(s, val, m->meta);
                break;
            }
        }
    }
}

char *encode(void *ptr, void *pointer_to_rtti) {
    char *r = NULL;
    if (ptr && pointer_to_rtti) {
        struct str s;
        str_init(&s);
        encode_obj(&s, ptr, (const struct type_info *)pointer_to_rtti);
        r = s.data;
    }
    return r;
}
