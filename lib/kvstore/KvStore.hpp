#ifndef KV_STORE_HPP
#define KV_STORE_HPP

#include <boost/pool/pool_alloc.hpp>
#include <tbb/concurrent_unordered_map.h>
#include <yaml-cpp/yaml.h>
#include "KvFilter.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

#ifdef _MSC_VER
#  include <intrin.h>
#  include <ppl.h>
#  define PARALLEL_FOR Concurrency::parallel_for
#else
#  include <immintrin.h>
#  include <parallel/algorithm>
#  define PARALLEL_FOR __gnu_parallel::for_each
#endif

#if defined( __AVX2__ )
#  define KVSTORE_USE_SIMD 1
#endif

#ifdef _WIN32
#  define KVSTORE_EXPORT __declspec ( dllexport )
#else
#  define KVSTORE_EXPORT
#endif

using namespace std;
using KvValue = variant<string, double, int64_t, float, bool>;

enum class KvType
{
  STRING,
  DOUBLE,
  INTEGER,
  FLOAT,
  BOOLEAN
};

struct FastStringHash
{
  size_t operator() ( string_view str ) const noexcept
  {
    static const size_t FNV_offset = 14695981039346656037ULL;
    static const size_t FNV_prime = 1099511628211ULL;
    size_t hash = FNV_offset;

    for ( char c : str )
    {
      hash ^= static_cast<size_t> ( c );
      hash *= FNV_prime;
    }

    return hash;
  }
};


class StringPool
{
private:
  unordered_set<string, FastStringHash> _pool;
  mutex _mutex;

public:
  string_view intern ( string_view str )
  {
    /* @MUTEX-LOCK */
    lock_guard<mutex> lock ( _mutex );
    return *_pool.insert ( string ( str ) ).first;
  }
};


template <typename T> class LockFreeCache
{
public:
  explicit LockFreeCache ( size_t c = 10000 ) : _capacity ( c )
  {
  }

  void insert ( string_view k, T v )
  {
    auto keyStr = string ( k );
    _cache.insert_or_assign ( keyStr, move ( v ) );

    if ( _cache.size () > _capacity )
    {
      auto victim = _cache.begin ();
      _cache.erase ( victim );
    }
  }

  optional<T> get ( string_view k ) const
  {
    auto it = _cache.find ( string ( k ) );

    if ( it != _cache.end () )
    {
      return it->second;
    }

    return nullopt;
  }

  void clear ()
  {
    _cache.clear ();
  }

  void setCapacity ( size_t c )
  {
    _capacity = c;
  }

private:
  tbb::concurrent_unordered_map<string, T, FastStringHash> _cache;
  atomic<size_t> _capacity;
};

/**
 * HASHMAP POOL
 */
template <typename K, typename V> using HashmapPool = unordered_map<K, V, FastStringHash, equal_to<K>, boost::fast_pool_allocator<pair<const K, V>>>;

class KVSTORE_EXPORT KvData
{
private:
  int _id;
  string _key;
  KvType _type;
  KvValue _value;
  int64_t _created;

public:
  KvData () = default;

  KvData ( int id, string_view key, KvType type, const KvValue& value ) : _id ( id ), _key ( key ), _type ( type ), _value ( value )
  {
    _created = chrono::system_clock::now ().time_since_epoch ().count ();
  }

  template <typename T> optional<T> get () const
  {
    try
    {
      return get<T> ( _value );
    }
    catch ( const bad_variant_access& )
    {
      return nullopt;
    }
  }

  optional<string> getString () const
  {
    return get<string> ();
  }

  optional<double> getDouble () const
  {
    return get<double> ();
  }

  optional<int64_t> getInt () const
  {
    return get<int64_t> ();
  }

  optional<float> getFloat () const
  {
    return get<float> ();
  }

  optional<bool> getBool () const
  {
    return get<bool> ();
  }

  int getId () const
  {
    return _id;
  }

  const string& getKey () const
  {
    return _key;
  }

  KvType getType () const
  {
    return _type;
  }

  const KvValue& getValue () const
  {
    return _value;
  }

  int64_t getCreated () const
  {
    return _created;
  }
};


class KVSTORE_EXPORT KvStore
{
public:
  explicit KvStore ( size_t size = 1024 )
  {
    _store.reserve ( size );
    _id_map.reserve ( size );
    _hot_cache.setCapacity ( size / 10 );
  }

  explicit KvStore ( const string& filename, size_t size = 1024 ) : KvStore ( size )
  {
    _filename = filename;
    load ( filename );
  }

  void push ( string_view k, const KvValue& v )
  {
    if ( k.empty () || k.length () > 255 )
    {
      return;
    }

    auto type = determineType ( v );
    int new_id = static_cast<int> ( _store.size () ) + 1;
    auto inter_key = _str_pool.intern ( k );
    auto data = make_shared<KvData> ( new_id, inter_key, type, v );

    /* @MUTEX-LOCK */
    unique_lock<shared_mutex> lock ( _mutex );

    _store[string ( inter_key )] = data;
    _id_map[new_id] = data;
    _hot_cache.insert ( inter_key, data );

    if ( _filter )
    {
      _filter->insert ( string ( inter_key ) );
    }
  }

  bool remove ( string_view k )
  {
    /* @MUTEX-LOCK */
    unique_lock<shared_mutex> lock ( _mutex );

    auto it = _store.find ( string ( k ) );
    if ( it == _store.end () )
    {
      return false;
    }

    if ( _filter && _filter->getType () == KvFilterType::BLOOM )
    {
      vector<pair<string, shared_ptr<KvData>>> temp;

      temp.reserve ( _store.size () - 1 );

      for ( const auto& [k, v] : _store )
      {
        if ( k != k )
        {
          temp.emplace_back ( k, v );
        }
      }

      clear ();

      _filter = FilterFactory::createFilter ( KvFilterType::BLOOM, temp.size () );

      for ( const auto& [k, v] : temp )
      {
        _store[k] = v;
        _id_map[v->getId ()] = v;
        _filter->insert ( k );
        _hot_cache.insert ( k, v );
      }

      return true;
    }
    else
    {
      auto id = it->second->getId ();

      _store.erase ( it );
      _id_map.erase ( id );
      _hot_cache.clear ();

      if ( _filter )
      {
        _filter->remove ( string ( k ) );
      }

      return true;
    }
  }

  bool hasKey ( string_view k ) const
  {
    /* @MUTEX-LOCK */
    shared_lock<shared_mutex> lock ( _mutex );

    if ( auto cache = _hot_cache.get ( k ) )
    {
      return true;
    }

    if ( _filter )
    {
      if ( !_filter->isContain ( string ( k ) ) )
      {
        return false;
      }
    }

    return _store.find ( string ( k ) ) != _store.end ();
  }

  bool hasId ( int id ) const
  {
    /* @MUTEX-LOCK */
    shared_lock<shared_mutex> lock ( _mutex );
    return _id_map.find ( id ) != _id_map.end ();
  }

  shared_ptr<KvData> id ( int id ) const
  {
    /* @MUTEX-LOCK */
    shared_lock<shared_mutex> lock ( _mutex );

    auto find = _id_map.find ( id );
    return ( find != _id_map.end () ) ? find->second : nullptr;
  }

  shared_ptr<KvData> key ( string_view k ) const
  {
    /* @MUTEX-LOCK */
    shared_lock<shared_mutex> lock ( _mutex );

    if ( auto cached = _hot_cache.get ( k ) )
    {
      return *cached;
    }

    auto find = _store.find ( string ( k ) );
    return ( find != _store.end () ) ? find->second : nullptr;
  }


  void flush ()
  {
    if ( _filename.empty () )
    {
      return;
    }
    flush ( _filename );
  }

  void flush ( const string& filename )
  {
    /* @MUTEX-LOCK */
    shared_lock<shared_mutex> lock ( _mutex );

    YAML::Node yml;
    yml["filter"] = filterToString ( _filter ? _filter->getType () : KvFilterType::DEFAULT );

    YAML::Node dataNode;
    dataNode.SetStyle ( YAML::EmitterStyle::Block );

    vector<pair<string, shared_ptr<KvData>>> items;
    items.reserve ( _store.size () );

    for ( const auto& [key, value] : _store )
    {
      items.emplace_back ( key, value );
    }

    PARALLEL_FOR ( items.begin (), items.end (), [] ( auto& item ) {} );
    sort ( items.begin (), items.end () );

    for ( const auto& [k, v] : items )
    {
      YAML::Node o;

      o["id"] = v->getId ();
      o["key"] = k;
      o["value"] = valueToYaml ( v->getValue () );
      o["type"] = typeToString ( v->getType () );
      o["created"] = v->getCreated ();

      dataNode.push_back ( o );
    }

    yml["data"] = dataNode;

    ofstream out ( filename );

    if ( out )
    {
      out << YAML::Dump ( yml );
    }
  }

  void load ( const string& filename )
  {
    /* @MUTEX-LOCK */
    unique_lock<shared_mutex> lock ( _mutex );

    _filename = filename;

    ifstream file ( filename );

    if ( !file.good () )
    {
      ofstream newFile ( filename );

      if ( newFile.good () )
      {
        YAML::Node yml;

        yml["filter"] = "default";
        yml["data"] = YAML::Node ( YAML::NodeType::Sequence );

        newFile << YAML::Dump ( yml );
      }
      return;
    }

    try
    {
      YAML::Node yml = YAML::LoadFile ( filename );

      if ( yml["filter"] )
      {
        auto filterType = stringToFilter ( yml["filter"].as<string> () );
        setFilter ( filterType );
      }

      if ( yml["data"] )
      {
        const auto& data = yml["data"];
        const size_t size = data.size ();
        const size_t threshold = 10000;

        if ( size > threshold )
        {
          vector<pair<string, KvValue>> temp;
          temp.reserve ( size );

          /* clang-format off */
          #pragma omp parallel for
          for ( size_t i = 0; i < size; ++i )
          {
            const auto& item = data[i];
            auto key = item["key"].as<string> ();
            auto type = stringToType ( item["type"].as<string> () );
            auto value = yamlToValue ( item["value"], type );

            #pragma omp critical
            temp.emplace_back ( key, value );
            /* clang-format on */
          }

          for ( const auto& [k, v] : temp )
          {
            push ( k, v );
          }
        }
        else
        {
          for ( const auto& item : data )
          {
            auto k = item["key"].as<string> ();
            auto type = stringToType ( item["type"].as<string> () );
            auto v = yamlToValue ( item["value"], type );

            push ( k, v );
          }
        }
      }
    }
    catch ( const YAML::Exception& e )
    {
      clear ();
    }
  }

  void close ()
  {
    flush ();
    clear ();
  }


  void clear ()
  {
    /* @MUTEX-LOCK */
    unique_lock<shared_mutex> lock ( _mutex );

    _store.clear ();
    _id_map.clear ();
    _store.rehash ( 0 );
    _id_map.rehash ( 0 );
    _hot_cache.clear ();

    if ( _filter )
    {
      _filter.reset ();
    }
  }

  void setFilter ( KvFilterType filter )
  {
    /* @MUTEX-LOCK */
    unique_lock<shared_mutex> lock ( _mutex );

    _filter = FilterFactory::createFilter ( filter, _store.size () );

    if ( _filter )
    {
      for ( const auto& [key, _] : _store )
      {
        _filter->insert ( key );
      }
    }
  }

  size_t size () const
  {
    shared_lock<shared_mutex> lock ( _mutex );
    return _store.size ();
  }

#ifdef KVSTORE_USE_SIMD
  template <typename T> vector<size_t> findValuesAVX ( const vector<T>& values, T target ) const
  {
    vector<size_t> results;

    const size_t size = values.size ();
    const size_t simdSize = size - ( size % 4 );

    if constexpr ( is_same_v<T, double> )
    {
      __m256d targetVec = _mm256_set1_pd ( target );

      for ( size_t i = 0; i < simdSize; i += 4 )
      {
        __m256d dataVec = _mm256_loadu_pd ( &values[i] );
        __m256d cmp = _mm256_cmp_pd ( dataVec, targetVec, _CMP_EQ_OQ );

        unsigned mask = _mm256_movemask_pd ( cmp );

        if ( mask )
        {
          for ( size_t j = 0; j < 4; ++j )
          {
            if ( mask & ( 1 << j ) )
            {
              results.push_back ( i + j );
            }
          }
        }
      }
    }

    for ( size_t i = simdSize; i < size; ++i )
    {
      if ( values[i] == target )
      {
        results.push_back ( i );
      }
    }

    return results;
  }
#endif

private:
  mutable shared_mutex _mutex;
  string _filename;
  unique_ptr<IKvFilter> _filter;
  HashmapPool<string, shared_ptr<KvData>> _store;
  HashmapPool<int, shared_ptr<KvData>> _id_map;
  StringPool _str_pool;
  LockFreeCache<shared_ptr<KvData>> _hot_cache;

  static KvType determineType ( const KvValue& value )
  {
    return visit (
        [] ( auto&& arg ) -> KvType
        {
          using T = decay_t<decltype ( arg )>;

          if constexpr ( is_same_v<T, double> )
          {
            return KvType::DOUBLE;
          }
          else if constexpr ( is_same_v<T, int64_t> )
          {
            return KvType::INTEGER;
          }
          else if constexpr ( is_same_v<T, float> )
          {
            return KvType::FLOAT;
          }
          else if constexpr ( is_same_v<T, bool> )
          {
            return KvType::BOOLEAN;
          }
          else
          {
            return KvType::STRING;
          }
        },
        value );
  }

  static string typeToString ( KvType type )
  {
    switch ( type )
    {
      case KvType::DOUBLE:
        return "DOUBLE";

      case KvType::INTEGER:
        return "INTEGER";

      case KvType::FLOAT:
        return "FLOAT";

      case KvType::BOOLEAN:
        return "BOOLEAN";

      default:
      case KvType::STRING:
        return "STRING";
    }
  }

  static string filterToString ( KvFilterType filter )
  {
    switch ( filter )
    {
      case KvFilterType::BLOOM:
        return "bloom";

      case KvFilterType::CUCKOO:
        return "cuckoo";

      default:
      case KvFilterType::DEFAULT:
        return "default";
    }
  }

  static KvFilterType stringToFilter ( const string& str )
  {
    if ( str == "bloom" )
    {
      return KvFilterType::BLOOM;
    }
    else if ( str == "cuckoo" )
    {
      return KvFilterType::CUCKOO;
    }
    else
    {
      return KvFilterType::DEFAULT;
    }
  }

  static KvType stringToType ( const string& str )
  {
    if ( str == "DOUBLE" )
    {
      return KvType::DOUBLE;
    }
    else if ( str == "INTEGER" )
    {
      return KvType::INTEGER;
    }
    else if ( str == "FLOAT" )
    {
      return KvType::FLOAT;
    }
    else if ( str == "BOOLEAN" )
    {
      return KvType::BOOLEAN;
    }
    else
    {
      return KvType::STRING;
    }
  }

  static YAML::Node valueToYaml ( const KvValue& value )
  {
    return visit ( [] ( auto&& arg ) -> YAML::Node { return YAML::Node ( arg ); }, value );
  }

  static KvValue yamlToValue ( const YAML::Node& node, KvType type )
  {
    switch ( type )
    {
      case KvType::DOUBLE:
        return node.as<double> ();

      case KvType::INTEGER:
        return node.as<int64_t> ();

      case KvType::FLOAT:
        return node.as<float> ();

      case KvType::BOOLEAN:
        return node.as<bool> ();
      default:

      case KvType::STRING:
        return node.as<string> ();
    }
  }
};

#endif