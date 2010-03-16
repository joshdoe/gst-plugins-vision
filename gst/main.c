#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvideolevels.h"

#define GST_CAT_DEFAULT gst_nvl_gstnvl_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

/* Register filters that make up the gstgl plugin */
static gboolean
plugin_init ( GstPlugin * plugin )
{
    GST_DEBUG_CATEGORY_INIT ( gst_nvl_gstnvl_debug, "nvl", 0, "nvl" );

    GST_CAT_INFO ( gst_nvl_gstnvl_debug, "plugin_init" );


    GST_CAT_INFO ( gst_nvl_gstnvl_debug, "registering videolevels element" );
    if ( !gst_element_register ( plugin, "videolevels", GST_RANK_NONE,
        GST_TYPE_VIDEOLEVELS ) ) {
        return FALSE;
    }
   
    return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
                   GST_VERSION_MINOR,
                   "nvl",
                   "Plugins of interest to NVL",
                   plugin_init,
                   VERSION,
                   GST_LICENSE,
                   GST_PACKAGE_NAME,
                   GST_PACKAGE_ORIGIN
                   );
