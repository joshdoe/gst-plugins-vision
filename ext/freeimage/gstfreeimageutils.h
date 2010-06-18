#ifndef __GST_FREEIMAGEUTILS_H__
#define __GST_FREEIMAGEUTILS_H__

#include <gst/gst.h>
#include <FreeImage.h>

GstCaps * gst_freeimageutils_caps_from_dib (FIBITMAP * dib,
    gint fps_n, gint fps_d);
GstCaps * gst_freeimageutils_caps_from_freeimage_format (
    FREE_IMAGE_FORMAT fif);

gboolean gst_freeimageutils_parse_caps (const GstCaps * caps,
    FREE_IMAGE_TYPE * type, gint * width, gint * height, gint * bpp,
    unsigned * red_mask, unsigned * green_mask, unsigned * blue_mask);
 
#endif // __GST_FREEIMAGEUTILS_H__