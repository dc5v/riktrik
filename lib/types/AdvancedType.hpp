#ifndef ADVANCED_TYPE_HPP
#define ADVANCED_TYPE_HPP

#include <cstdint>
#include <cstring>
#include <iostream>

using namespace std;

class uint24_t
{
private:
  uint8_t _bytes[3];

public:
  uint24_t () : _bytes{ 0, 0, 0 }
  {
  }

  uint24_t ( uint32_t ui32 )
  {
    *this = ui32;
  }
  inline uint32_t to_uint32 () const
  {
    return to32 ( _bytes );
  }

  inline uint24_t& operator= ( uint32_t ui32 )
  {
    to24 ( ui32, _bytes );
    return *this;
  }

  bool operator== ( const uint24_t& ui24 ) const
  {
    return memcmp ( _bytes, ui24._bytes, 3 ) == 0;
  }

  friend ostream& operator<< ( ostream& out, const uint24_t& ui24 )
  {
    out << ui24.to_uint32 ();
    return out;
  }

private:
  static inline uint32_t to32 ( const uint8_t* bytes )
  {
    uint32_t result;

    asm volatile (

        // bytes[0] <- eax32
        "movzx %1, %%eax\n\t"

        // bytes[1] <- ecx32
        "movzx %2, %%ecx\n\t"

        // << 1 
        "shl $8, %%ecx\n\t"

        // eax | ecx
        "or %%ecx, %%eax\n\t"

        // bytes[2] <- ecx32
        "movzx %3, %%ecx\n\t"

        // << 2
        "shl $16, %%ecx\n\t"

        // eax | ecx
        "or %%ecx, %%eax\n\t"

        // MOV ecx
        "mov %%eax, %0\n\t"
        
        : "=r"( result ) : "m"( bytes[0] ), "m"( bytes[1] ), "m"( bytes[2] ) : "eax", "ecx" );


    return result;
  }

  static inline void to24 ( uint32_t value, uint8_t* bytes )
  {
    asm volatile (

        // value --> eax32
        "mov %1, %%eax\n\t"

        // MOV LSB
        "mov %%al, %0\n\t"

        // >> 1
        "shr $8, %%eax\n\t"

        // MOV
        "mov %%al, %1\n\t"

        // >> 1
        "shr $8, %%eax\n\t"

        // MOV msb
        "mov %%al, %2\n\t"

        : "=m"( bytes[0] ), "=m"( bytes[1] ), "=m"( bytes[2] )  : "r"( value ) : "eax" );
  }
};

#endif
