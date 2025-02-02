#ifndef EPOCHTIME_HPP
#define EPOCHTIME_HPP

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

class Epochtime
{
private:
  chrono::system_clock::time_point _time;

public:
  int _yyyy, _MM, _dd, _hh, _mm, _ss, _SSS;

  Epochtime ()
  {
    set_time ( 1970, 1, 1, 0, 0, 0 );
  }

  Epochtime ( int yyyy, int MM, int dd, int hh = 0, int mm = 0, int ss = 0 )
  {
    set_time ( yyyy, MM, dd, hh, mm, ss );
  }

  Epochtime ( long long epoch )
  {
    set_time ( epoch );
  }

  void set_time ( int yyyy, int MM, int dd, int hh = 0, int mm = 0, int ss = 0 )
  {
    _yyyy = yyyy;
    _MM = MM;
    _dd = dd;
    _hh = hh;
    _mm = mm;
    _ss = ss;
    _SSS = 0;

    tm t = 
    {
      .tm_year = yyyy - 1900,
      .tm_mon = MM - 1,
      .tm_mday = dd,
      .tm_hour = hh,
      .tm_min = mm,
      .tm_sec = ss
    };
    
    time_t tt = mktime ( &t );
    
    _time = chrono::system_clock::from_time_t ( tt );
  }

  void set_time ( long long epoch )
  {
    _time = chrono::system_clock::time_point ( chrono::milliseconds ( epoch ) );
    update ( epoch );
  }

  long long now ( int ms = 0 )
  {
    auto now = chrono::system_clock::now ();
    auto duration = chrono::duration_cast<chrono::milliseconds> ( now - _time );
    long long result = duration.count ();

    if ( ms == 0 )
    {
      result /= 1000;
    }

    return result;
  }

  string now_string ( int ms = 0 )
  {
    return to_string ( now ( ms ) );
  }

private:
  void update ( long long epoch )
  {
    time_t raw = epoch / 1000;
    
    _SSS = epoch % 1000;
    tm* t = gmtime ( &raw );

    _yyyy = t->tm_year + 1900;
    _MM = t->tm_mon + 1;
    _dd = t->tm_mday;
    _hh = t->tm_hour;
    _mm = t->tm_min;
    _ss = t->tm_sec;
  }
};

#endif
