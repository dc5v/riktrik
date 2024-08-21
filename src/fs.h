#ifndef FS_H
#define FS_H

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define DATA_DIR  "data"
#define MODEL_DIR "models"
#define FILENAME_SIZE 1024

void fs_mkdir ( const char *directory );
void fs_filename ( char *type, char *tag, int64_t epoch, char *output_filename );
void fs_mkdir ( const char *directory );
void fs_save ( const char *filename, const void *data, size_t size );

#endif