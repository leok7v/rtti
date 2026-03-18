#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void * oom(void * p) {
    if (!p) {
        perror("Out of Memory");
        abort();
    }
    return p;
}

struct field_info {
    char * type_name;
    char * field_name;
    bool is_pointer;
    bool is_struct;
};

struct struct_info {
    char * name;
    struct field_info * fields;
    int field_count;
    int field_capacity;
};

static struct struct_info * structs = NULL;
static int struct_count = 0;
static int struct_capacity = 0;

static void trim(char * str) {
    char * p = str;
    int l = (int)strlen(p);
    while (l > 0 && isspace(p[l - 1])) { p[--l] = 0; }
    while (*p && isspace(*p)) {
        ++p;
        --l;
    }
    memmove(str, p, (size_t)(l + 1));
}

static bool is_ident(const char * p, const char * id) {
    const size_t n = strlen(id);
    return strncmp(p, id, n) == 0 &&
           (p[n] == '\0' || p[n] == '*' || isspace(p[n]));
}

static const char * skip_white_space(const char * p) {
    while (isspace(*p)) { p++; }
    return p;
}

static bool if_ident_skip(char ** p, const char * id) {
    bool b = is_ident(*p, id);
    if (b) { *p = (char *)skip_white_space(*p + strlen(id)); }
    return b;
}

static char kind(const char * t, bool is_struct, bool is_pointer) {
    char r = 'i';
    if (is_struct) {
        r = '{';
    } else if (is_ident(t, "char") && is_pointer) {
        r = 's';
    } else if (is_ident(t, "double") || is_ident(t, "float")) {
        r = 'd';
    } else if (is_ident(t, "bool") || is_ident(t, "_Bool")) {
        r = 'b';
    }
    return r;
}

static void parse_field(char * line) {
    char * semi = strchr(line, ';');
    if (semi) { *semi = '\0'; }
    struct struct_info * si = &structs[struct_count];
    if (si->field_count >= si->field_capacity) {
        si->field_capacity = si->field_capacity == 0 ? 
                             16 : si->field_capacity * 2;
        size_t bytes = sizeof(struct field_info) * si->field_capacity;
        si->fields = oom(realloc(si->fields, bytes));
    }
    struct field_info * f = &si->fields[si->field_count];
    f->is_pointer = false;
    f->is_struct = false;
    char * p = line;
    (void)if_ident_skip(&p, "const");
    f->is_struct = if_ident_skip(&p, "struct");
    char * star = strchr(p, '*');
    if (star) {
        f->is_pointer = true;
        char * end = star;
        while (end > p && isspace(*(end - 1))) { end--; }
        f->type_name = strndup(p, (size_t)(end - p));
        f->field_name = strdup(skip_white_space(star + 1));
    } else {
        char * last = strrchr(p, ' ');
        if (last) {
            f->type_name = strndup(p, (size_t)(last - p));
            f->field_name = strdup(skip_white_space(last + 1));
        }
    }
    char * brk = strchr(f->field_name, '[');
    if (brk) {
        *brk = '\0';
        f->is_pointer = true;
    }
    trim(f->type_name);
    trim(f->field_name);
    si->field_count++;
}

static void parse_file(const char * filename) {
    FILE * f = fopen(filename, "r");
    if (f) {
        char line[32 * 1024];
        bool in_struct = false;
        while (fgets(line, sizeof(line), f)) {
            char * c_s = strstr(line, "/*");
            if (c_s) {
                char * c_e = strstr(c_s, "*/");
                if (c_e) {
                    memmove(c_s, c_e + 2, strlen(c_e + 2) + 1);
                } else {
                    *c_s = '\0';
                }
            }
            trim(line);
            if (strlen(line) > 0 && strncmp(line, "//", 2) != 0) {
                if (!in_struct) {
                    if (strncmp(line, "codable struct", 14) == 0) {
                        if (struct_count >= struct_capacity) {
                            struct_capacity = struct_capacity == 0 ? 
                                              16 : struct_capacity * 2;
                            size_t b = sizeof(struct struct_info) *
                                       struct_capacity;
                            structs = oom(realloc(structs, b));
                        }
                        char * p = (char *)skip_white_space(line + 14);
                        char * e = p;
                        while (*e && *e != ' ' && *e != '{') { e++; }
                        structs[struct_count].name = strndup(p, 
                                                     (size_t)(e - p));
                        structs[struct_count].field_count = 0;
                        structs[struct_count].field_capacity = 0;
                        structs[struct_count].fields = NULL;
                        in_struct = strchr(line, '{');
                    }
                } else {
                    if (strchr(line, '}')) {
                        in_struct = false;
                        struct_count++;
                    } else {
                        parse_field(line);
                    }
                }
            }
        }
        fclose(f);
    }
}

static void emit_fields(struct struct_info * s) {
    for (int j = 0; j < s->field_count; j++) {
        struct field_info * f = &s->fields[j];
        const char k = kind(f->type_name, f->is_struct, f->is_pointer);
        bool arr = f->is_pointer && k != 's';
        char * b;
        if (arr) {
            size_t n = strlen(f->type_name) + 32;
            b = oom(malloc(n));
            if (f->is_struct) {
                snprintf(b, n, "sizeof(struct %s)", f->type_name);
            } else {
                snprintf(b, n, "sizeof(%s)", f->type_name);
            }
        } else {
            size_t n = strlen(s->name) + strlen(f->field_name) + 64;
            b = oom(malloc(n));
            snprintf(b, n, "sizeof(((struct %s *)0)->%s)", s->name, 
                     f->field_name);
        }
        printf("  { \"%s\",\n"
               "    offsetof(struct %s, %s),\n"
               "    %s,\n"
               "    %s%s,\n"
               "    '%c', %s },\n",
               f->field_name, s->name, f->field_name, b,
               f->is_struct ? f->type_name : "NULL",
               f->is_struct ? "_rtti" : "",
               k, arr ? "true" : "false");
        free(b);
    }
}

static void emit_rtti(void) {
    printf("#include <stddef.h>\n");
    printf("#include <stdbool.h>\n\n");
    printf("#ifndef struct_rtti\n");
    printf("#define struct_rtti\n");
    printf("struct rtti {\n");
    printf("    const char * name;\n");
    printf("    size_t offset;\n");
    printf("    size_t bytes;\n");
    printf("    const struct rtti * rtti;\n");
    printf("    char kind;\n");
    printf("    bool is_array;\n");
    printf("};\n");
    printf("#endif\n\n");
    for (int i = 0; i < struct_count; i++) {
        printf("extern const struct rtti %s_rtti[];\n", structs[i].name);
    }
    printf("\n");
    for (int i = 0; i < struct_count; i++) {
        struct struct_info * s = &structs[i];
        printf("const struct rtti %s_rtti[] = {\n", s->name);
        emit_fields(s);
        printf("  { NULL, 0, sizeof(struct %s), NULL, (char)0, false }\n",
               s->name);
        printf("};\n\n");
    }
}

int main(int argc, char * argv[]) {
    int result = EXIT_SUCCESS;
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.h> [-o <out.h>]\n", argv[0]);
        result = EXIT_FAILURE;
    } else {
        parse_file(argv[1]);
        if (struct_count > 0) {
            if (argc == 4 && strcmp(argv[2], "-o") == 0) {
                if (!freopen(argv[3], "w", stdout)) { result = EXIT_FAILURE; }
            }
            if (result == EXIT_SUCCESS) { emit_rtti(); }
        }
    }
    return result;
}
