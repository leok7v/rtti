#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 4 * 1024
#define MAX_STRUCTS 1024
#define MAX_FIELDS 128

struct field_info {
  char type_name[64];
  char field_name[64];
  bool is_pointer;
  bool is_struct;
};

struct struct_info {
  char name[64];
  struct field_info fields[MAX_FIELDS];
  int field_count;
};

struct struct_info structs[MAX_STRUCTS];
int struct_count = 0;

void trim(char *str) {
  char *p = str;
  int l = (int)strlen(p);
  while (l > 0 && isspace(p[l - 1])) {
    p[--l] = 0;
  }
  while (*p && isspace(*p)) {
    ++p;
    --l;
  }
  memmove(str, p, (size_t)(l + 1));
}

const char *map_type_to_kind(const char *t, bool is_st, bool is_ptr) {
  const char *result = "'i'";
  if (is_st) {
    result = "'{'";
  } else if (strcmp(t, "char") == 0 && is_ptr) {
    result = "'s'";
  } else if (strcmp(t, "double") == 0 || strcmp(t, "float") == 0) {
    result = "'d'";
  } else if (strcmp(t, "bool") == 0 || strcmp(t, "_Bool") == 0) {
    result = "'b'";
  }
  return result;
}

void process_field_line(char *line) {
  char *semi = strchr(line, ';');
  if (semi) {
    *semi = '\0';
  }
  struct field_info *f =
      &structs[struct_count].fields[structs[struct_count].field_count];
  f->is_pointer = false;
  f->is_struct = false;
  char *p = line;
  if (strncmp(p, "const ", 6) == 0) {
    p += 6;
  }
  if (strncmp(p, "struct ", 7) == 0) {
    f->is_struct = true;
    p += 7;
  }
  char *star = strchr(p, '*');
  if (star) {
    f->is_pointer = true;
    char *type_end = star;
    while (type_end > p && isspace(*(type_end - 1))) {
      type_end--;
    }
    size_t t_len = (size_t)(type_end - p);
    if (t_len >= 64) {
      t_len = 63;
    }
    strncpy(f->type_name, p, t_len);
    f->type_name[t_len] = '\0';
    char *name_start = star + 1;
    while (isspace(*name_start)) {
      name_start++;
    }
    strcpy(f->field_name, name_start);
  } else {
    char *last_space = strrchr(p, ' ');
    if (last_space) {
      size_t t_len = (size_t)(last_space - p);
      if (t_len >= 64) {
        t_len = 63;
      }
      strncpy(f->type_name, p, t_len);
      f->type_name[t_len] = '\0';
      char *name_start = last_space + 1;
      while (isspace(*name_start)) {
        name_start++;
      }
      strcpy(f->field_name, name_start);
    }
  }
  char *bracket = strchr(f->field_name, '[');
  if (bracket) {
    *bracket = '\0';
    f->is_pointer = true;
  }
  trim(f->type_name);
  trim(f->field_name);
  structs[struct_count].field_count++;
}

void parse_file(const char *filename) {
  FILE *f = fopen(filename, "r");
  if (f) {
    char line[MAX_LINE];
    bool in_struct = false;
    while (fgets(line, sizeof(line), f)) {
      char *c_start = strstr(line, "/*");
      if (c_start) {
        char *c_end = strstr(c_start, "*/");
        if (c_end) {
          memmove(c_start, c_end + 2, strlen(c_end + 2) + 1);
        } else {
          *c_start = '\0';
        }
      }
      trim(line);
      if (strlen(line) > 0 && strncmp(line, "//", 2) != 0) {
        if (!in_struct) {
          if (strncmp(line, "codable struct", 14) == 0) {
            char *p = line + 14;
            while (isspace(*p)) {
              p++;
            }
            char *end = p;
            while (*end && *end != ' ' && *end != '{') {
              end++;
            }
            if (struct_count < MAX_STRUCTS) {
              size_t t_len = (size_t)(end - p);
              if (t_len >= 64) {
                t_len = 63;
              }
              strncpy(structs[struct_count].name, p, t_len);
              structs[struct_count].name[t_len] = '\0';
              structs[struct_count].field_count = 0;
              if (strchr(line, '{')) {
                in_struct = true;
              }
            }
          } else if (strchr(line, '{') && struct_count > 0 &&
                     structs[struct_count - 1].field_count == 0) {
            in_struct = true;
          }
        } else {
          if (strchr(line, '}')) {
            in_struct = false;
            struct_count++;
          } else {
            process_field_line(line);
          }
        }
      }
    }
    fclose(f);
  }
}

void generate_meta_fields(struct struct_info *s) {
  for (int j = 0; j < s->field_count; j++) {
    struct field_info *f = &s->fields[j];
    const char *kind_str =
        map_type_to_kind(f->type_name, f->is_struct, f->is_pointer);
    int is_array = 0;
    if (f->is_pointer && strcmp(kind_str, "\"'s\"") != 0 && strcmp(kind_str, "'s'") != 0) {
      is_array = 1;
    }
    const char *child_meta = "NULL";
    static char cm[128];
    if (f->is_struct) {
      snprintf(cm, sizeof(cm), "%s_rtti", f->type_name);
      child_meta = cm;
    }
    char size_str[128];
    if (is_array) {
      if (f->is_struct) {
        snprintf(size_str, sizeof(size_str), "sizeof(struct %s)",
                 f->type_name);
      } else {
        snprintf(size_str, sizeof(size_str), "sizeof(%s)", f->type_name);
      }
    } else {
      snprintf(size_str, sizeof(size_str), "sizeof(((struct %s *)0)->%s)",
               s->name, f->field_name);
    }
    printf("  { \"%s\", offsetof(struct %s, %s),\n"
           "    %s,\n"
           "    %s, %d, %s },\n",
           f->field_name, s->name, f->field_name, size_str, kind_str, is_array,
           child_meta);
  }
}

void generate_meta(const char *header_filename) {
  (void)header_filename;
  printf("#include <stddef.h>\n\n");
  printf("#ifndef struct_type_info\n");
  printf("#define struct_type_info\n");
  printf("struct type_info {\n");
  printf("  const char *name;\n");
  printf("  size_t offset;\n");
  printf("  size_t bytes;\n");
  printf("  char kind;\n");
  printf("  int is_array;\n");
  printf("  const struct type_info *meta;\n");
  printf("};\n");
  printf("#endif\n\n");
  for (int i = 0; i < struct_count; i++) {
    printf("extern const struct type_info %s_rtti[];\n", structs[i].name);
  }
  printf("\n");
  for (int i = 0; i < struct_count; i++) {
    struct struct_info *s = &structs[i];
    printf("const struct type_info %s_rtti[] = {\n", s->name);
    generate_meta_fields(s);
    printf("  { NULL, 0, 0, 0, 0, NULL }\n");
    printf("};\n\n");
  }
}

int main(int argc, char *argv[]) {
  int result = 0;
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <header_file.h>\n", argv[0]);
    result = 1;
  } else {
    parse_file(argv[1]);
    generate_meta(argv[1]);
  }
  return result;
}
