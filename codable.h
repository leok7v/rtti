#ifndef CODABLE_H
#define CODABLE_H

#include <stddef.h>

#ifndef struct_type_info
#define struct_type_info
struct type_info { /* rtti - run time type information */
    const char *name;
    size_t offset;
    size_t bytes;
    char kind;
    int is_array;
    const struct type_info *meta;
};
#endif

char *encode(void *pointer_to_struct, void *pointer_to_rtti);

void *decode(void *pointer_to_struct, void *pointer_to_rtti, char *json);

#endif
