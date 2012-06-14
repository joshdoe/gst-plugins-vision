#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstsensorfx3dnoise.h"

#define GST_CAT_DEFAULT gst_sensorfx_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);


/* Register filters that make up the gstsfx plugin */
static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "sensorfx", 0, "sensorfx");

  GST_DEBUG ("plugin_init");

  if (!gst_element_register (plugin, "sfx3dnoise", GST_RANK_NONE,
          GST_TYPE_SFX3DNOISE)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "sensorfx",
    "Filters to simulate the effects of real sensors",
    plugin_init, VERSION, GST_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN);
