#include <gst/gst.h>

#include "gstkayasink.h"
#include "gstkayasrc.h"

GST_DEBUG_CATEGORY_STATIC(kayaplugin_debug);
#define GST_CAT_DEFAULT kayaplugin_debug

static gboolean
plugin_init(GstPlugin* plugin)
{
    GST_DEBUG_CATEGORY_INIT(kayaplugin_debug, "kaya", 0,
        "debug category for kaya plugin");

    if (!gst_element_register(plugin, "kayasink", GST_RANK_NONE,
        gst_kayasink_get_type()))
        return FALSE;

    if (!gst_element_register(plugin, "kayasrc", GST_RANK_NONE,
        gst_kayasrc_get_type()))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    kaya,
    "Kaya plugin",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN);
