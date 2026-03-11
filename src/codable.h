#ifndef CODABLE_H
#define CODABLE_H

#include <stddef.h>

enum kind {
  i8 = 1, u8, i16, u16, i32, u32, i64, u64, str, dbl, bln, srct
};

struct type_info {
  const char *name;
  size_t offset;
  size_t bytes;
  enum kind kind;
  int is_array;
  const struct type_info *meta;
};

char *encode(void *pointer_to_struct, void *pointer_to_meta);

void *decode(void *pointer_to_struct, void *pointer_to_meta, char *json);

#endif
