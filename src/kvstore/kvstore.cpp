#include "kvstore.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

struct KeyData
{
  KeyData ( const std::string& key = "", const std::string& value = "" ) : key ( key ), value ( value ), created_at ( 0 ), updated_at ( 0 )
  {
  }

  void append ( const std::string& key, const AppendKeyData& appendData )
  {
    appends[key] = appendData;
  }

  void removeAppend ( const std::string& key )
  {
    appends.erase ( key );
  }

  bool existsAppend ( const std::string& key ) const
  {
    return appends.find ( key ) != appends.end ();
  }

  std::optional<AppendKeyData> getAppend ( const std::string& key ) const
  {
    auto it = appends.find ( key );

    if ( it != appends.end () )
    {
      return it->second;
    }

    return std::nullopt;
  }

  std::vector<std::string> appendKeys () const
  {
    std::vector<std::string> keys;

    for ( const auto& [appendKey, _] : appends )
    {
      keys.push_back ( appendKey );
    }

    return keys;
  }

  std::string toYaml () const
  {
    YAML::Emitter yml;

    yml << YAML::BeginMap;
    yml << YAML::Key << "id" << YAML::Value << id;
    yml << YAML::Key << "key" << YAML::Value << key;
    yml << YAML::Key << "value" << YAML::Value << value;
    yml << YAML::Key << "created_at" << YAML::Value << created_at;
    yml << YAML::Key << "updated_at" << YAML::Value << updated_at;

    if ( !appends.empty () )
    {
      yml << YAML::Key << "appends" << YAML::Value << YAML::BeginMap;

      for ( const auto& [appendKey, appendData] : appends )
      {
        yml << YAML::Key << appendKey << YAML::Value << YAML::BeginMap;
        yml << YAML::Key << "type" << YAML::Value << appendData.getType ();
        yml << YAML::Key << "value" << YAML::Value << appendData.getString ();
        yml << YAML::EndMap;
      }

      yml << YAML::EndMap;
    }

    yml << YAML::EndMap;

    return std::string ( yml.c_str () );
  }
};

KVStore::KVStrore ( const string& filename ) : filename ( filename )
{
  load ();

  thread_queue_stop = false;
  thread_queue      = thread ( &KVStore::queueWorker, this );
}

KVStore::~KVStore ()
{
  {
    lock_guard<mutex> lock ( mutex_lock );
    thread_queue_stop = true;
    queue_condition.notify_all ();
  }

  if ( thread_queue.joinable () )
  {
    thread_queue.join ();
  }

  save ();
}

void KVStore::set ( const string& key, const string& value )
{
  lock_guard<mutex> lock ( mutex_lock );
  auto              now = getEpoch ();

  if ( index_key.find ( key ) != index_key.end () )
  {
    index_key[key].value      = value;
    index_key[key].updated_at = now;
  }
  else
  {
    KeyData newKey ( key, value );

    newKey.id         = generateID ();
    newKey.created_at = now;
    newKey.updated_at = now;

    index_key[key]                   = newKey;
    index_id[newKey.id]              = key;
    index_created[newKey.created_at] = key;
    index_updated[newKey.updated_at] = key;
  }

  queue.push ( index_key[key] );
  queue_condition.notify_one ();
}

string KVStore::get ( const string& key ) const
{
  lock_guard<mutex> lock ( mutex_lock );

  if ( index_key.find ( key ) == index_key.end () )
  {
    throw runtime_error ( "Key not found" );
  }

  return index_key.at ( key ).value;
}

string KVStore::generateID ()
{
  static int counter = 0;

  while ( true )
  {
    ostringstream oss;

    oss << hex << setw ( 6 ) << setfill ( '0' ) << ( counter++ );
    string newId = oss.str ();

    if ( index_id.find ( newId ) == index_id.end () )
    {
      return newId;
    }
  }
}

void KVStore::load ()
{
  lock_guard<mutex> lock ( mutex_lock );

  if ( !filesystem::exists ( filename ) )
  {
    return;
  }

  auto       ftime        = filesystem::last_write_time ( filename );
  auto       modifiedTime = chrono::duration_cast<chrono::seconds> ( ftime.time_since_epoch () ).count ();
  YAML::Node data         = YAML::LoadFile ( filename );

  for ( auto& item : data["store"] )
  {
    KeyData keyData;
    keyData.key        = item["key"].as<string> ();
    keyData.value      = item["value"].as<string> ();
    keyData.id         = item["id"] ? item["id"].as<string> () : generateID ();
    keyData.created_at = item["created_at"] ? item["created_at"].as<double> () : modifiedTime;
    keyData.updated_at = item["updated_at"] ? item["updated_at"].as<double> () : modifiedTime;

    if ( item["appends"] )
    {
      for ( auto& append : item["appends"] )
      {
        AppendKeyData appendData;
        appendData.type                             = append.second["type"].as<string> ();
        appendData.value                            = append.second["value"].as<string> ();
        keyData.appends[append.first.as<string> ()] = appendData;
      }
    }

    index_key[keyData.key]            = keyData;
    index_id[keyData.id]              = keyData.key;
    index_created[keyData.created_at] = keyData.key;
    index_updated[keyData.updated_at] = keyData.key;
  }
}

void KVStrore::save ()
{
  YAML::Emitter yml;

  yml << YAML::BeginMap;
  yml << YAML::Key << "store" << YAML::Value << YAML::BeginSeq;

  for ( const auto& [key, data] : index_key )
  {
    yml << YAML::BeginMap;
    yml << YAML::Key << "id" << YAML::Value << data.id;
    yml << YAML::Key << "key" << YAML::Value << data.key;
    yml << YAML::Key << "value" << YAML::Value << data.value;
    yml << YAML::Key << "created_at" << YAML::Value << data.created_at;
    yml << YAML::Key << "updated_at" << YAML::Value << data.updated_at;

    for ( const auto& [appendKey, appendData] : data.appends )
    {
      yml << YAML::Key << appendKey << YAML::Value << YAML::BeginMap;
      yml << YAML::Key << "type" << YAML::Value << appendData.getType ();
      yml << YAML::Key << "value" << YAML::Value << appendData.getString ();
      yml << YAML::EndMap;
    }

    yml << YAML::EndMap;
  }

  yml << YAML::EndSeq;
  yml << YAML::EndMap;

  ofstream outFile ( filename );
  outFile << yml.c_str ();

  outFile.close ();
}

void KVStrore::append ( const string& key, const AppendKeyData& appendData )
{
  lock_guard<mutex> lock ( mutex_lock );

  if ( index_key.find ( key ) != index_key.end () )
  {
    index_key[key].appends[appendData.getType ()] = appendData;
    queue.push ( index_key[key] );
    queue_condition.notify_one ();
  }
  else
  {
    throw runtime_error ( "Key not found" );
  }
}

optional<AppendKeyData> KVStrore::getAppend ( const string& key ) const
{
  lock_guard<mutex> lock ( mutex_lock );

  if ( index_key.find ( key ) != index_key.end () )
  {
    return index_key.at ( key ).appends[key];
  }
  return nullopt;
}

bool KVStrore::existsAppend ( const string& key ) const
{
  lock_guard<mutex> lock ( mutex_lock );

  return index_key.find ( key ) != index_key.end () && index_key.at ( key ).appends.find ( key ) != index_key.at ( key ).appends.end ();
}

vector<string> KVStrore::appendKeys () const
{
  lock_guard<mutex> lock ( mutex_lock );
  vector<string>    keys;

  for ( const auto& [key, data] : index_key )
  {
    for ( const auto& [appendKey, _] : data.appends )
    {
      keys.push_back ( appendKey );
    }
  }

  return keys;
}

string KVStrore::toYaml () const
{
  YAML::Emitter yml;

  yml << YAML::BeginMap;
  yml << YAML::Key << "store" << YAML::Value << YAML::BeginSeq;

  for ( const auto& [key, data] : index_key )
  {
    yml << data.toYaml ();
  }

  yml << YAML::EndSeq;
  yml << YAML::EndMap;
  return string ( yml.c_str () );
}

double KVStrore::getEpoch ( bool withMilliseconds ) const
{
  auto now      = chrono::system_clock::now ();
  auto duration = now.time_since_epoch ();

  return withMilliseconds ? chrono::duration<double> ( duration ).count () : chrono::duration_cast<chrono::seconds> ( duration ).count ();
}

void KVStrore::queueWorker ()
{
  while ( !thread_queue_stop )
  {
    unique_lock<mutex> lock ( mutex_lock );

    queue_condition.wait ( lock, [&] () { return !queue.empty () || thread_queue_stop; } );

    if ( thread_queue_stop )
    {
      return;
    }

    while ( !queue.empty () )
    {
      KeyData data = queue.front ();
      queue.pop ();
      index_key[data.key] = data;
    }

    lock.unlock ();
    save ();
  }
}

vData KVStrore::find ( const std::string& pattern ) const
{
  std::lock_guard<std::mutex> lock ( mutex_lock );

  std::vector<KeyData> results;

  for ( const auto& [key, data] : index_key )
  {
    if ( key.find ( pattern ) != std::string::npos )
    {
      results.push_back ( data );
    }
  }

  return KvData ( results );
}

KvData KVStrore::findRegex ( const std::string& regexPattern ) const
{
  std::lock_guard<std::mutex> lock ( mutex_lock );

  std::regex           regex ( regexPattern );
  std::vector<KeyData> results;

  for ( const auto& [key, data] : index_key )
  {
    if ( std::regex_match ( key, regex ) )
    {
      results.push_back ( data );
    }
  }

  return KvData ( results );
}

KvData KVStrore::findCreatedDate ( double start, double end ) const
{
  std::lock_guard<std::mutex> lock ( mutex_lock );

  std::vector<KeyData> results;
  double               now = getEpoch ();

  start = start < 0 ? now + start : start;

  if ( end != 0 )
  {
    end = end < 0 ? now + end : end;
  }

  for ( const auto& [time, key] : index_created )
  {
    if ( time >= start && ( end == 0 || time <= end ) )
    {
      results.push_back ( index_key.at ( key ) );
    }
  }

  return KvData ( results );
}

KvData KVStrore::findUpdatedDate ( double start, double end ) const
{
  std::lock_guard<std::mutex> lock ( mutex_lock );

  std::vector<KeyData> results;
  double               now = getEpoch ();

  start = start < 0 ? now + start : start;

  if ( end != 0 )
  {
    end = end < 0 ? now + end : end;
  }

  for ( const auto& [time, key] : index_updated )
  {
    if ( time >= start && ( end == 0 || time <= end ) )
    {
      results.push_back ( index_key.at ( key ) );
    }
  }

  return KvData ( results );
}

void KVStrore::clear ()
{
  std::lock_guard<std::mutex> lock ( mutex_lock );

  index_key.clear ();
  index_id.clear ();
  index_created.clear ();
  index_updated.clear ();
  save ();
}

void KVStrore::close ()
{
  save ();
}