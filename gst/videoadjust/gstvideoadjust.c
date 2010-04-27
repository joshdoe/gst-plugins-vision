#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvideolevels.h"

#define GST_CAT_DEFAULT gst_nvl_gstvideoadjust_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);


/* Register filters that make up the gstgl plugin */
static gboolean
plugin_init (GstPlugin * plugin)
{
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "videoadjust", 0, "videoadjust");

    GST_DEBUG ("plugin_init");

    GST_CAT_INFO (GST_CAT_DEFAULT, "registering videolevels element");

    if ( !gst_element_register (plugin, "videolevels", GST_RANK_NONE,
        GST_TYPE_VIDEOLEVELS)) {
        return FALSE;
    }
   
    return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
                   GST_VERSION_MINOR,
                   "videoadjust",
                   "Filters that apply transform from 16-bit to 8-bit video",
                   plugin_init,
                   VERSION,
                   GST_LICENSE,
                   GST_PACKAGE_NAME,
                   GST_PACKAGE_ORIGIN
                   );
