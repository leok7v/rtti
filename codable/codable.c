#include "codable.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef struct_type_info
#define struct_type_info
struct type_info {
    const char *name;
    size_t offset;
    size_t bytes;
    const struct type_info *rtti;
    char kind;
    bool is_array;
};
#endif

static size_t rtti_struct_bytes(const struct type_info *m) {
    const struct type_info *f = m;
    while (f->name) { f++; }
    return f->bytes;
}

static char *skip_ws(char *p) {
    while (*p && isspace((unsigned char)*p)) { p++; }
    return p;
}

static char *skip_str(char *j) {
    if (*j == '"') {
        j++;
        while (*j && *j != '"') {
            j += (*j == '\\' && *(j + 1)) ? 2 : 1;
        }
        if (*j == '"') { j++; }
    }
    return skip_ws(j);
}

static char *skip_value(char *j) {
    int d = 0, in_str = 0;
    while (*j) {
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
    return j;
}

// measure pass: count array bytes needed

struct measure_ctx {
    size_t array_bytes;
};

static char *measure_val(char *j, const struct type_info *m,
                         struct measure_ctx *ctx);

static char *measure_obj(char *j, const struct type_info *m,
                         struct measure_ctx *ctx) {
    char *r = j;
    if (*j == '{') {
        j = skip_ws(j + 1);
        while (*j && *j != '}') {
            char *loop_start = j;
            if (*j == '"') {
                j++;
                const char *k = j;
                while (*j && *j != '"') {
                    j += (*j == '\\' && *(j + 1)) ? 2 : 1;
                }
                size_t klen = (size_t)(j - k);
                if (*j == '"') { j++; }
                j = skip_ws(j);
                if (*j == ':') { j = skip_ws(j + 1); }
                const struct type_info *f = m;
                int found = 0;
                while (f && f->name && !found) {
                    if (strlen(f->name) == klen &&
                        strncmp(f->name, k, klen) == 0)
                    {
                        j = measure_val(j, f, ctx);
                        found = 1;
                    }
                    f++;
                }
                if (!found) { j = skip_value(j); }
            }
            j = skip_ws(j);
            if (*j == ',') { j = skip_ws(j + 1); }
            if (j == loop_start) { j++; }
        }
        if (*j == '}') { j = skip_ws(j + 1); }
        r = j;
    }
    return r;
}

static char *measure_array(char *j, const struct type_info *m,
                           struct measure_ctx *ctx) {
    char *r = j;
    if (*j == '[') {
        j = skip_ws(j + 1);
        size_t n = 0;
        struct type_info em = *m;
        em.is_array = 0;
        while (*j && *j != ']') {
            char *prev = j;
            j = measure_val(j, &em, ctx);
            if (j == prev) { j++; } else { n++; }
            j = skip_ws(j);
            if (*j == ',') { j = skip_ws(j + 1); }
        }
        if (*j == ']') { j = skip_ws(j + 1); }
        ctx->array_bytes += (n + 1) * m->bytes;
        r = j;
    }
    return r;
}

static char *measure_val(char *j, const struct type_info *m,
                         struct measure_ctx *ctx) {
    char *prev = j;
    if (m->is_array && *j == '[') {
        return measure_array(j, m, ctx);
    }
    if (m->kind == '{' && *j == '{') {
        return measure_obj(j, m->rtti, ctx);
    }
    if (m->kind == 's' && *j == '"') {
        return skip_str(j);
    }
    j = skip_value(j);
    if (j == prev && *j) { j++; }
    return j;
}

// parse pass: fill single allocation

struct parse_ctx {
    char *bump;
    char *bump_end;
};

static char *parse_val(char *j, void *p, const struct type_info *m,
                       struct parse_ctx *ctx);

static char *parse_str(char *j, const char **output) {
    char *r = j;
    if (*j == '"') {
        j++;
        *output = j;
        while (*j && *j != '"') {
            j += (*j == '\\' && *(j + 1)) ? 2 : 1;
        }
        if (*j == '"') { *j = '\0'; j++; }
        r = skip_ws(j);
    }
    return r;
}

static char *parse_obj(char *j, void *p,
                       const struct type_info *m,
                       struct parse_ctx *ctx) {
    char *r = j;
    if (*j == '{') {
        j = skip_ws(j + 1);
        while (*j && *j != '}') {
            if (*j == '"') {
                const char *k = NULL;
                j = parse_str(j, &k);
                j = skip_ws(j);
                if (*j == ':') { j = skip_ws(j + 1); }
                const struct type_info *f = m;
                int found = 0;
                while (f && f->name && !found) {
                    if (strcmp(f->name, k) == 0) {
                        j = parse_val(j, (char *)p + f->offset,
                                      f, ctx);
                        found = 1;
                    }
                    f++;
                }
                if (!found) { j = skip_value(j); }
            }
            j = skip_ws(j);
            if (*j == ',') { j = skip_ws(j + 1); }
        }
        if (*j == '}') { j = skip_ws(j + 1); }
        r = j;
    }
    return r;
}

static char *parse_array(char *j, void **output,
                         const struct type_info *m,
                         struct parse_ctx *ctx) {
    char *r = j;
    if (*j == '[') {
        j = skip_ws(j + 1);
        size_t elem = m->bytes;
        char *scan = j;
        size_t n = 0;
        int depth = 0, in_str = 0;
        while (*scan && !(*scan == ']' && depth == 0 && !in_str)) {
            if (*scan == '"') {
                in_str = !in_str;
            } else if (*scan == '\\' && in_str) {
                scan++;
            } else if (!in_str) {
                if (*scan == '{' || *scan == '[') {
                    depth++;
                } else if (*scan == '}' || *scan == ']') {
                    depth--;
                } else if (*scan == ',' && depth == 0) {
                    n++;
                }
            }
            scan++;
        }
        if (n > 0 || (j != scan && *j != ']')) { n++; }
        size_t need = (n + 1) * elem;
        void *data = NULL;
        if (ctx->bump + need <= ctx->bump_end) {
            data = ctx->bump;
            memset(data, 0, need);
            ctx->bump += need;
        }
        if (data) {
            struct type_info em = *m;
            em.is_array = 0;
            size_t i = 0;
            while (*j && *j != ']' && i < n) {
                void *e = (char *)data + i * elem;
                char *prev = j;
                j = parse_val(j, e, &em, ctx);
                if (j == prev) { j++; } else { i++; }
                j = skip_ws(j);
                if (*j == ',') { j = skip_ws(j + 1); }
            }
            if (*j == ']') { j = skip_ws(j + 1); }
            *output = data;
        }
        r = j;
    }
    return r;
}

static char *parse_val(char *j, void *p,
                       const struct type_info *m,
                       struct parse_ctx *ctx) {
    char *r = j;
    if (j && *j) {
        if (m->is_array) {
            r = parse_array(j, (void **)p, m, ctx);
        } else {
            char *prev = j;
            switch (m->kind) {
                case 's': {
                    if (strncmp(j, "null", 4) == 0) {
                        *(const char **)p = NULL;
                        j = skip_ws(j + 4);
                    } else {
                        const char *s = NULL;
                        j = parse_str(j, &s);
                        *(const char **)p = s;
                    }
                    break;
                }
                case 'i': {
                    long long v = 0;
                    if (*j == '"') {
                        v = strtoll(j + 1, &j, 10);
                        while (*j && *j != '"') {
                            j += (*j == '\\' && *(j + 1)) ? 2 : 1;
                        }
                        if (*j == '"') { j++; }
                    } else {
                        v = strtoll(j, &j, 10);
                    }
                    if (m->bytes == 1) {
                        *(int8_t *)p = (int8_t)v;
                    } else if (m->bytes == 2) {
                        *(int16_t *)p = (int16_t)v;
                    } else if (m->bytes == 4) {
                        *(int32_t *)p = (int32_t)v;
                    } else {
                        *(int64_t *)p = (int64_t)v;
                    }
                    j = skip_ws(j);
                    break;
                }
                case 'd': {
                    double v = 0;
                    if (*j == '"') {
                        v = strtod(j + 1, &j);
                        while (*j && *j != '"') {
                            j += (*j == '\\' && *(j + 1)) ? 2 : 1;
                        }
                        if (*j == '"') { j++; }
                    } else {
                        v = strtod(j, &j);
                    }
                    if (m->bytes == sizeof(float)) {
                        *(float *)p = (float)v;
                    } else {
                        *(double *)p = v;
                    }
                    j = skip_ws(j);
                    break;
                }
                case 'b': {
                    if (strncmp(j, "true", 4) == 0) {
                        *(bool *)p = true;
                        j += 4;
                    } else if (strncmp(j, "false", 5) == 0) {
                        *(bool *)p = false;
                        j += 5;
                    }
                    j = skip_ws(j);
                    break;
                }
                case '{': {
                    j = parse_obj(j, p, m->rtti, ctx);
                    break;
                }
                default: break;
            }
            if (j == prev) {
                while (*j && *j != ',' && *j != '}' &&
                       *j != ']')
                {
                    j++;
                }
            }
            r = j;
        }
    }
    return r;
}

void *decode(void *rtti, const char *json) {
    void *r = NULL;
    const struct type_info *ti = rtti;
    if (ti && json) {
        size_t sb = rtti_struct_bytes(ti);
        size_t jlen = strlen(json);
        char *dup = strdup(json);
        if (dup) {
            struct measure_ctx mctx = {0};
            char *p = skip_ws(dup);
            if (*p == '{') { measure_obj(p, ti, &mctx); }
            free(dup);
            size_t total = sb + jlen + 1 + mctx.array_bytes;
            char *block = calloc(1, total);
            if (block) {
                char *jcopy = block + sb;
                memcpy(jcopy, json, jlen + 1);
                struct parse_ctx pctx;
                pctx.bump = block + sb + jlen + 1;
                pctx.bump_end = block + total;
                p = skip_ws(jcopy);
                if (*p == '{') {
                    parse_obj(p, block, ti, &pctx);
                }
                r = block;
            }
        }
    }
    return r;
}

struct str {
    char *data;
    size_t count;
    size_t capacity;
};

static bool str_alloc(struct str *s, size_t count) {
    s->count = 0;
    s->data = malloc(count);
    s->capacity = s->data ? (int)count : 0;
    return s->data != NULL;
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

static void encode_obj(struct str *s, void *p, const struct type_info *m) {
    str_append_char(s, '{');
    bool first = true;
    for (const struct type_info *f = m; f && f->name; f++) {
        void *fp = (char *)p + f->offset;
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

static void encode_array(struct str *s, void *a, const struct type_info *m) {
    str_append_char(s, '[');
    struct type_info em = *m;
    em.is_array = 0;
    size_t size = m->bytes;
    void *p = a;
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
                encode_obj(s, val, m->rtti);
                break;
            }
        }
    }
}

char *encode(void *p, void *rtti) {
    char *r = NULL;
    if (p && rtti) {
        struct str s = {0};
        str_alloc(&s, 256);
        encode_obj(&s, p, (const struct type_info *)rtti);
        r = s.data;
    }
    return r;
}
