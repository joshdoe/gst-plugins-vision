#ifndef _GET_UNIX_NS_H_
#define _GET_UNIX_NS_H_

#include <gmodule.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif


#ifdef _WIN32
typedef struct _MYFILETIME
{
  guint32 dwLowDateTime;
  guint32 dwHighDateTime;
} MYFILETIME;
typedef void (*GetSystemTimeFunc) (MYFILETIME * lpSystemTimeAsFileTime);

static guint64
get_unix_ns ()
{
  MYFILETIME ftime;
  LARGE_INTEGER ltime;
  static GetSystemTimeFunc time_func = NULL;
  if (!time_func) {
    GModule *module;
    module = g_module_open ("Kernel32.dll", G_MODULE_BIND_LAZY);
    if (module) {
      if (!g_module_symbol (module, "GetSystemTimePreciseAsFileTime",
              (gpointer *) & time_func) || time_func == NULL) {
        GST_WARNING
            ("Couldn't find GetSystemTimePreciseAsFileTime, falling back to GetSystemTimeAsFileTime");
        if (!g_module_symbol (module, "GetSystemTimeAsFileTime",
                (gpointer *) & time_func) || time_func == NULL) {
          GST_WARNING
              ("Couldn't find GetSystemTimeAsFileTime, something is very wrong");
        }
      }
    }
  }
  //GetSystemTimePreciseAsFileTime(&ftime);
  time_func (&ftime);
  ltime.HighPart = ftime.dwHighDateTime;
  ltime.LowPart = ftime.dwLowDateTime;
  ltime.QuadPart -= 11644473600000 * 10000;
  return ltime.QuadPart * 100;
}
#endif /* _WIN32 */


#ifdef __unix__
static guint64
get_unix_ns ()
{
  struct timespec spec;

  clock_gettime (CLOCK_REALTIME, &spec);
  return (guint64) spec.tv_sec * 1000000000L + (guint64) spec.tv_nsec;
}
#endif /* __unix__ */


#endif /* _GET_UNIX_NS_H_ */
