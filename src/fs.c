#include "fs.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

void fs_mkdir ( const char *directory )
{
  struct stat st = { 0 };

  if ( stat ( directory, &st ) == -1 && mkdir ( directory, 0700 ) != 0 )
  {
    error_log ( "fs_mkdir - failed to create directory." );
  }
}

void fs_filename ( char *type, char *tag, int64_t epoch, char *output_filename )
{
  fs_mkdir ( DATA_DIR );

  if ( strcmp ( type, "uid" ) == 0 )
  {
    snprintf ( output_filename, FILENAME_SIZE, "%s/index.dat", DATA_DIR );
  }
  else if ( strcmp ( type, "tag" ) == 0 )
  {
    char      datetime[9];
    time_t    t  = epoch / 1000;
    struct tm tm = *localtime ( &t );

    strftime ( datetime, sizeof ( datetime ), "%Y%m%d", &tm );
    snprintf ( output_filename, FILENAME_SIZE, "%s/%s-%s.db", DATA_DIR, tag, datetime );
  }
}

void fs_save ( const char *filename, const void *data, size_t size )
{
  FILE *file = fopen ( filename, "ab" );

  if ( !file )
  {
    error_log ( "fs_save - failed to write file." );
  }

  fwrite ( data, size, 1, file );
}
