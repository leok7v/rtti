#include "codable.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *skip_ws(char *p) {
  while (*p && isspace(*p)) {
    p++;
  }
  return p;
}

static char *parse_val(char *j, void *ptr, const struct type_info *meta);

static char *parse_str(char *j, const char **out) {
  char *res = j;
  if (*j == '"') {
    j++;
    *out = j;
    while (*j && *j != '"') {
      j += (*j == '\\' && *(j + 1)) ? 2 : 1;
    }
    if (*j == '"') {
      *j = '\0';
      j++;
    }
    res = skip_ws(j);
  }
  return res;
}

static char *parse_obj(char *j, void *ptr, const struct type_info *m) {
  char *res = j;
  if (*j == '{') {
    j = skip_ws(j + 1);
    while (*j && *j != '}') {
      if (*j == '"') {
        const char *k = NULL;
        j = parse_str(j, &k);
        j = skip_ws(j);
        if (*j == ':') {
          j = skip_ws(j + 1);
          const struct type_info *f = m;
          bool found = false;
          while (f && f->name) {
            if (strcmp(f->name, k) == 0) {
              found = true;
              void *f_ptr = (char *)ptr + f->offset;
              j = parse_val(j, f_ptr, f);
              break;
            }
            f++;
          }
          if (!found) {
            int d = 0;
            bool in_str = false;
            int iters = 0;
            while (*j && iters < 1000) {
              iters++;
              if (*j == '"') {
                in_str = !in_str;
              } else if (*j == '\\' && in_str) {
                j++;
              } else if (!in_str) {
                if (*j == '{' || *j == '[') {
                  d++;
                } else if (*j == '}' || *j == ']') {
                  d--;
                  if (d < 0) {
                    break;
                  }
                } else if ((*j == ',' || *j == '}') && d == 0) {
                  break;
                }
              }
              j++;
            }
            if (iters >= 1000) {
              fprintf(stderr, "LOOP CAUGHT AT '%10s...'\n", j);
            }
          }
        }
      }
      j = skip_ws(j);
      if (*j == ',') {
        j = skip_ws(j + 1);
      }
    }
    if (*j == '}') {
      j = skip_ws(j + 1);
    }
    res = j;
  }
  return res;
}

static char *parse_array(char *j, void **out, const struct type_info *m) {
  char *res = j;
  if (*j == '[') {
    j = skip_ws(j + 1);
    size_t cap = 4;
    size_t cnt = 0;
    size_t sz = m->bytes;
    void *data = calloc(cap + 1, sz);
    if (data) {
      struct type_info in_m = *m;
      in_m.is_array = 0;
      while (*j && *j != ']') {
        if (cnt >= cap) {
          cap *= 2;
          void *new_d = realloc(data, (cap + 1) * sz);
          if (!new_d) {
            free(data);
            data = NULL;
            break;
          }
          data = new_d;
          memset((char *)data + cnt * sz, 0, (cap + 1 - cnt) * sz);
        }
        void *elem = (char *)data + (cnt * sz);
        char *old_j = j;
        j = parse_val(j, elem, &in_m);
        if (j == old_j) {
          j++;
        } else {
          cnt++;
        }
        j = skip_ws(j);
        if (*j == ',') {
          j = skip_ws(j + 1);
        }
      }
      if (data) {
        if (*j == ']') {
          j = skip_ws(j + 1);
        }
        void *fin = realloc(data, (cnt + 1) * sz);
        if (fin) {
          data = fin;
        }
        memset((char *)data + (cnt * sz), 0, sz);
        *out = data;
        res = j;
      }
    }
  }
  return res;
}

static char *parse_val(char *j, void *ptr, const struct type_info *m) {
  char *res = j;
  if (j && *j) {
    if (m->is_array) {
      res = parse_array(j, (void **)ptr, m);
    } else {
      char *old_j = j;
      switch (m->kind) {
      case 's': {
        if (strncmp(j, "null", 4) == 0) {
          *(const char **)ptr = NULL;
          j += 4;
          j = skip_ws(j);
        } else {
          const char *s = NULL;
          j = parse_str(j, &s);
          *(const char **)ptr = s;
        }
        break;
      }
      case 'i': {
        long long val = strtoll(j, &j, 10);
        if (m->bytes == 1) {
          *(int8_t *)ptr = (int8_t)val;
        } else if (m->bytes == 2) {
          *(int16_t *)ptr = (int16_t)val;
        } else if (m->bytes == 4) {
          *(int32_t *)ptr = (int32_t)val;
        } else if (m->bytes == 8) {
          *(int64_t *)ptr = (int64_t)val;
        }
        j = skip_ws(j);
        break;
      }
      case 'd': {
        double val = strtod(j, &j);
        if (m->bytes == sizeof(float)) {
          *(float *)ptr = (float)val;
        } else {
          *(double *)ptr = val;
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
      if (j == old_j) {
        while (*j && *j != ',' && *j != '}' && *j != ']') {
          j++;
        }
      }
      res = j;
    }
  }
  return res;
}

void *decode(void *ptr, void *pointer_to_rtti, char *json) {
  void *res = NULL;
  if (ptr && pointer_to_rtti && json) {
    char *copy = strdup(json);
    if (copy) {
      char *p = skip_ws(copy);
      if (*p == '{') {
        parse_obj(p, ptr, (const struct type_info *)pointer_to_rtti);
      }
      res = copy;
    }
  }
  return res;
}

struct sb {
  char *str;
  size_t len;
  size_t cap;
};

static void sb_init(struct sb *b) {
  b->cap = 256;
  b->len = 0;
  b->str = malloc(b->cap);
  if (b->str) {
    b->str[0] = '\0';
  }
}

static void sb_append(struct sb *b, const char *s) {
  if (b->str && s) {
    size_t slen = strlen(s);
    if (b->len + slen + 1 > b->cap) {
      b->cap = (b->cap + slen + 1) * 2;
      b->str = realloc(b->str, b->cap);
    }
    if (b->str) {
      strcpy(b->str + b->len, s);
      b->len += slen;
    }
  }
}

static void sb_append_char(struct sb *b, char c) {
  char s[2] = {c, '\0'};
  sb_append(b, s);
}

static void encode_val(struct sb *b, void *val, const struct type_info *m);

static void encode_obj(struct sb *b, void *ptr, const struct type_info *m) {
  sb_append_char(b, '{');
  bool first = true;
  for (const struct type_info *f = m; f && f->name; f++) {
    void *f_ptr = (char *)ptr + f->offset;
    if (!((f->kind == 's' || f->is_array) && *(void **)f_ptr == NULL)) {
      if (!first) {
        sb_append_char(b, ',');
      }
      first = false;
      sb_append_char(b, '"');
      sb_append(b, f->name);
      sb_append(b, "\":");
      encode_val(b, f_ptr, f);
    }
  }
  sb_append_char(b, '}');
}

static void encode_array(struct sb *b, void *arr, const struct type_info *m) {
  sb_append_char(b, '[');
  struct type_info in_m = *m;
  in_m.is_array = 0;
  size_t sz = m->bytes;
  void *ptr = arr;
  bool first = true;
  while (1) {
    bool is_null = true;
    if (m->kind == 's') {
      if (*(char **)ptr != NULL) {
        is_null = false;
      }
    } else {
      for (size_t i = 0; i < sz; i++) {
        if (((char *)ptr)[i] != 0) {
          is_null = false;
          break;
        }
      }
    }
    if (is_null) {
      break;
    }
    if (!first) {
      sb_append_char(b, ',');
    }
    first = false;
    encode_val(b, ptr, &in_m);
    ptr = (char *)ptr + sz;
  }
  sb_append_char(b, ']');
}

static void encode_val(struct sb *b, void *val, const struct type_info *m) {
  if (m->is_array) {
    encode_array(b, *(void **)val, m);
  } else {
    char buf[64];
    switch (m->kind) {
    case 's':
      sb_append_char(b, '"');
      sb_append(b, *(const char **)val);
      sb_append_char(b, '"');
      break;
    case 'i':
      if (m->bytes == 1) {
        snprintf(buf, sizeof(buf), "%d", *(int8_t *)val);
      } else if (m->bytes == 2) {
        snprintf(buf, sizeof(buf), "%d", *(int16_t *)val);
      } else if (m->bytes == 4) {
        snprintf(buf, sizeof(buf), "%d", *(int32_t *)val);
      } else if (m->bytes == 8) {
        snprintf(buf, sizeof(buf), "%lld", *(long long *)val);
      }
      sb_append(b, buf);
      break;
    case 'd':
      if (m->bytes == sizeof(float)) {
        snprintf(buf, sizeof(buf), "%g", *(float *)val);
      } else {
        snprintf(buf, sizeof(buf), "%g", *(double *)val);
      }
      sb_append(b, buf);
      break;
    case 'b':
      sb_append(b, *(bool *)val ? "true" : "false");
      break;
    case '{':
      encode_obj(b, val, m->meta);
      break;
    }
  }
}

char *encode(void *ptr, void *pointer_to_rtti) {
  char *res = NULL;
  if (ptr && pointer_to_rtti) {
    struct sb b;
    sb_init(&b);
    encode_obj(&b, ptr, (const struct type_info *)pointer_to_rtti);
    res = b.str;
  }
  return res;
}
