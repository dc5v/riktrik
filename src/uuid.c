#include "uuid.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <uuid/uuid.h>

void UUID ( char *out )
{
  uuid_t uuid;
  uuid_generate ( uuid );

  uint64_t high = ( ( uint64_t )uuid[0] << 56 ) | ( ( uint64_t )uuid[1] << 48 ) | ( ( uint64_t )uuid[2] << 40 ) | ( ( uint64_t )uuid[3] << 32 ) | ( ( uint64_t )uuid[4] << 24 ) | ( ( uint64_t )uuid[5] << 16 ) | ( ( uint64_t )uuid[6] << 8 ) | ( uint64_t )uuid[7];
  uint64_t low  = ( ( uint64_t )uuid[8] << 56 ) | ( ( uint64_t )uuid[9] << 48 ) | ( ( uint64_t )uuid[10] << 40 ) | ( ( uint64_t )uuid[11] << 32 ) | ( ( uint64_t )uuid[12] << 24 ) | ( ( uint64_t )uuid[13] << 16 ) | ( ( uint64_t )uuid[14] << 8 ) | ( uint64_t )uuid[15];
  uint8_t  idx  = UUID_SIZE - 1;

  out[idx--] = '\0';

  while ( idx >= 0 && ( high > 0 || low > 0 ) )
  {
    uint64_t remain;
    __asm__ ( "xor %%rdx, %%rdx\n\t"
              "divq %[base]\n\t"
              : "=a"( low ), "=d"( remain )
              : "a"( low ), "d"( high ), [base] "r"( ( uint64_t )62 )
              : "cc" );

    out[idx--] = BASE62[remain];
  }

  while ( idx >= 0 )
  {
    out[idx--] = '0';
  }
}