#ifndef CODABLE_H
#define CODABLE_H

#include <stddef.h>

/* encode a struct to json */
char * encode(void * pointer_to_struct, void * pointer_to_rtti);

/* decode a json to memory allocated pointer to struct */
void * decode(void * pointer_to_rtti, const char * json);

#endif
