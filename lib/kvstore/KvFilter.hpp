#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

constexpr size_t CUCKOO_BUCKET_SIZE = 4;
constexpr size_t CUCKOO_FINGERPRINT_SIZE = 16;
constexpr uint32_t CUCKOO_MAX_KICKS = 500;

/**
 * MHasher (https://github.com/aappleby/smhasher)
 * MurmurHash3 and its variants: Analysis and Improvements" (Austin Appleby, 2011)
 */
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
    const int blocknum = len / 4;
    uint32_t h1 = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    const uint32_t* blocks = ( const uint32_t* )( data + blocknum * 4 );

    for ( int i = -blocknum; i; i++ )
    {
      uint32_t k1 = blocks[i];

      k1 *= c1;
      k1 = rotl32 ( k1, 15 );
      k1 *= c2;
      h1 ^= k1;
      h1 = rotl32 ( h1, 13 );
      h1 = h1 * 5 + 0xe6546b64;
    }

    const uint8_t* tail = ( const uint8_t* )( data + blocknum * 4 );
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

/**
 * FILTER
 */

enum class KvFilterType
{
  BLOOM,
  CUCKOO,
  DEFAULT
};

class IKvFilter
{
public:
  virtual ~IKvFilter () = default;
  IKvFilter () = default;


  virtual void insert ( const string& key ) = 0;
  virtual bool isContain ( const string& key ) const = 0;
  virtual bool remove ( const string& key ) = 0;
  virtual void clear () = 0;
  virtual size_t size () const = 0;
  virtual KvFilterType getType () const = 0;

  static uint32_t hash ( const string& key, uint32_t seed = 0 )
  {
    return MurmurHash3::hash ( key, seed );
  }

  static size_t nPow ( size_t x )
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

/**
 * BLOOM FILTER
 *
 * Space/Time Trade-offs in Hash Coding with Allowable Errors
 * - Cassandra
 * - Redis
 * - Guava
 */
class BloomFilter : public IKvFilter
{
private:
  const size_t _hashed_num;
  const size_t _bits_num;
  vector<uint64_t> _bit_arr;
  vector<uint32_t> _seeds;

public:
  BloomFilter ( size_t expectedElements, double falsePositiveRate = 0.01 ) : _hashed_num ( genHash ( falsePositiveRate ) ), _bits_num ( genSize ( expectedElements, falsePositiveRate ) ), _bit_arr ( ( _bits_num + 63 ) / 64 )
  {
    auto now = chrono::high_resolution_clock::now ();
    auto seed = static_cast<unsigned int> ( now.time_since_epoch ().count () );

    uniform_int_distribution<uint32_t> distribution;
    random_device hwRand;
    mt19937 generator ( hwRand () );

    generator.seed ( generator () ^ seed );
    _seeds.reserve ( _hashed_num );

    for ( size_t i = 0; i < _hashed_num; ++i )
    {
      _seeds.push_back ( distribution ( generator ) );
    }
  }

  void insert ( const string& key ) override
  {
    for ( uint32_t s : _seeds )
    {
      size_t hash = MurmurHash3::hash ( key, s ) % _bits_num;
      _bit_arr[hash / 64] |= 1ULL << ( hash % 64 );
    }
  }

  bool isContain ( const string& key ) const override
  {
    for ( uint32_t s : _seeds )
    {
      size_t hash = MurmurHash3::hash ( key, s ) % _bits_num;

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
    return _bits_num;
  }

  KvFilterType getType () const override
  {
    return KvFilterType::DEFAULT;
  }

private:
  static size_t genSize ( size_t n, double p )
  {
    return static_cast<size_t> ( -double ( n ) * log ( p ) / ( log ( 2 ) * log ( 2 ) ) );
  }

  static size_t genHash ( double p )
  {
    return static_cast<size_t> ( max ( 1.0, -log ( p ) / log ( 2 ) ) );
  }
};

/**
 * CUCKOO FILTER
 *
 * https://github.com/efficient/cuckoofilter
 * - Impala
 * - RocksDB
 * - ScyllaDB
 */
class CuckooFilter : public IKvFilter
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

public:
  explicit CuckooFilter ( size_t expectedElements ) : _buckets_num ( nPow ( expectedElements / CUCKOO_BUCKET_SIZE * 1.05 ) )
  {
    _buckets.resize ( _buckets_num );
  }

  void insert ( const string& key ) override
  {
    uint16_t fp = genFingerprint ( key );
    size_t i1 = MurmurHash3::hash ( key ) % _buckets_num;
    size_t i2 = altIndex ( i1, fp );

    if ( insertBucket ( i1, fp ) || insertBucket ( i2, fp ) )
    {
      return;
    }

    size_t idx = i1;
    for ( uint32_t i = 0; i < CUCKOO_MAX_KICKS; i++ )
    {
      size_t slot = rand () % CUCKOO_BUCKET_SIZE;
      uint16_t old_pf = _buckets[idx][slot];

      _buckets[idx][slot] = fp;
      fp = old_pf;
      idx = altIndex ( idx, fp );

      if ( insertBucket ( idx, fp ) )
      {
        return;
      }
    }

    throw runtime_error ( "Cuckoo filter is full" );
  }

  bool isContain ( const string& key ) const override
  {
    uint16_t fp = genFingerprint ( key );
    size_t i1 = MurmurHash3::hash ( key ) % _buckets_num;
    size_t i2 = altIndex ( i1, fp );

    return isContainsBucket ( i1, fp ) || isContainsBucket ( i2, fp );
  }

  bool remove ( const string& key ) override
  {
    uint16_t fp = genFingerprint ( key );
    size_t i1 = MurmurHash3::hash ( key ) % _buckets_num;
    size_t i2 = altIndex ( i1, fp );

    return removeBucket ( i1, fp ) || removeBucket ( i2, fp );
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

  KvFilterType getType () const override
  {
    return KvFilterType::CUCKOO;
  }

private:
  static uint16_t genFingerprint ( const string& key )
  {
    uint32_t hash = MurmurHash3::hash ( key );
    return ( ( hash & ( ( 1ULL << CUCKOO_FINGERPRINT_SIZE ) - 1 ) ) + 1 );
  }

  size_t altIndex ( size_t index, uint16_t fp ) const
  {
    size_t hash = MurmurHash3::hash ( to_string ( fp ) );
    return ( index ^ hash ) % _buckets_num;
  }

  bool insertBucket ( size_t idx, uint16_t fp )
  {
    Bucket& b = _buckets[idx];

    for ( size_t i = 0; i < CUCKOO_BUCKET_SIZE; i++ )
    {
      if ( !b.occupied[i] )
      {
        b.items[i] = fp;
        b.occupied[i] = true;

        return true;
      }
    }

    return false;
  }

  bool isContainsBucket ( size_t idx, uint16_t fp ) const
  {
    const Bucket& b = _buckets[idx];

    for ( size_t i = 0; i < CUCKOO_BUCKET_SIZE; i++ )
    {
      if ( b.occupied[i] && b.items[i] == fp )
      {
        return true;
      }
    }

    return false;
  }

  bool removeBucket ( size_t idx, uint16_t fp )
  {
    Bucket& b = _buckets[idx];

    for ( size_t i = 0; i < CUCKOO_BUCKET_SIZE; i++ )
    {
      if ( b.occupied[i] && b.items[i] == fp )
      {
        b.occupied[i] = false;
        return true;
      }
    }

    return false;
  }
};

class FilterFactory
{
public:
  static unique_ptr<IKvFilter> createFilter ( KvFilterType type, size_t elements, double falseRate = 0.01 )
  {
    switch ( type )
    {
      case KvFilterType::BLOOM:
        return make_unique<BloomFilter> ( elements, falseRate );

      case KvFilterType::CUCKOO:
        return make_unique<CuckooFilter> ( elements );

      default:
      case KvFilterType::DEFAULT:
        return nullptr;
    }
  }
};
