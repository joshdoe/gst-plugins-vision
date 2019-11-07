#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "PvVersion.h"
#if VERSION_MAJOR >= 6
#include "gstpleorasink.h"
#endif

#include "gstpleorasrc.h"

#define GST_CAT_DEFAULT gst_pleora_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "pleora", 0,
      "debug category for pleora plugin");

#if VERSION_MAJOR >= 6
  /* eBUS 6 is needed for this sink element */
  if (!gst_element_register (plugin, "pleorasink", GST_RANK_NONE,
      gst_pleorasink_get_type ())) {
          return FALSE;
  }
#endif

  if (!gst_element_register (plugin, "pleorasrc", GST_RANK_NONE,
      gst_pleorasrc_get_type ())) {
          return FALSE;
  }

  return TRUE;
}

#define PLUGIN_NAME G_PASTE(pleora, VERSION_MAJOR)

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    PLUGIN_NAME,
    "Pleora eBUS video elements",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
