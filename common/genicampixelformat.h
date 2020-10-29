/* GStreamer
 * Copyright (c) 2018 outside US, United States Government, Joshua M. Doe <oss@nvl.army.mil>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include <gst/video/video-format.h>

#define GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER8(format)                     \
   "video/x-bayer, "                                       \
  "format = (string) " format ", "                     \
  "width = " GST_VIDEO_SIZE_RANGE ", "                     \
  "height = " GST_VIDEO_SIZE_RANGE ", "                    \
  "framerate = " GST_VIDEO_FPS_RANGE

#define GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16(format,endianness)         \
  "video/x-bayer, "                                        \
  "format = (string) " format ", "                     \
  "endianness = (int) " endianness ", "                \
  "bpp = (int) {16, 14, 12, 10}, "                         \
  "width = " GST_VIDEO_SIZE_RANGE ", "                     \
  "height = " GST_VIDEO_SIZE_RANGE ", "                    \
  "framerate = " GST_VIDEO_FPS_RANGE

typedef struct
{
    const char *pixel_format;
    const char *pixel_format_spaced;
    int endianness;
    const char *gst_caps_string;
    int bpp;
    int depth;
    int row_multiple;
} GstGenicamPixelFormatInfo;

GstGenicamPixelFormatInfo gst_genicam_pixel_format_infos[] = {
  {"Mono8", "Mono 8", 0, GST_VIDEO_CAPS_MAKE ("GRAY8"), 8, 8, 4}
  ,
  {"Mono10", "Mono 10", G_LITTLE_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_LE"), 10, 16, 4}
  ,
  {"Mono10", "Mono 10", G_BIG_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_BE"), 10, 16, 4}
  ,
  {"Mono12", "Mono 12", G_LITTLE_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_LE"), 12, 16, 4}
  ,
  {"Mono12", "Mono 12", G_BIG_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_BE"), 12, 16, 4}
  ,
  {"Mono14", "Mono 14", G_LITTLE_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_LE"), 14, 16, 4}
  ,
  {"Mono14", "Mono 14", G_BIG_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_BE"), 14, 16, 4}
  ,
  {"Mono16", "Mono 16", G_LITTLE_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_LE"), 16, 16, 4}
  ,
  {"Mono16", "Mono 16", G_BIG_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_BE"), 16, 16, 4}
  ,
  {"RGB8", "RGB 8", 0, GST_VIDEO_CAPS_MAKE ("RGB"), 24, 24, 4}
  ,
  {"BGR8", "BGR 8", 0, GST_VIDEO_CAPS_MAKE ("BGR"), 24, 24, 4}
  ,
  {"RGBa8", "RGBa 8", 0, GST_VIDEO_CAPS_MAKE ("RGBA"), 32, 32, 4}
  ,
  {"BGRa8", "BGRa 8", 0, GST_VIDEO_CAPS_MAKE ("BGRA"), 32, 32, 4}
  ,
  {"BGRA8Packed", "BGRA 8 Packed", 0, GST_VIDEO_CAPS_MAKE ("BGRA"), 32, 32, 4}
  ,
  {"YUV422Packed", "YUV 422 Packed", 0, GST_VIDEO_CAPS_MAKE ("UYVY"), 16, 16, 4}
  ,
  {"YCbCr422_8", "YCbCr422_8", 0, GST_VIDEO_CAPS_MAKE ("YUY2"), 16, 16, 4}
  ,
  {"BayerBG8", "Bayer BG 8", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER8 ("bggr"), 8, 8, 1}
  ,
  {"BayerGR8", "Bayer GR 8", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER8 ("grbg"), 8, 8, 1}
  ,
  {"BayerRG8", "Bayer RG 8", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER8 ("rggb"), 8, 8, 1}
  ,
  {"BayerGB8", "Bayer GB 8", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER8 ("gbrg"), 8, 8, 1}
  ,
  //TODO: make sure we use standard caps strings for 16-bit Bayer
  {"BayerBG10", "Bayer BG 10", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("bggr16", "1234"), 10, 16, 1}
  ,
  {"BayerGR10", "Bayer GR 10", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("grbg16", "1234"), 10, 16, 1}
  ,
  {"BayerRG10", "Bayer RG 10", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("rggb16", "1234"), 10, 16, 1}
  ,
  {"BayerGB10", "Bayer GB 10", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("gbrg16", "1234"), 10, 16, 1}
  ,
  {"BayerBG12", "Bayer BG 12", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("bggr16", "1234"), 12, 16, 1}
  ,
  {"BayerGR12", "Bayer GR 12", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("grbg16", "1234"), 12, 16, 1}
  ,
  {"BayerRG12", "Bayer RG 12", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("rggb16", "1234"), 12, 16, 1}
  ,
  {"BayerGB12", "Bayer GB 12", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("gbrg16", "1234"), 12, 16, 1}
  ,
  {"BayerBG14", "Bayer BG 14", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("bggr16", "1234"), 14, 16, 1}
  ,
  {"BayerGR14", "Bayer GR 14", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("grbg16", "1234"), 14, 16, 1}
  ,
  {"BayerRG14", "Bayer RG 14", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("rggb16", "1234"), 14, 16, 1}
  ,
  {"BayerGB14", "Bayer GB 14", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("gbrg16", "1234"), 14, 16, 1}
  ,
  {"BayerBG16", "Bayer BG 16", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("bggr16", "1234"), 16, 16, 1}
  ,
  {"BayerGR16", "Bayer GR 16", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("grbg16", "1234"), 16, 16, 1}
  ,
  {"BayerRG16", "Bayer RG 16", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("rggb16", "1234"), 16, 16, 1}
  ,
  {"BayerGB16", "Bayer GB 16", 0, GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16 ("gbrg16", "1234"), 16, 16, 1}
  ,
  {"JPEG", "JPEG", 0, "image/jpeg", 8, 8, 1}
  ,
  /* Formats from Basler */
  {"YUV422_YUYV_Packed", "YUV422_YUYV_Packed", 0, GST_VIDEO_CAPS_MAKE ("YUY2"), 16, 16, 4}
};

int strcmp_ignore_whitespace (const char *s1, const char *s2)
{
  const char *p1 = s1, *p2 = s2;

  while (TRUE) {
      /* skip all whitespace characters in both strings */
      while (g_ascii_isspace(*p1)) ++p1;
      if (!*p1) break;

      while (g_ascii_isspace(*p2)) ++p2;
      if (!*p2) return 1;

      if (*p2 > *p1) return -1;
      if (*p1 > *p2) return 1;

      ++p1;
      ++p2;
  }

  if (*p2) return -1;

  return 0;
}

int strncasecmp_ignore_whitespace (const char *s1, const char *s2)
{
  const char *p1 = s1, *p2 = s2;

  while (TRUE) {
    gchar c1, c2;

    /* skip all whitespace characters in both strings */
    while (g_ascii_isspace(*p1)) ++p1;
    if (!*p1) break;

    while (g_ascii_isspace(*p2)) ++p2;
    if (!*p2) return 1;

    c1 = g_ascii_tolower(*p1);
    c2 = g_ascii_tolower(*p2);

    if (c2 > c1) return -1;
    if (c1 > c2) return 1;

    ++p1;
    ++p2;
  }

  if (*p2) return -1;

  return 0;
}

static const GstGenicamPixelFormatInfo *
gst_genicam_pixel_format_get_info (const char *pixel_format, int endianness)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (gst_genicam_pixel_format_infos); i++) {
    GstGenicamPixelFormatInfo *info = &gst_genicam_pixel_format_infos[i];
    if (strcmp_ignore_whitespace (pixel_format, info->pixel_format_spaced) == 0 &&
        (info->endianness == endianness || info->endianness == 0))
      return info;
  }

  GST_WARNING ("PixelFormat '%s' is not supported", pixel_format);
  return NULL;
}

static const char *
gst_genicam_pixel_format_to_caps_string (const char *pixel_format,
    int endianness)
{
  const GstGenicamPixelFormatInfo *info =
      gst_genicam_pixel_format_get_info (pixel_format, endianness);

  if (!info)
    return NULL;

  return info->gst_caps_string;
}

static const char *
gst_genicam_pixel_format_from_caps (const GstCaps * caps, int *endianness)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (gst_genicam_pixel_format_infos); i++) {
    GstCaps *super_caps;
    super_caps = gst_caps_from_string (gst_genicam_pixel_format_infos[i].gst_caps_string);
    if (gst_caps_is_subset (caps, super_caps)) {
      *endianness = gst_genicam_pixel_format_infos[i].endianness;
      return gst_genicam_pixel_format_infos[i].pixel_format;
    }
  }

  return NULL;
}

static int
gst_genicam_pixel_format_get_depth (const char *pixel_format,
    int endianness)
{
  const GstGenicamPixelFormatInfo *info =
      gst_genicam_pixel_format_get_info (pixel_format, endianness);

  if (!info)
    return 0;

  return info->depth;
}

static int
gst_genicam_pixel_format_get_stride (const char *pixel_format,
    int endianness, int width)
{
  return width * gst_genicam_pixel_format_get_depth (pixel_format,
      endianness) / 8;
}

static GstCaps *
gst_genicam_pixel_format_caps_from_pixel_format (const char *pixel_format,
    int endianness, int width, int height, int framerate_n, int framerate_d,
    int par_n, int par_d)
{
  const char *caps_string;
  GstCaps *caps;
  GstStructure *structure;

  GST_DEBUG
      ("Trying to create caps from: %s, endianness=%d, %dx%d, fps=%d/%d, par=%d/%d",
      pixel_format, endianness, width, height, framerate_n, framerate_d, par_n,
      par_d);

  caps_string =
      gst_genicam_pixel_format_to_caps_string (pixel_format, endianness);
  if (caps_string == NULL)
    return NULL;

  GST_DEBUG ("Got caps string: %s", caps_string);

  structure = gst_structure_from_string (caps_string, NULL);
  if (structure == NULL)
    return NULL;

  gst_structure_set (structure,
      "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height,
      "framerate", GST_TYPE_FRACTION, framerate_n, framerate_d,
      "pixel-aspect-ratio", GST_TYPE_FRACTION, par_n, par_d, NULL);

  if (g_str_has_prefix (pixel_format, "Bayer")) {
      const GstGenicamPixelFormatInfo *info = gst_genicam_pixel_format_get_info(pixel_format, endianness);
      g_assert (info);
      gst_structure_set(structure, "bpp", G_TYPE_INT, (gint)info->bpp, NULL);
  }

  caps = gst_caps_new_empty ();
  gst_caps_append_structure (caps, structure);

  caps = gst_caps_fixate (caps);

  return caps;
}

static GstCaps *
gst_genicam_pixel_format_caps_from_pixel_format_var (const char *pixel_format,
    int endianness, int width, int height)
{
  const char *caps_string;
  GstCaps *caps;
  GstStructure *structure;

  GST_DEBUG
      ("Trying to create caps from: %s, endianness=%d, %dx%d",
      pixel_format, endianness, width, height);

  caps_string =
      gst_genicam_pixel_format_to_caps_string (pixel_format, endianness);
  if (caps_string == NULL)
    return NULL;

  GST_DEBUG ("Got caps string: %s", caps_string);

  structure = gst_structure_from_string (caps_string, NULL);
  if (structure == NULL)
    return NULL;

  gst_structure_set (structure,
      "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height,
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

  if (g_str_has_prefix (pixel_format, "Bayer")) {
      const GstGenicamPixelFormatInfo *info = gst_genicam_pixel_format_get_info(pixel_format, endianness);
      g_assert (info);
      gst_structure_set(structure, "bpp", G_TYPE_INT, (gint)info->bpp, NULL);
  }

  caps = gst_caps_new_empty ();
  gst_caps_append_structure (caps, structure);

  return caps;
}