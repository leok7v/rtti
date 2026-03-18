#ifndef rtti_h
#define rtti_h
#include <stddef.h>  // size_t
#include <stdbool.h> // bool

#ifndef struct_rtti
#define struct_rtti

struct rtti {
    const char *name;
    size_t offset;
    size_t bytes;
    const struct type_info *rtti;
    char kind;
    bool is_array;
};

#endif

#endif 
