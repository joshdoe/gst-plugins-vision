#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstedtpdvsink.h"

#define GST_CAT_DEFAULT gst_gstedt_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "edt", 0,
      "debug category for EDT elements");

  if (!gst_element_register (plugin, "edtpdvsink", GST_RANK_NONE,
          GST_TYPE_EDT_PDV_SINK)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    edt,
    "EDT PDV elements",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
