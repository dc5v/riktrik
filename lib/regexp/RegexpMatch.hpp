#ifndef REGEXP_MATCH_HPP
#define REGEXP_MATCH_HPP

#include <algorithm>
#include <codecvt>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _MSC_VER
#  include <intrin.h>
#else
#  include <x86intrin.h>
#endif

#ifdef _WIN32
#  ifdef REGEX_ENGINE_EXPORTS
#    define REGEX_API __declspec ( dllexport )
#  else
#    define REGEX_API __declspec ( dllimport )
#  endif
#else
#  define REGEX_API
#endif

using namespace std;

class REGEX_API RegexpMatch
{
public:
  RegexpMatch ( const string& pattern )
  {
    try
    {
      wstring wpattern = utf2Wstring ( pattern );
      init ( wpattern );
    }
    catch ( const exception& e )
    {
      throw runtime_error ( "RUNTIME_ERROR: " + string ( e.what () ) );
    }
  }

  RegexpMatch ( const wstring& pattern )
  {
    try
    {
      init ( pattern );
    }
    catch ( const exception& e )
    {
      throw runtime_error ( "RUNTIME_ERROR: " + string ( e.what () ) );
    }
  }

  ~RegexpMatch () = default;

  bool test ( const string& text ) const
  {
    try
    {
      return test ( utf2Wstring ( text ) );
    }
    catch ( const exception& e )
    {
      throw runtime_error ( "RUNTIME_ERROR test: " + string ( e.what () ) );
    }
  }

  bool test ( const wstring& text ) const
  {
    if ( text.empty () )
    {
      return false;
    }

    try
    {
      if ( simd_search )
      {
        return matchSIMD ( text );
      }

      return match ( text );
    }
    catch ( const exception& e )
    {
      throw runtime_error ( "RUNTIME_ERROR test: " + string ( e.what () ) );
    }
  }

private:
  struct State
  {
    bool is_final;
    map<wstring, shared_ptr<State>> mapper;
    State () : is_final ( false )
    {
    }
  };

  shared_ptr<State> root;
  wstring pattern;
  size_t pattern_len;
  bool simd_search;
  vector<__m256i> simd_pattern;

  void init ( const wstring& str )
  {
    if ( str.empty () )
    {
      throw invalid_argument ( "Pattern cannot be empty" );
    }

    pattern = str;
    pattern_len = str.length ();
    root = make_shared<State> ();

    build ( pattern );

    simd_search = hasOnlySimplePattern ( pattern ) && isAvailableSIMD ();

    if ( simd_search )
    {
      genPatternSIMD ();
    }
  }

  bool hasOnlySimplePattern ( const wstring& str ) const
  {
    return str.find_first_of ( L"[]\\*+?{}()^$" ) == wstring::npos;
  }

  bool isAvailableSIMD () const
  {
    /* clang-format off */
    #ifdef _MSC_VER
        int cpu_info[4];
        __cpuid ( cpu_info, 1 );
        return ( cpu_info[2] & ( 1 << 28 ) ) != 0;
    #else
        return __builtin_cpu_supports ( "avx2" );
    #endif
    /* clang-format on */
  }

  bool matchSIMD ( const wstring& str ) const
  {
    if ( pattern_len > str.length () )
    {
      return false;
    }

    const size_t text_len = str.length ();
    const size_t chunks = simd_pattern.size ();

    for ( size_t i = 0; i <= text_len - pattern_len; i += 16 )
    {
      bool potential_match = true;

      for ( size_t j = 0; j < chunks && potential_match; ++j )
      {
        alignas ( 32 ) wchar_t temp[16] = { 0 };
        size_t remain = min ( size_t ( 16 ), text_len - i );

        copy_n ( str.begin () + i, remain, temp );

        __m256i text_chunk = _mm256_load_si256 ( reinterpret_cast<const __m256i*> ( temp ) );
        __m256i cmp = _mm256_cmpeq_epi16 ( text_chunk, simd_pattern[j] );
        unsigned int mask = _mm256_movemask_epi8 ( cmp );

        if ( mask != 0xFFFFFFFF )
        {
          potential_match = false;
        }
      }

      if ( potential_match && verify ( str, i ) )
      {
        return true;
      }
    }

    return false;
  }

  bool verify ( const wstring& str, size_t pos ) const
  {
    if ( pos + pattern_len > str.length () )
    {
      return false;
    }

    return equal ( pattern.begin (), pattern.end (), str.begin () + pos );
  }

  bool match ( const wstring& str ) const
  {
    auto now = root;
    size_t pos = 0;

    while ( pos < str.length () )
    {
      bool matched = false;

      for ( const auto& transition : now->mapper )
      {
        if ( matchePatterns ( str, pos, transition.first ) )
        {
          pos += getMatchLength ( transition.first );
          now = transition.second;
          matched = true;

          break;
        }
      }

      if ( !matched )
      {
        if ( now->is_final )
        {
          return true;
        }

        if ( pos == 0 )
        {
          pos++;
          now = root;
        }
        else
        {
          return false;
        }
      }
    }

    return now->is_final;
  }

  void build ( const wstring& str )
  {
    auto now = root;
    wstring token;

    for ( size_t i = 0; i < str.length (); ++i )
    {
      token.clear ();

      if ( str[i] == L'\\' && i + 1 < str.length () )
      {
        token = str.substr ( i, 2 );
        i++;
      }
      else if ( str[i] == L'[' )
      {
        size_t end = str.find ( L']', i );

        if ( end != wstring::npos )
        {
          token = str.substr ( i, end - i + 1 );
          i = end;
        }
        else
        {
          throw runtime_error ( "RUNTIME_ERROR: [...]" );
        }
      }
      else
      {
        token = str[i];
      }

      auto next = make_shared<State> ();

      now->mapper[token] = next;
      now = next;
    }

    now->is_final = true;
  }

  bool matchePatterns ( const wstring& str, size_t pos, const wstring& pattern ) const
  {
    if ( pattern.empty () || pos >= str.length () )
    {
      return false;
    }

    if ( pattern[0] == L'\\' )
    {
      if ( pattern.length () < 2 )
      {
        throw runtime_error ( "RUNTIME_ERROR: escape" );
      }
      switch ( pattern[1] )
      {
        case L'd':
          return iswdigit ( str[pos] );
        case L'w':
          return iswalnum ( str[pos] ) || str[pos] == L'_';
        case L's':
          return iswspace ( str[pos] );
        default:
          return str[pos] == pattern[1];
      }
    }


    if ( pattern[0] == L'[' )
    {
      return matcheCharacterClasses ( str[pos], pattern );
    }


    return str[pos] == pattern[0];
  }

  bool matcheCharacterClasses ( wchar_t c, const wstring& pattern ) const
  {
    if ( pattern.length () < 3 )
    {
      throw runtime_error ( "RUNTIME_ERROR: character class" );
    }

    bool negate = pattern[1] == L'^';
    size_t start = negate ? 2 : 1;
    bool matched = false;

    for ( size_t i = start; i < pattern.length () - 1; ++i )
    {

      if ( pattern[i] == L'-' && i > start && i + 1 < pattern.length () - 1 )
      {
        if ( c >= pattern[i - 1] && c <= pattern[i + 1] )
        {
          matched = true;
          break;
        }
        i++;
      }

      else if ( c == pattern[i] )
      {
        matched = true;
        break;
      }
    }

    return negate ? !matched : matched;
  }

  size_t getMatchLength ( const wstring& pattern ) const
  {
    if ( pattern[0] == L'\\' )
      return 1;
    if ( pattern[0] == L'[' )
      return 1;
    return pattern.length ();
  }

  void genPatternSIMD ()
  {
    simd_pattern.clear ();

    for ( size_t i = 0; i < pattern.length (); i += 16 )
    {
      alignas ( 32 ) wchar_t temp[16] = { 0 };
      size_t remain = min ( size_t ( 16 ), pattern.length () - i );
      copy_n ( pattern.begin () + i, remain, temp );

      __m256i chunk = _mm256_load_si256 ( reinterpret_cast<const __m256i*> ( temp ) );
      simd_pattern.push_back ( chunk );
    }
  }

  static wstring utf2Wstring ( const string& str )
  {
    wstring_convert<codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes ( str );
  }

  static string wstring2Utf ( const wstring& str )
  {
    wstring_convert<codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes ( str );
  }
};

#endif