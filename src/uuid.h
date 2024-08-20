#include <uuid/uuid.h>

#ifndef UUID_H
#define UUID_H

#define BASE62    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define UUID_SIZE 13

void UUID ( char* out );

#endif