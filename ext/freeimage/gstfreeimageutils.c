#include <gst/video/video.h>

#include "gstfreeimageutils.h"

GstCaps *
gst_freeimageutils_caps_from_dib (FIBITMAP * dib, gint fps_n, gint fps_d)
{
  FREE_IMAGE_TYPE image_type;
  guint width, height, bpp;
  gint video_format = -1;
  GstCaps * caps = NULL;
  gint endianness;

  if (dib == NULL)
    return NULL;

  /* Get bits per channel */
  bpp = FreeImage_GetBPP (dib);

  /* Get image type */
  image_type = FreeImage_GetImageType (dib);

  /* Get width and height */
  width = FreeImage_GetWidth (dib);
  height = FreeImage_GetHeight (dib);

  GST_LOG ("Image_type=%d, %dx%dx%d", image_type, width, height, bpp);

  switch (image_type) {
    case FIT_BITMAP:
      if (bpp == 24) {
        if (FreeImage_GetRedMask (dib) == GST_VIDEO_BYTE1_MASK_24_INT &&
          FreeImage_GetGreenMask (dib) == GST_VIDEO_BYTE2_MASK_24_INT &&
          FreeImage_GetBlueMask (dib) == GST_VIDEO_BYTE3_MASK_24_INT) {
            video_format = GST_VIDEO_FORMAT_RGB;
        }
        else if (FreeImage_GetRedMask (dib) == GST_VIDEO_BYTE3_MASK_24_INT &&
          FreeImage_GetGreenMask (dib) == GST_VIDEO_BYTE2_MASK_24_INT &&
          FreeImage_GetBlueMask (dib) == GST_VIDEO_BYTE1_MASK_24_INT) {
            video_format = GST_VIDEO_FORMAT_BGR;
        }
        else {
          return NULL;
        }
      }
      else if (bpp == 32) {
        if (FreeImage_GetRedMask (dib) == GST_VIDEO_BYTE1_MASK_32_INT &&
          FreeImage_GetGreenMask (dib) == GST_VIDEO_BYTE2_MASK_32_INT &&
          FreeImage_GetBlueMask (dib) == GST_VIDEO_BYTE3_MASK_32_INT) {
            video_format = GST_VIDEO_FORMAT_RGBA;
        }
        else if (FreeImage_GetRedMask (dib) == GST_VIDEO_BYTE3_MASK_32_INT &&
          FreeImage_GetGreenMask (dib) == GST_VIDEO_BYTE2_MASK_32_INT &&
          FreeImage_GetBlueMask (dib) == GST_VIDEO_BYTE1_MASK_32_INT) {
            video_format = GST_VIDEO_FORMAT_BGRA;
        }
        else {
          return NULL;
        }
      }
      else {
        return NULL;
      }

      /* We could not find a supported format */
      if (video_format == -1) {
        caps = NULL;
      }
      else {
        caps = gst_video_format_new_caps (video_format, width, height,
          fps_n, fps_d, 1, 1);
      }
      break;
    case FIT_UINT16:
      endianness = G_BYTE_ORDER;

      caps = gst_caps_new_simple ("video/x-raw-gray",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "bpp", G_TYPE_INT, 16,
        "depth", G_TYPE_INT, 16,
        "endianness", G_TYPE_INT, endianness,
        "framerate", GST_TYPE_FRACTION, fps_n, fps_d,
        NULL);
      break;
    case FIT_INT16:
      endianness = G_BYTE_ORDER;

      caps = gst_caps_new_simple ("video/x-raw-gray",
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "bpp", G_TYPE_INT, 16,
        "depth", G_TYPE_INT, 16,
        "endianness", G_TYPE_INT, endianness,
        "framerate", GST_TYPE_FRACTION, fps_n, fps_d,
        "signed", G_TYPE_BOOLEAN, TRUE,
        NULL);
      break;
    default:
      caps = NULL;
  }
  return caps;
}

GstCaps *
gst_freeimageutils_caps_from_freeimage_format (FREE_IMAGE_FORMAT fif)
{
  GstCaps * caps = gst_caps_new_empty ();

  if (FreeImage_FIFSupportsExportType (fif, FIT_BITMAP)) {
    if (FreeImage_FIFSupportsExportBPP (fif, 1) ||
      FreeImage_FIFSupportsExportBPP (fif, 4) ||
      FreeImage_FIFSupportsExportBPP (fif, 8) ||
      FreeImage_FIFSupportsExportBPP (fif, 24)) {
        if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
          gst_caps_append (caps, gst_caps_from_string (GST_VIDEO_CAPS_BGR));
        }
        else {
          gst_caps_append (caps, gst_caps_from_string (GST_VIDEO_CAPS_RGB));
        }
        
    }
    if (FreeImage_FIFSupportsExportBPP (fif, 16)) {
      gst_caps_append (caps, gst_caps_from_string (GST_VIDEO_CAPS_RGB_15));
      gst_caps_append (caps, gst_caps_from_string (GST_VIDEO_CAPS_RGB_16));
    }
    if (FreeImage_FIFSupportsExportBPP (fif, 32)) {
      if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        gst_caps_append (caps, gst_caps_from_string (GST_VIDEO_CAPS_BGRA));
      }
      else {
        gst_caps_append (caps, gst_caps_from_string (GST_VIDEO_CAPS_RGBA));
      }
      
    }
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_UINT16)) {
    if (G_BYTE_ORDER == G_BIG_ENDIAN)
      gst_caps_append (caps, gst_caps_from_string (
      GST_VIDEO_CAPS_GRAY16 ("BIG_ENDIAN")));
    else
      gst_caps_append (caps, gst_caps_from_string (
      GST_VIDEO_CAPS_GRAY16 ("LITTLE_ENDIAN")));
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_INT16)) {
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_UINT32)) {
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_INT32)) {
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_FLOAT)) {
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_DOUBLE)) {
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_COMPLEX)) {
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_RGB16)) {
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_RGBA16)) {
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_RGBF)) {
  }
  if (FreeImage_FIFSupportsExportType (fif, FIT_RGBAF)) {
  }

  /* non-standard format, we'll try and convert to RGB */
  if (gst_caps_get_size (caps) == 0) {
    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
      gst_caps_append (caps, gst_caps_from_string (GST_VIDEO_CAPS_BGR));
      gst_caps_append (caps, gst_caps_from_string (GST_VIDEO_CAPS_BGRA));
    }
    else {
      gst_caps_append (caps, gst_caps_from_string (GST_VIDEO_CAPS_RGB));
      gst_caps_append (caps, gst_caps_from_string (GST_VIDEO_CAPS_RGBA));
    }
  }

  return caps;
}

gboolean
gst_freeimageutils_parse_caps (const GstCaps * caps, FREE_IMAGE_TYPE * type,
    gint * width, gint * height, gint * bpp, guint32 * red_mask,
    guint32 * green_mask, guint32 * blue_mask)
{
  GstStructure * s;

  s = gst_caps_get_structure (caps, 0);

  gst_structure_get_int (s, "width", width);
  gst_structure_get_int (s, "height", height);
  gst_structure_get_int (s, "bpp", bpp);

  if (g_strcmp0 (gst_structure_get_name (s), "video/x-raw-rgb") == 0) {
    *type = FIT_BITMAP;
    gst_structure_get_int (s, "red_mask", red_mask);
    gst_structure_get_int (s, "green_mask", green_mask);
    gst_structure_get_int (s, "blue_mask", blue_mask);
  }
  else if (g_strcmp0 (gst_structure_get_name (s), "video/x-raw-gray") == 0) {
    gboolean is_signed;
    if (!gst_structure_get_boolean (s, "signed", &is_signed))
      is_signed = FALSE;

    if (*bpp == 8)
      *type = FIT_BITMAP; /* need to create palette for this later */
    else if (*bpp == 16 && is_signed == FALSE)
      *type = FIT_UINT16;
    else if (*bpp == 16 && is_signed == TRUE)
      *type = FIT_INT16;
    else
      return FALSE;
  }

  return TRUE;
}