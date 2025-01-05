#pragma once
#include "flat_hash_map.hpp"
#include "yaml-cpp/yaml.h"
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct AppendKeyData
{
  string type;
  string value;

  string getType () const
  {
    return type;
  }

  int getInt () const
  {
    return stoi ( value );
  }

  double getDouble () const
  {
    return stod ( value );
  }

  float getFloat () const
  {
    return stof ( value );
  }

  string getString () const
  {
    return value;
  }

  bool getBoolean () const
  {
    return value == "true";
  }

  void set ( const string& type, const string& newValue )
  {
    this->type  = type;
    this->value = newValue;
  }

  void remove ()
  {
    value.clear ();
  }

  bool exists () const
  {
    return !value.empty ();
  }
};

struct KeyData
{
  string id;
  string key;
  string value;
  double created_at;
  double updated_at;

  tsl::flat_hash_map<string, AppendKeyData> appends;

  KeyData ( const string& key = "", const string& value = "" );

  void                    append ( const string& key, const AppendKeyData& appendData );
  void                    removeAppend ( const string& key );
  bool                    existsAppend ( const string& key ) const;
  optional<AppendKeyData> getAppend ( const string& key ) const;
  vector<string>          appendKeys () const;

  string toYaml () const;
};

class KvData
{
private:
  vector<KeyData> data;

public:
  explicit KvData ( const vector<KeyData>& results ) : data ( results )
  {
  }
  size_t size () const
  {
    return data.size ();
  }
  KeyData index ( size_t idx ) const
  {
    return data.at ( idx );
  }
};

class KVStore
{
private:
  string                              filename;
  tsl::flat_hash_map<string, KeyData> index_key;
  tsl::flat_hash_map<string, string>  index_id;
  map<double, string>                 index_created;
  map<double, string>                 index_updated;
  mutex                               mutex_lock;

  queue<KeyData>     queue;
  condition_variable queue_condition;
  bool               thread_queue_stop = false;
  thread             thread_queue;

  void   queueWorker ();
  double getEpoch ( bool withMilliseconds = true ) const;

public:
  explicit KVStore ( const string& filename );
  ~KVStore ();

  void                    set ( const string& key, const string& value );
  string                  get ( const string& key ) const;
  string                  generateID ();
  void                    load ();
  void                    save ();
  void                    append ( const string& key, const AppendKeyData& appendData );
  void                    removeAppend ( const string& key );
  bool                    existsAppend ( const string& key ) const;
  optional<AppendKeyData> getAppend ( const string& key ) const;
  vector<string>          appendKeys () const;
  string                  toYaml () const;

  KvData find ( const string& pattern ) const;
  KvData findRegex ( const string& regexPattern ) const;
  KvData findCreatedDate ( double start, double end = 0 ) const;
  KvData findUpdatedDate ( double start, double end = 0 ) const;

  void clear ();
  void close ();
};
