#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700

#include <dirent.h>
#include <inttypes.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <uthash.h>
#include <json-c/json.h>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <uuid/uuid.h>

#define PORT                     8832
#define BUFFER_SIZE              1024
#define FILENAME_SIZE            1024
#define UID_SIZE                 13
#define CLIENT_TIMEOUT           5
#define BASE62                   "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define DATA_DIR                 "data"
#define MODEL_DIR                "models"
#define LOG_DIR                  "logs"
#define LIMIT                    100
#define CHUNK_SIZE               1000
#define EVALUATE_MEMORY_LIMIT_MB 100
#define EVALUATE_BATCH_LIMIT     1000

typedef struct
{
  double value;
} Data;

typedef struct
{
  char    uid[UID_SIZE];
  Data   *data;
  int     data_length;
  char  **tags;
  int     tag_count;
  int64_t epoch;
} Record;

typedef struct
{
  char           key[UID_SIZE];
  int64_t        epoch;
  long           offset;
  UT_hash_handle hh;
} IndexEntry;

typedef struct
{
  const double *data;
  size_t        data_len;
  int           client_socket;
} EvaluationArgs;

IndexEntry     *Tag_index = NULL;
pthread_mutex_t Index_lock;

double c_mean ( const double *data, size_t length );
double c_cv ( double mean, double standard_deviation );
double c_exp_value ( const double *data, size_t len );
double c_geo_mean ( const double *data, size_t len );
double c_harm_mean ( const double *data, size_t len );
double c_iqr ( double *data, size_t len );
double c_kurtosis ( const double *data, size_t len, double mean, double standard_deviation );
double c_mad ( double *data, size_t len, double median );
double c_mae ( const double *data, size_t len );
double c_max_deviation ( const double *data, size_t len, double mean );
double c_mead ( double *data, size_t len );
double c_mean ( const double *data, size_t len );
double c_median ( double *data, size_t len );
double c_mode ( double *data, size_t len );
double c_mse ( const double *data, size_t len, double mean );
double c_mskewness ( double *data, size_t len, double median );
double c_percent_range ( const double *data, size_t len );
double c_percentile ( double *data, size_t len, double percentile );
double c_range ( const double *data, size_t len );
double c_rms ( const double *data, size_t len );
double c_rmsle ( const double *data, size_t len );
double c_std_deviation ( const double *data, size_t len, double mean );
double c_trim_mean ( double *data, size_t len, double trim_ratio );
double c_variance ( const double *data, size_t len, double mean );
double c_zskewness ( const double *data, size_t len, double mean );
double c_bin_entropy ( const double *data, size_t len ); // 수정된 함수 선언
void   c_quartiles ( double *data, size_t len, double *q1, double *q2, double *q3 );

int64_t     now ();
void        now_timestamp ( char *buffer );
void        error_log ( const char *message );
void        fs_filename ( char *type, char *tag, int64_t epoch, char *output_filename );
void        fs_mkdir ( const char *directory );
void        fs_save ( const char *filename, const void *data, size_t size );
void        generate_uid ( char *uid );
void       *handle_client ( void *arg );
void        index_entry_add ( IndexEntry **index, const char *key, int64_t epoch, long offset );
IndexEntry *index_entry_search ( IndexEntry *index, const char *key );
int64_t     parse_date ( const char *date_str );
void        query_diagnosis ( int client_socket, struct json_object *_json );
void        query_evaluate ( int client_socket, const double *all_data, size_t data_len ); // 수정된 함수 선언
void        query_push ( int client_socket, struct json_object *_json );
void        query_search ( int client_socket, struct json_object *_json, bool is_evaluate );
void        search ( const char **tags, int tag_count, const char *condition, int64_t start_time, int64_t end_time, int client_socket, bool is_evaluate );
void        send_err ( int client_socket, int error_code, const char *message_format, ... );
void        free_index_entries ( IndexEntry *index );
size_t      memory_usage ();
void       *evaluate_thread ( void *args );
void        process_batch ( const double *data, size_t len, int client_socket, bool is_evaluate );
void        log_request ( const char *query, const char **tags, int tag_count, const char *condition, const char *data, int64_t start_time, int64_t response_time );

size_t memory_usage ()
{
  struct rusage usage;

  getrusage ( RUSAGE_SELF, &usage );
  return usage.ru_maxrss / 1024;
}

void *evaluate_thread ( void *args )
{
  EvaluationArgs *eval_args = ( EvaluationArgs * )args;

  query_evaluate ( eval_args->client_socket, eval_args->data, eval_args->data_len );
  free ( ( void * )eval_args->data );
  free ( eval_args );
  return NULL;
}

void UUID ( char *out )
{
  uuid_t uuid;
  uuid_generate ( uuid );

  uint64_t high = ( ( uint64_t )uuid[0] << 56 ) | ( ( uint64_t )uuid[1] << 48 ) | ( ( uint64_t )uuid[2] << 40 ) | ( ( uint64_t )uuid[3] << 32 ) | ( ( uint64_t )uuid[4] << 24 ) | ( ( uint64_t )uuid[5] << 16 ) | ( ( uint64_t )uuid[6] << 8 ) | ( uint64_t )uuid[7];
  uint64_t low  = ( ( uint64_t )uuid[8] << 56 ) | ( ( uint64_t )uuid[9] << 48 ) | ( ( uint64_t )uuid[10] << 40 ) | ( ( uint64_t )uuid[11] << 32 ) | ( ( uint64_t )uuid[12] << 24 ) | ( ( uint64_t )uuid[13] << 16 ) | ( ( uint64_t )uuid[14] << 8 ) | ( uint64_t )uuid[15];
  int      idx  = UID_SIZE - 1;

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

int64_t now ()
{
  struct timespec ts;
  clock_gettime ( CLOCK_REALTIME, &ts );

  return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

void now_timestamp ( char *buffer )
{
  time_t     now_seconds = now () / 1000;
  struct tm *now_tm      = localtime ( &now_seconds );

  strftime ( buffer, 32, "%Y-%m-%d %H:%M:%S", now_tm );
}


void error_log ( const char *message )
{
  perror ( message );
  exit ( 1 );
}

void fs_mkdir ( const char *directory )
{
  struct stat st = { 0 };

  if ( stat ( directory, &st ) == -1 && mkdir ( directory, 0700 ) != 0 )
  {
    error_log ( "fs_mkdir - failed to create directory." );
  }
}

void generate_uid ( char *uid )
{
  uuid_t             binuuid;
  unsigned char      base62_str[UID_SIZE + 1] = { 0 };
  unsigned long long decimal_value            = 0;

  uuid_generate ( binuuid );

  for ( int i = 0; i < 16; ++i )
  {
    decimal_value = ( decimal_value << 8 ) | binuuid[i];
  }

  unsigned int index = UID_SIZE;
  base62_str[index]  = '\0';

  do
  {
    base62_str[--index] = BASE62[decimal_value % 62];
    decimal_value /= 62;
  } while ( decimal_value > 0 && index > 0 );

  memcpy ( uid, base62_str + index, UID_SIZE - index );
  uid[UID_SIZE - index] = '\0';
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

void index_entry_add ( IndexEntry **index, const char *key, int64_t epoch, long offset )
{
  IndexEntry *entry = ( IndexEntry * )malloc ( sizeof ( IndexEntry ) );

  strncpy ( entry->key, key, UID_SIZE );
  entry->epoch  = epoch;
  entry->offset = offset;

  HASH_ADD_STR ( *index, key, entry );
}

IndexEntry *index_entry_search ( IndexEntry *index, const char *key )
{
  IndexEntry *entry;

  HASH_FIND_STR ( index, key, entry );
  return entry;
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

void process_batch ( const double *data, size_t len, int client_socket, bool is_evaluate )
{
  if ( is_evaluate )
  {
    int64_t request_time = now ();

    EvaluationArgs *args = malloc ( sizeof ( EvaluationArgs ) );
    args->data           = malloc ( len * sizeof ( double ) );
    memcpy ( ( void * )args->data, data, len * sizeof ( double ) );
    args->data_len      = len;
    args->client_socket = client_socket;

    pthread_t eval_thread;
    int       thread_result = pthread_create ( &eval_thread, NULL, evaluate_thread, args );
    if ( thread_result != 0 )
    {
      fprintf ( stderr, "Failed to create thread: %s\n", strerror ( thread_result ) );
      free ( args );
      close ( client_socket );
      return;
    }

    pthread_detach ( eval_thread );
    log_request ( "evaluate", NULL, 0, NULL, NULL, request_time, now () );
  }
}


void log_request ( const char *query, const char **tags, int tag_count, const char *condition, const char *data, int64_t start_time, int64_t response_time )
{
  char log_filename[128];
  char timestamp[32];
  char date_str[16];

  now_timestamp ( timestamp );

  // 로그 디렉토리 생성
  fs_mkdir ( LOG_DIR );

  time_t     now_seconds = now () / 1000;
  struct tm *now_tm      = localtime ( &now_seconds );
  strftime ( date_str, sizeof ( date_str ), "%Y%m%d", now_tm );

  snprintf ( log_filename, sizeof ( log_filename ), "./%s/%s.log", LOG_DIR, date_str );

  FILE *log_file = fopen ( log_filename, "a" );
  if ( !log_file )
  {
    perror ( "Failed to open log file" );
    return;
  }

  char tags_buffer[1024] = { 0 };
  if ( tags != NULL )
  {
    size_t tags_len = 0;
    for ( int i = 0; i < tag_count; i++ )
    {
      if ( tags_len + strlen ( tags[i] ) + 2 < sizeof ( tags_buffer ) )
      {
        if ( i > 0 )
        {
          strcat ( tags_buffer, ", " );
          tags_len += 2;
        }

        strcat ( tags_buffer, tags[i] );
        tags_len += strlen ( tags[i] );
      }
      else
      {
        break;
      }
    }
  }

  double elapsed_time = ( response_time - start_time ) / 1000.0;
  fprintf ( log_file, "[%s] %s | tags: %s", timestamp, query, tags_buffer );

  if ( condition && strlen ( condition ) > 0 )
  {
    fprintf ( log_file, " | condition: %s", condition );
  }

  if ( data && strlen ( data ) > 0 )
  {
    fprintf ( log_file, " | data: %s", data );
  }

  fprintf ( log_file, " | response: %.3f sec.\n", elapsed_time );
  fclose ( log_file );
}


void free_index_entries ( IndexEntry *index )
{
  IndexEntry *current_entry, *tmp;

  HASH_ITER ( hh, index, current_entry, tmp )
  {
    HASH_DEL ( index, current_entry );
    free ( current_entry );
  }
}


void *handle_client ( void *arg )
{
  int  client_socket       = ( intptr_t )arg;
  char buffer[BUFFER_SIZE] = { 0 };

  read ( client_socket, buffer, BUFFER_SIZE );

  struct json_object *_json = json_tokener_parse ( buffer );

  if ( _json )
  {
    struct json_object *command_obj;

    if ( json_object_object_get_ex ( _json, "query", &command_obj ) )
    {
      const char *query = json_object_get_string ( command_obj );

      if ( strcmp ( query, "push" ) == 0 )
      {
        query_push ( client_socket, _json );
      }
      else if ( strcmp ( query, "search" ) == 0 )
      {
        query_search ( client_socket, _json, false );
      }
      else if ( strcmp ( query, "evaluate" ) == 0 )
      {
        query_diagnosis ( client_socket, _json );
      }
      else
      {
        send_err ( client_socket, 11, "Invalid request. Not supported query command" );
      }

      json_object_put ( _json );
    }
    else
    {
      send_err ( client_socket, 10, "Invalid request. Query command not found." );
    }
  }
  else
  {
    send_err ( client_socket, 0, "Invalid request. Failed to parse JSON." );
  }

  close ( client_socket );
  return NULL;
}

void query_push ( int client_socket, struct json_object *_json )
{
  int64_t request_time = now ();
  char    uid[UID_SIZE];

  UUID ( uid );
  int64_t epoch = now ();

  struct json_object *data_array = json_object_object_get ( _json, "data" );
  struct json_object *tags_arr   = json_object_object_get ( _json, "tags" );

  if ( data_array == NULL || tags_arr == NULL )
  {
    send_err ( client_socket, 50, "Invalid request. Tags and data are required." );
    return;
  }

  Record record;
  strncpy ( record.uid, uid, UID_SIZE );

  record.epoch       = epoch;
  record.data_length = json_object_array_length ( data_array );
  record.data        = ( Data        *)malloc ( record.data_length * sizeof ( Data ) );
  record.tag_count   = json_object_array_length ( tags_arr );
  record.tags        = ( char        **)malloc ( record.tag_count * sizeof ( char        *) );

  // Copy tags and data to be used in log_request
  char **log_tags          = ( char          **)malloc ( record.tag_count * sizeof ( char          *) );
  char   data_buffer[1024] = { 0 };
  for ( int i = 0; i < record.data_length; i++ )
  {
    struct json_object *data_item = json_object_array_get_idx ( data_array, i );
    record.data[i].value          = json_object_get_double ( data_item );

    char value_str[32];
    snprintf ( value_str, sizeof ( value_str ), "%.2f", record.data[i].value );
    if ( i > 0 )
    {
      strcat ( data_buffer, ", " );
    }
    strcat ( data_buffer, value_str );
  }

  for ( int i = 0; i < record.tag_count; i++ )
  {
    record.tags[i] = strdup ( json_object_get_string ( json_object_array_get_idx ( tags_arr, i ) ) );
    log_tags[i]    = strdup ( record.tags[i] );
  }

  // Log request before freeing record.tags
  log_request ( "push", ( const char ** )log_tags, record.tag_count, NULL, data_buffer, request_time, now () );

  for ( int i = 0; i < record.tag_count; i++ )
  {
    char filename[FILENAME_SIZE];
    fs_filename ( "tag", record.tags[i], epoch, filename );
    FILE *file = fopen ( filename, "ab" );
    if ( !file )
    {
      error_log ( "query_push - failed to open file for write." );
    }

    fwrite ( &record, sizeof ( Record ), 1, file );
    fwrite ( record.data, sizeof ( Data ), record.data_length, file );

    for ( int j = 0; j < record.tag_count; j++ )
    {
      size_t tag_len = strlen ( record.tags[j] ) + 1;
      fwrite ( &tag_len, sizeof ( size_t ), 1, file );
      fwrite ( record.tags[j], sizeof ( char ), tag_len, file );
    }

    fclose ( file );
  }

  pthread_mutex_lock ( &Index_lock );
  index_entry_add ( &Tag_index, record.uid, record.epoch, 0 );
  pthread_mutex_unlock ( &Index_lock );

  free ( record.data );
  for ( int i = 0; i < record.tag_count; i++ )
  {
    free ( record.tags[i] );
    free ( log_tags[i] ); // Free log_tags after logging
  }
  free ( record.tags );
  free ( log_tags );

  struct json_object *response = json_object_new_object ();
  json_object_object_add ( response, "uid", json_object_new_string ( uid ) );
  const char *response_str = json_object_to_json_string ( response );
  send ( client_socket, response_str, strlen ( response_str ), 0 );

  json_object_put ( response );
}


void send_err ( int client_socket, int error_code, const char *message_format, ... )
{
  char message_buffer[BUFFER_SIZE / 2];
  char json_buffer[BUFFER_SIZE];

  va_list args;
  va_start ( args, message_format );
  vsnprintf ( message_buffer, sizeof ( message_buffer ), message_format, args );
  va_end ( args );
  snprintf ( json_buffer, sizeof ( json_buffer ), "{\"error\": %d, \"message\": \"%s\"}", error_code, message_buffer );
  send ( client_socket, json_buffer, strlen ( json_buffer ), 0 );
}

void query_search ( int client_socket, struct json_object *_json, bool is_evaluate )
{
  int64_t             request_time = now ();
  struct json_object *_tags        = json_object_object_get ( _json, "tags" );
  struct json_object *_conditions  = json_object_object_get ( _json, "condition" );
  struct json_object *_start_time  = json_object_object_get ( _json, "startTime" );
  struct json_object *_end_time    = json_object_object_get ( _json, "endTime" );

  if ( _tags == NULL || json_object_array_length ( _tags ) == 0 )
  {
    send_err ( client_socket, 20, "Invalid request. Tags not found." );
    return;
  }

  int     tag_count    = json_object_array_length ( _tags );
  int64_t current_time = now ();
  int64_t start_time   = 0;
  int64_t end_time     = current_time;

  if ( _start_time != NULL )
  {
    start_time = json_object_get_int64 ( _start_time );
  }

  if ( _end_time != NULL )
  {
    end_time = json_object_get_int64 ( _end_time );
  }

  if ( start_time < 0 || start_time > current_time )
  {
    send_err ( client_socket, 41, "Start time must be greater than 0 and less than %ld.", start_time );
    return;
  }

  if ( end_time < 0 || end_time > current_time )
  {
    send_err ( client_socket, 42, "End time must be greater than 0 and less than %ld.", end_time );
    return;
  }

  if ( start_time > end_time )
  {
    send_err ( client_socket, 43, "Start time must be less than the end time." );
    return;
  }

  const char *condition = "or";

  if ( _conditions != NULL )
  {
    condition = json_object_get_string ( _conditions );
  }

  if ( strcmp ( condition, "and" ) != 0 && strcmp ( condition, "or" ) != 0 && strcmp ( condition, "nand" ) != 0 && strcmp ( condition, "nor" ) != 0 )
  {
    send_err ( client_socket, 30, "Invalid request. Not supported condition." );
    return;
  }

  if ( tag_count == 1 && strcmp ( condition, "and" ) == 0 )
  {
    condition = "or";
  }

  if ( tag_count == 1 && strcmp ( condition, "nand" ) == 0 )
  {
    condition = "nor";
  }

  const char **tags = ( const char ** )malloc ( tag_count * sizeof ( char * ) );

  for ( int i = 0; i < tag_count; i++ )
  {
    tags[i] = json_object_get_string ( json_object_array_get_idx ( _tags, i ) );
  }

  search ( tags, tag_count, condition, start_time, end_time, client_socket, is_evaluate );

  log_request ( "search", tags, tag_count, condition, NULL, request_time, now () );

  free ( tags );
}

void query_diagnosis ( int client_socket, struct json_object *_json )
{
  query_search ( client_socket, _json, true );
}

void search ( const char **tags, int tag_count, const char *condition, int64_t start_time, int64_t end_time, int client_socket, bool is_evaluate )
{
  DIR                *d;
  struct dirent      *dir;
  char                path[FILENAME_SIZE];
  struct json_object *json_array = json_object_new_array ();

  snprintf ( path, FILENAME_SIZE, "%s/", DATA_DIR );
  d = opendir ( path );

  if ( !d )
  {
    fs_mkdir ( path );
    d = opendir ( path );
  }

  double   *all_data     = NULL;
  size_t    data_len     = 0;
  size_t    result_count = 0;
  pthread_t eval_thread;

  while ( ( dir = readdir ( d ) ) != NULL )
  {
    if ( dir->d_type == DT_REG )
    {
      char *filename     = dir->d_name;
      bool  file_matches = false;

      if ( strcmp ( condition, "nand" ) == 0 || strcmp ( condition, "nor" ) == 0 )
      {
        file_matches = true;

        for ( int i = 0; i < tag_count; i++ )
        {
          if ( strncmp ( filename, tags[i], strlen ( tags[i] ) ) == 0 && strstr ( filename, ".db" ) )
          {
            file_matches = false;
            break;
          }
        }
      }
      else if ( strcmp ( condition, "or" ) == 0 || strcmp ( condition, "and" ) == 0 )
      {
        for ( int i = 0; i < tag_count; i++ )
        {
          if ( strncmp ( filename, tags[i], strlen ( tags[i] ) ) == 0 && strstr ( filename, ".db" ) )
          {
            file_matches = true;
            break;
          }
        }
      }

      if ( file_matches )
      {
        char date_str[9];
        sscanf ( filename + strlen ( tags[0] ) + 1, "%8s", date_str );
        int64_t file_time = parse_date ( date_str ) * 1000;

        if ( file_time >= start_time && file_time <= end_time )
        {
          char full_filename[FILENAME_SIZE];
          int  len = snprintf ( full_filename, FILENAME_SIZE, "%s/%s", path, filename );

          if ( len >= FILENAME_SIZE )
          {
            error_log ( "search - filename is too long." );
          }

          FILE *file = fopen ( full_filename, "rb" );
          if ( !file )
          {
            error_log ( "search - failed to open file for read." );
          }

          while ( 1 )
          {
            Record record;
            size_t record_size = fread ( &record, sizeof ( Record ), 1, file );

            if ( record_size != 1 )
            {
              if ( feof ( file ) )
              {
                break;
              }
              else
              {
                error_log ( "search - failed to read amount data." );
                break;
              }
            }

            record.data = ( Data * )malloc ( record.data_length * sizeof ( Data ) );
            if ( !record.data )
            {
              fclose ( file );
              error_log ( "search - failed to malloc record.data" );
            }

            size_t read_data = fread ( record.data, sizeof ( Data ), record.data_length, file );
            if ( read_data != record.data_length )
            {
              free ( record.data );

              if ( feof ( file ) )
              {
                break;
              }
              else
              {
                fclose ( file );
                error_log ( "search - failed to read amount data" );
              }
            }

            record.tags = ( char ** )malloc ( record.tag_count * sizeof ( char * ) );
            if ( !record.tags )
            {
              free ( record.data );
              fclose ( file );
              error_log ( "search - failed to malloc record.tags" );
            }

            bool read_error = false;

            for ( int j = 0; j < record.tag_count; j++ )
            {
              size_t tag_len;
              if ( fread ( &tag_len, sizeof ( size_t ), 1, file ) != 1 )
              {
                read_error = true;
                break;
              }

              record.tags[j] = ( char * )malloc ( tag_len );
              if ( !record.tags[j] )
              {
                read_error = true;
                break;
              }

              if ( fread ( record.tags[j], sizeof ( char ), tag_len, file ) != tag_len )
              {
                read_error = true;
                break;
              }
            }

            if ( read_error )
            {
              for ( int j = 0; j < record.tag_count; j++ )
              {
                if ( record.tags[j] )
                {
                  free ( record.tags[j] );
                }
              }

              free ( record.tags );
              free ( record.data );
              fclose ( file );
              error_log ( "search - failed to read tags" );
            }

            bool match = false;

            if ( strcmp ( condition, "and" ) == 0 )
            {
              match = true;

              for ( int j = 0; j < tag_count; j++ )
              {
                bool found = false;

                for ( int k = 0; k < record.tag_count; k++ )
                {
                  if ( strcmp ( tags[j], record.tags[k] ) == 0 )
                  {
                    found = true;
                    break;
                  }
                }

                if ( !found )
                {
                  match = false;
                  break;
                }
              }
            }
            else if ( strcmp ( condition, "nand" ) == 0 || strcmp ( condition, "nor" ) == 0 )
            {
              match = true;

              for ( int j = 0; j < tag_count; j++ )
              {
                for ( int k = 0; k < record.tag_count; k++ )
                {
                  if ( strcmp ( tags[j], record.tags[k] ) == 0 )
                  {
                    match = false;
                    break;
                  }
                }

                if ( !match )
                {
                  break;
                }
              }
            }
            else if ( strcmp ( condition, "or" ) == 0 )
            {
              match = true;
            }

            if ( match && record.epoch >= start_time && record.epoch <= end_time )
            {
              if ( is_evaluate )
              {
                if ( memory_usage () > EVALUATE_MEMORY_LIMIT_MB )
                {
                  error_log ( "Memory usage limit exceeded." );
                  break;
                }

                all_data = realloc ( all_data, ( data_len + record.data_length ) * sizeof ( double ) );

                if ( !all_data )
                {
                  fclose ( file );
                  error_log ( "search - failed to realloc diagnosis" );
                }

                for ( int j = 0; j < record.data_length; j++ )
                {
                  all_data[data_len++] = record.data[j].value;
                }

                if ( data_len >= EVALUATE_BATCH_LIMIT )
                {
                  EvaluationArgs *args = malloc ( sizeof ( EvaluationArgs ) );

                  args->data          = all_data;
                  args->data_len      = data_len;
                  args->client_socket = client_socket;
                  pthread_create ( &eval_thread, NULL, evaluate_thread, args );
                  pthread_detach ( eval_thread );
                  all_data = NULL;
                  data_len = 0;
                }
              }
              else
              {
                struct json_object *json_record = json_object_new_object ();
                json_object_object_add ( json_record, "uid", json_object_new_string ( record.uid ) );
                json_object_object_add ( json_record, "timestamp", json_object_new_int64 ( record.epoch ) );

                struct json_object *data_obj = json_object_new_array ();

                for ( int j = 0; j < record.data_length; j++ )
                {
                  json_object_array_add ( data_obj, json_object_new_double ( record.data[j].value ) );
                }

                json_object_object_add ( json_record, "data", data_obj );
                json_object_array_add ( json_array, json_record );
                result_count++;

                if ( result_count >= LIMIT )
                {
                  const char *json_output = json_object_to_json_string_ext ( json_array, JSON_C_TO_STRING_PRETTY );
                  send ( client_socket, json_output, strlen ( json_output ), 0 );
                  json_object_put ( json_array );
                  json_array   = json_object_new_array ();
                  result_count = 0;
                }
              }
            }

            for ( int j = 0; j < record.tag_count; j++ )
            {
              free ( record.tags[j] );
            }

            free ( record.tags );
            free ( record.data );
          }

          fclose ( file );
        }
      }
    }
  }

  closedir ( d );

  if ( is_evaluate && data_len > 0 )
  {
    EvaluationArgs *args = malloc ( sizeof ( EvaluationArgs ) );
    args->data           = all_data;
    args->data_len       = data_len;
    args->client_socket  = client_socket;
    pthread_create ( &eval_thread, NULL, evaluate_thread, args );
    pthread_detach ( eval_thread );
  }
  else
  {
    if ( result_count > 0 )
    {
      const char *json_output = json_object_to_json_string_ext ( json_array, JSON_C_TO_STRING_PRETTY );
      send ( client_socket, json_output, strlen ( json_output ), 0 );
    }
    json_object_put ( json_array );
  }
  close ( client_socket ); // 소켓 닫기
}

void query_evaluate ( int client_socket, const double *all_data, size_t data_len )
{
  struct json_object *json_diagnosis = json_object_new_object ();

  json_object_object_add ( json_diagnosis, "length", json_object_new_int64 ( data_len ) );
  json_object_object_add ( json_diagnosis, "limit", json_object_new_int64 ( LIMIT ) );

  if ( data_len > 0 )
  {
    double *data_copy = ( double * )malloc ( data_len * sizeof ( double ) );
    if ( !data_copy )
    {
      error_log ( "query_evaluate - failed to malloc data_copy." );
    }
    memcpy ( data_copy, all_data, data_len * sizeof ( double ) );

    qsort ( data_copy, data_len, sizeof ( double ), ( int ( * ) ( const void *, const void * ) )strcmp );
    double mean = c_mean ( data_copy, data_len );

    double q1, q2, q3;
    c_quartiles ( data_copy, data_len, &q1, &q2, &q3 );

    json_object_object_add ( json_diagnosis, "max", json_object_new_double ( data_copy[data_len - 1] ) );
    json_object_object_add ( json_diagnosis, "min", json_object_new_double ( data_copy[0] ) );
    json_object_object_add ( json_diagnosis, "mean", json_object_new_double ( mean ) );
    json_object_object_add ( json_diagnosis, "median", json_object_new_double ( c_median ( data_copy, data_len ) ) );
    json_object_object_add ( json_diagnosis, "mode", json_object_new_double ( c_mode ( data_copy, data_len ) ) );
    json_object_object_add ( json_diagnosis, "variance", json_object_new_double ( c_variance ( data_copy, data_len, mean ) ) );
    json_object_object_add ( json_diagnosis, "standardDeviation", json_object_new_double ( c_std_deviation ( data_copy, data_len, mean ) ) );
    json_object_object_add ( json_diagnosis, "trimmedMean", json_object_new_double ( c_trim_mean ( data_copy, data_len, 0.1 ) ) );
    json_object_object_add ( json_diagnosis, "harmonicMean", json_object_new_double ( c_harm_mean ( data_copy, data_len ) ) );
    json_object_object_add ( json_diagnosis, "geometricMean", json_object_new_double ( c_geo_mean ( data_copy, data_len ) ) );
    json_object_object_add ( json_diagnosis, "range", json_object_new_double ( c_range ( data_copy, data_len ) ) );
    json_object_object_add ( json_diagnosis, "iqr", json_object_new_double ( c_iqr ( data_copy, data_len ) ) );
    json_object_object_add ( json_diagnosis, "expectedValue", json_object_new_double ( mean ) );
    json_object_object_add ( json_diagnosis, "mad", json_object_new_double ( c_mad ( data_copy, data_len, mean ) ) );
    json_object_object_add ( json_diagnosis, "mead", json_object_new_double ( c_mead ( data_copy, data_len ) ) );
    json_object_object_add ( json_diagnosis, "rms", json_object_new_double ( c_rms ( data_copy, data_len ) ) );
    json_object_object_add ( json_diagnosis, "mse", json_object_new_double ( c_mse ( data_copy, data_len, mean ) ) );
    json_object_object_add ( json_diagnosis, "mae", json_object_new_double ( c_mae ( data_copy, data_len ) ) );
    json_object_object_add ( json_diagnosis, "zskewness", json_object_new_double ( c_zskewness ( data_copy, data_len, mean ) ) );
    json_object_object_add ( json_diagnosis, "mskewness", json_object_new_double ( c_mskewness ( data_copy, data_len, c_median ( data_copy, data_len ) ) ) );
    json_object_object_add ( json_diagnosis, "kurtosis", json_object_new_double ( c_kurtosis ( data_copy, data_len, mean, c_std_deviation ( data_copy, data_len, mean ) ) ) );
    json_object_object_add ( json_diagnosis, "cv", json_object_new_double ( c_cv ( mean, c_std_deviation ( data_copy, data_len, mean ) ) ) );
    json_object_object_add ( json_diagnosis, "maximumDeviation", json_object_new_double ( c_max_deviation ( data_copy, data_len, mean ) ) );
    json_object_object_add ( json_diagnosis, "binaryEntropy", json_object_new_double ( c_bin_entropy ( data_copy, data_len ) ) ); // 수정된 호출
    json_object_object_add ( json_diagnosis, "rmsle", json_object_new_double ( c_rmsle ( data_copy, data_len ) ) );
    json_object_object_add ( json_diagnosis, "percentRange", json_object_new_double ( c_percent_range ( data_copy, data_len ) ) );

    free ( data_copy );
  }

  const char *json_output = json_object_to_json_string_ext ( json_diagnosis, JSON_C_TO_STRING_PRETTY );
  send ( client_socket, json_output, strlen ( json_output ), 0 );
  json_object_put ( json_diagnosis );
}


int64_t parse_date ( const char *date_str )
{
  struct tm tm = { 0 };

  strptime ( date_str, "%Y%m%d", &tm );
  return ( int64_t )mktime ( &tm );
}

double c_mean ( const double *data, size_t length )
{
  double sum = 0.0;

  for ( size_t i = 0; i < length; i++ )
  {
    sum += data[i];
  }

  return sum / length;
}

double c_variance ( const double *data, size_t len, double mean )
{
  double sum_squared_diff = 0.0;

  for ( size_t i = 0; i < len; i++ )
  {
    sum_squared_diff += ( data[i] - mean ) * ( data[i] - mean );
  }

  return sum_squared_diff / len;
}

double c_std_deviation ( const double *data, size_t len, double mean )
{
  return sqrt ( c_variance ( data, len, mean ) );
}

double c_median ( double *data, size_t len )
{
  qsort ( data, len, sizeof ( double ), ( int ( * ) ( const void *, const void * ) )strcmp );

  if ( len % 2 == 0 )
  {
    return ( data[len / 2 - 1] + data[len / 2] ) / 2.0;
  }
  else
  {
    return data[len / 2];
  }
}

double c_mode ( double *data, size_t len )
{
  double mode       = data[0];
  int    mode_count = 1;
  int    max_count  = 1;

  for ( size_t i = 1; i < len; i++ )
  {
    if ( data[i] == mode )
    {
      mode_count++;
    }
    else
    {
      if ( mode_count > max_count )
      {
        max_count = mode_count;
        mode      = data[i - 1];
      }

      mode_count = 1;
    }
  }

  return mode;
}

double c_trim_mean ( double *data, size_t len, double trim_ratio )
{
  size_t trim_amount = ( size_t )( len * trim_ratio );
  return c_mean ( data + trim_amount, len - 2 * trim_amount );
}

double c_harm_mean ( const double *data, size_t len )
{
  double sum_reciprocal = 0.0;
  size_t valid_count    = 0;

  for ( size_t i = 0; i < len; i++ )
  {
    if ( data[i] != 0 )
    {
      sum_reciprocal += 1.0 / data[i];
      valid_count++;
    }
  }

  if ( valid_count == 0 )
  {
    return 0.0;
  }

  return valid_count / sum_reciprocal;
}

double c_geo_mean ( const double *data, size_t len )
{
  double product = 1.0;

  for ( size_t i = 0; i < len; i++ )
  {
    if ( data[i] > 0 )
    {
      product *= data[i];
    }
  }

  return pow ( product, 1.0 / len );
}

double c_range ( const double *data, size_t len )
{
  double min_val = data[0];
  double max_val = data[0];

  for ( size_t i = 1; i < len; i++ )
  {
    if ( data[i] < min_val )
    {
      min_val = data[i];
    }

    if ( data[i] > max_val )
    {
      max_val = data[i];
    }
  }

  return max_val - min_val;
}

double c_percentile ( double *data, size_t len, double percentile )
{
  double k = ( percentile / 100.0 ) * ( len - 1 );

  size_t f = ( size_t )floor ( k );
  size_t c = ( size_t )ceil ( k );

  return data[f] + ( k - f ) * ( data[c] - data[f] );
}

double c_iqr ( double *data, size_t len )
{
  double q1 = c_percentile ( data, len, 25.0 );
  double q3 = c_percentile ( data, len, 75.0 );

  return q3 - q1;
}

double c_exp_value ( const double *data, size_t len )
{
  double sum = 0.0;

  for ( size_t i = 0; i < len; i++ )
  {
    sum += data[i];
  }

  return sum / len;
}

double c_mad ( double *data, size_t len, double median )
{
  double *absolute_deviation = ( double * )malloc ( len * sizeof ( double ) );

  for ( size_t i = 0; i < len; i++ )
  {
    absolute_deviation[i] = fabs ( data[i] - median );
  }

  qsort ( absolute_deviation, len, sizeof ( double ), ( int ( * ) ( const void *, const void * ) )strcmp );
  double mad = c_median ( absolute_deviation, len );
  free ( absolute_deviation );

  return mad;
}

double c_rms ( const double *data, size_t len )
{
  double sum_of_squares = 0.0;

  for ( size_t i = 0; i < len; i++ )
  {
    sum_of_squares += data[i] * data[i];
  }

  return sqrt ( sum_of_squares / len );
}

double c_mse ( const double *data, size_t len, double mean )
{
  double sum_of_squares = 0.0;

  for ( size_t i = 0; i < len; i++ )
  {
    sum_of_squares += ( data[i] - mean ) * ( data[i] - mean );
  }

  return sum_of_squares / len;
}

double c_mae ( const double *data, size_t len )
{
  double sum_of_absolute_errors = 0.0;

  for ( size_t i = 0; i < len; i++ )
  {
    sum_of_absolute_errors += fabs ( data[i] );
  }

  return sum_of_absolute_errors / len;
}

double c_zskewness ( const double *data, size_t len, double mean )
{
  double sum_cubed_deviation = 0.0;
  double standard_deviation  = c_std_deviation ( data, len, mean );

  for ( size_t i = 0; i < len; i++ )
  {
    sum_cubed_deviation += pow ( data[i] - mean, 3 );
  }

  return ( sum_cubed_deviation / len ) / pow ( standard_deviation, 3 );
}

double c_mskewness ( double *data, size_t len, double median )
{
  double sum_cubed_deviation = 0.0;

  for ( size_t i = 0; i < len; i++ )
  {
    sum_cubed_deviation += pow ( data[i] - median, 3 );
  }

  double mean               = c_mean ( data, len );
  double standard_deviation = c_std_deviation ( data, len, mean );

  return ( sum_cubed_deviation / len ) / pow ( standard_deviation, 3 );
}

double c_kurtosis ( const double *data, size_t len, double mean, double standard_deviation )
{
  double sum_fourth_deviation = 0.0;

  for ( size_t i = 0; i < len; i++ )
  {
    sum_fourth_deviation += pow ( data[i] - mean, 4 );
  }

  return ( sum_fourth_deviation / len ) / pow ( standard_deviation, 4 ) - 3.0;
}

double c_cv ( double mean, double standard_deviation )
{
  return standard_deviation / mean;
}

double c_max_deviation ( const double *data, size_t len, double mean )
{
  double max_deviation = 0.0;

  for ( size_t i = 0; i < len; i++ )
  {
    double deviation = fabs ( data[i] - mean );

    if ( deviation > max_deviation )
    {
      max_deviation = deviation;
    }
  }

  return max_deviation;
}

double c_bin_entropy ( const double *data, size_t len ) // 수정된 정의
{
  double entropy = 0.0;

  for ( size_t i = 0; i < len; i++ )
  {
    if ( data[i] > 0 )
    {
      entropy -= data[i] * log2 ( data[i] );
    }
  }

  return entropy / len;
}

double c_rmsle ( const double *data, size_t len )
{
  double sum_of_squares = 0.0;

  for ( size_t i = 0; i < len; i++ )
  {
    sum_of_squares += pow ( log ( data[i] + 1 ), 2 );
  }

  return sqrt ( sum_of_squares / len );
}

void c_quartiles ( double *data, size_t len, double *q1, double *q2, double *q3 )
{
  qsort ( data, len, sizeof ( double ), ( int ( * ) ( const void *, const void * ) )strcmp );

  *q2 = c_median ( data, len );

  if ( len % 2 == 0 )
  {
    *q1 = c_median ( data, len / 2 );
    *q3 = c_median ( data + len / 2, len / 2 );
  }
  else
  {
    *q1 = c_median ( data, len / 2 );
    *q3 = c_median ( data + len / 2 + 1, len / 2 );
  }
}

double c_mead ( double *data, size_t len )
{
  double  median             = c_median ( data, len );
  double *absolute_deviation = ( double * )malloc ( len * sizeof ( double ) );

  for ( size_t i = 0; i < len; i++ )
  {
    absolute_deviation[i] = fabs ( data[i] - median );
  }

  double mead = c_median ( absolute_deviation, len );
  free ( absolute_deviation );

  return mead;
}

double c_percent_range ( const double *data, size_t len )
{
  double min_val = data[0];
  double max_val = data[0];

  for ( size_t i = 1; i < len; i++ )
  {
    if ( data[i] < min_val )
    {
      min_val = data[i];
    }

    if ( data[i] > max_val )
    {
      max_val = data[i];
    }
  }

  return ( ( max_val - min_val ) / max_val ) * 100.0;
}

int main ()
{
  int                server_fd, client_socket;
  struct sockaddr_in address;
  int                addrlen = sizeof ( address );
  int                opt     = 1;

  pthread_mutex_init ( &Index_lock, NULL );

  if ( ( server_fd = socket ( AF_INET, SOCK_STREAM, 0 ) ) == 0 )
  {
    error_log ( "main - failed to socket init" );
  }

  if ( setsockopt ( server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof ( opt ) ) )
  {
    error_log ( "main - failed to setsockopt." );
  }

  address.sin_family      = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port        = htons ( PORT );

  if ( bind ( server_fd, ( struct sockaddr * )&address, sizeof ( address ) ) < 0 )
  {
    error_log ( "main - failed to socket bind." );
  }

  if ( listen ( server_fd, 3 ) < 0 )
  {
    error_log ( "main - failed to socket listen." );
  }

  printf ( "TicTacDB Listening now : %d \n", PORT );

  while ( 1 )
  {
    if ( ( client_socket = accept ( server_fd, ( struct sockaddr * )&address, ( socklen_t * )&addrlen ) ) < 0 )
    {
      error_log ( "main - failed to client accept" );
    }

    pthread_t thread_id;
    if ( pthread_create ( &thread_id, NULL, handle_client, ( void * )( intptr_t )client_socket ) != 0 )
    {
      error_log ( "main - failed to create pthread" );
    }
    pthread_detach ( thread_id );
  }

  free_index_entries ( Tag_index );
  pthread_mutex_destroy ( &Index_lock );
  return 0;
}
