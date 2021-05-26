#ifndef _GET_UNIX_NS_H_
#define _GET_UNIX_NS_H_

#include <gmodule.h>

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

#endif /* _GET_UNIX_NS_H_ */
