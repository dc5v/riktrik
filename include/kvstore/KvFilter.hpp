#pragma once`

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

using namespace std;

constexpr size_t CUCKOO_BUCKET_SIZE = 4;
constexpr size_t CUCKOO_FINGERPRINT_SIZE = 16;
constexpr uint32_t CUCKOO_MAX_KICKS = 500;

class KvFilter
{
public:
  virtual ~KvFilter () = default;

  virtual void insert ( const string& key ) = 0;
  virtual bool isContain ( const string& key ) const = 0;
  virtual bool remove ( const string& key ) = 0;
  virtual void clear () = 0;
  virtual size_t size () const = 0;

  static uint32_t hash ( const string& key, uint32_t seed = 0 )
  {
    return MurmurHash3::hash ( key, seed );
  }

protected:
  class MurmurHash3
  {
  private:
    static constexpr uint32_t rotl32 ( uint32_t x, int8_t r )
    {
      return ( x << r ) | ( x >> ( 32 - r ) );
    }

  public:
    static uint32_t hash ( const string& key, uint32_t seed = 0 )
    {
      const uint8_t* data = ( const uint8_t* )key.data ();
      const int len = static_cast<int> ( key.length () );
      const int nblocks = len / 4;
      uint32_t h1 = seed;
      const uint32_t c1 = 0xcc9e2d51;
      const uint32_t c2 = 0x1b873593;

      const uint32_t* blocks = ( const uint32_t* )( data + nblocks * 4 );
      for ( int i = -nblocks; i; i++ )
      {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = rotl32 ( k1, 15 );
        k1 *= c2;
        h1 ^= k1;
        h1 = rotl32 ( h1, 13 );
        h1 = h1 * 5 + 0xe6546b64;
      }

      const uint8_t* tail = ( const uint8_t* )( data + nblocks * 4 );
      uint32_t k1 = 0;
      switch ( len & 3 )
      {
        case 3:
          k1 ^= tail[2] << 16;
          [[fallthrough]];
        case 2:
          k1 ^= tail[1] << 8;
          [[fallthrough]];
        case 1:
          k1 ^= tail[0];
          k1 *= c1;
          k1 = rotl32 ( k1, 15 );
          k1 *= c2;
          h1 ^= k1;
      }

      h1 ^= len;
      h1 ^= h1 >> 16;
      h1 *= 0x85ebca6b;
      h1 ^= h1 >> 13;
      h1 *= 0xc2b2ae35;
      h1 ^= h1 >> 16;

      return h1;
    }
  };

  static size_t nextPow2 ( size_t x )
  {
    if ( x == 0 )
    {
      return 1;
    }
    else
    {
      --x;

      x |= x >> 1;
      x |= x >> 2;
      x |= x >> 4;
      x |= x >> 8;
      x |= x >> 16;

      if ( sizeof ( size_t ) == 8 )
      {
        x |= x >> 32;
      }

      return x + 1;
    }
  }
};

class BloomFilter : public KvFilter
{
public:
  BloomFilter ( size_t expectedElements, double falsePositiveRate = 0.01 ) : _hash_num ( calculateOptimalHashes ( falsePositiveRate ) ), _bits ( calculateOptimalSize ( expectedElements, falsePositiveRate ) ), _bit_arr ( ( _bits + 63 ) / 64 )
  {
    uniform_int_distribution<uint32_t> dis;
    random_device rd;
    mt19937 gen ( rd () );

    for ( size_t i = 0; i < _hash_num; ++i )
    {
      _seeds.push_back ( dis ( gen ) );
    }
  }

  void insert ( const string& key ) override
  {
    for ( uint32_t seed : _seeds )
    {
      size_t hash = MurmurHash3::hash ( key, seed ) % _bits;
      _bit_arr[hash / 64] |= 1ULL << ( hash % 64 );
    }
  }

  bool isContain ( const string& key ) const override
  {
    for ( uint32_t seed : _seeds )
    {
      size_t hash = MurmurHash3::hash ( key, seed ) % _bits;
      if ( !( _bit_arr[hash / 64] & ( 1ULL << ( hash % 64 ) ) ) )
      {
        return false;
      }
    }

    return true;
  }

  bool remove ( const string& key ) override
  {
    return false;
  }

  void clear () override
  {
    fill ( _bit_arr.begin (), _bit_arr.end (), 0 );
  }

  size_t size () const override
  {
    return _bits;
  }

private:
  static size_t calculateOptimalSize ( size_t n, double p )
  {
    return static_cast<size_t> ( -double ( n ) * log ( p ) / ( log ( 2 ) * log ( 2 ) ) );
  }

  static size_t calculateOptimalHashes ( double p )
  {
    return static_cast<size_t> ( max ( 1.0, -log ( p ) / log ( 2 ) ) );
  }

  const size_t _hash_num;
  const size_t _bits;
  vector<uint64_t> _bit_arr;
  vector<uint32_t> _seeds;
};

class CuckooFilter : public KvFilter
{
private:
  struct Bucket
  {
    array<uint16_t, CUCKOO_BUCKET_SIZE> items{};
    array<bool, CUCKOO_BUCKET_SIZE> occupied{};

    uint16_t& operator[] ( size_t index )
    {
      return items[index];
    }

    const uint16_t& operator[] ( size_t index ) const
    {
      return items[index];
    }
  };

  vector<Bucket> _buckets;
  const size_t _buckets_num;

  static uint16_t generateFingerprint ( const string& key )
  {
    uint32_t hash = MurmurHash3::hash ( key );
    uint16_t fingerprint = ( hash & ( ( 1ULL << CUCKOO_FINGERPRINT_SIZE ) - 1 ) ) + 1;

    return fingerprint;
  }

  size_t altIndex ( size_t index, uint16_t fingerprint ) const
  {
    size_t hash = MurmurHash3::hash ( to_string ( fingerprint ) );
    return ( index ^ hash ) % _buckets_num;
  }

  bool insertIntoBucket ( size_t index, uint16_t fingerprint )
  {
    Bucket& bucket = _buckets[index];

    for ( size_t i = 0; i < CUCKOO_BUCKET_SIZE; i++ )
    {
      if ( !bucket.occupied[i] )
      {
        bucket.items[i] = fingerprint;
        bucket.occupied[i] = true;
        return true;
      }
    }

    return false;
  }

  bool containsInBucket ( size_t index, uint16_t fingerprint ) const
  {
    const Bucket& bucket = _buckets[index];

    for ( size_t i = 0; i < CUCKOO_BUCKET_SIZE; i++ )
    {
      if ( bucket.occupied[i] && bucket.items[i] == fingerprint )
      {
        return true;
      }
    }

    return false;
  }

  bool removeFromBucket ( size_t index, uint16_t fingerprint )
  {
    Bucket& bucket = _buckets[index];

    for ( size_t i = 0; i < CUCKOO_BUCKET_SIZE; i++ )
    {
      if ( bucket.occupied[i] && bucket.items[i] == fingerprint )
      {
        bucket.occupied[i] = false;
        return true;
      }
    }

    return false;
  }

public:
  explicit CuckooFilter ( size_t expectedElements ) : _buckets_num ( nextPow2 ( expectedElements / CUCKOO_BUCKET_SIZE * 1.05 ) )
  {
    _buckets.resize ( _buckets_num );
  }

  void insert ( const string& key ) override
  {
    uint16_t fingerprint = generateFingerprint ( key );
    size_t i1 = MurmurHash3::hash ( key ) % _buckets_num;
    size_t i2 = altIndex ( i1, fingerprint );

    if ( insertIntoBucket ( i1, fingerprint ) || insertIntoBucket ( i2, fingerprint ) )
    {
      return;
    }

    size_t curIndex = i1;

    for ( uint32_t count = 0; count < CUCKOO_MAX_KICKS; count++ )
    {
      size_t slot = rand () % CUCKOO_BUCKET_SIZE;
      uint16_t oldFingerprint = _buckets[curIndex][slot];

      _buckets[curIndex][slot] = fingerprint;
      fingerprint = oldFingerprint;
      curIndex = altIndex ( curIndex, fingerprint );

      if ( insertIntoBucket ( curIndex, fingerprint ) )
      {
        return;
      }
    }

    throw runtime_error ( "Cuckoo filter is full" );
  }

  bool isContain ( const string& key ) const override
  {
    uint16_t fingerprint = generateFingerprint ( key );
    size_t i1 = MurmurHash3::hash ( key ) % _buckets_num;
    size_t i2 = altIndex ( i1, fingerprint );

    return containsInBucket ( i1, fingerprint ) || containsInBucket ( i2, fingerprint );
  }

  bool remove ( const string& key ) override
  {
    uint16_t fingerprint = generateFingerprint ( key );
    size_t i1 = MurmurHash3::hash ( key ) % _buckets_num;
    size_t i2 = altIndex ( i1, fingerprint );

    return removeFromBucket ( i1, fingerprint ) || removeFromBucket ( i2, fingerprint );
  }

  void clear () override
  {
    for ( auto& bucket : _buckets )
    {
      bucket.occupied.fill ( false );
    }
  }

  size_t size () const override
  {
    return _buckets_num * CUCKOO_BUCKET_SIZE;
  }
};

class FilterFactory
{
public:
  enum class FilterType
  {
    BLOOM, CUCKOO
  };

  static unique_ptr<KvFilter> createFilter ( FilterType type, size_t expectedElements, double falsePositiveRate = 0.01 )
  {
    switch ( type )
    {
      case FilterType::BLOOM:
        return make_unique<BloomFilter> ( expectedElements, falsePositiveRate );
      case FilterType::CUCKOO:
        return make_unique<CuckooFilter> ( expectedElements );
      default:
        throw invalid_argument ( "Unknown filter type" );
    }
  }
};

using KvFilter = FilterFactory::FilterType;
