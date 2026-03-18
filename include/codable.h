#ifndef CODABLE_H
#define CODABLE_H

#include <stddef.h>


char *encode(void *pointer_to_struct, void *pointer_to_rtti);

void *decode(void *pointer_to_rtti, const char *json);

#endif
