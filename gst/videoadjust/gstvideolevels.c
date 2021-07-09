/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2003 Arwed v. Merkatz <v.merkatz@gmx.net>
 * Copyright (C) 2006 Mark Nauwelaerts <manauw@skynet.be>
 * Copyright (C) 2010 United States Government, Joshua M. Doe <oss@nvl.army.mil>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
* SECTION:element-videolevels
*
* Convert grayscale video from one bpp/depth combination to another.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch videotestsrc ! videolevels ! ffmpegcolorspace ! autovideosink
* ]|
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstvideolevels.h"
#include "genicampixelformat.h"

#include <gst/video/video.h>

/* GstVideoLevels signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_LOWIN,
  PROP_HIGHIN,
  PROP_LOWOUT,
  PROP_HIGHOUT,
  PROP_AUTO,
  PROP_INTERVAL,
  PROP_LOWER_SATURATION,
  PROP_UPPER_SATURATION,
  PROP_ROI_X,
  PROP_ROI_Y,
  PROP_ROI_WIDTH,
  PROP_ROI_HEIGHT,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

#define DEFAULT_PROP_LOWIN  0
#define DEFAULT_PROP_HIGHIN  65535
#define DEFAULT_PROP_LOWOUT  0
#define DEFAULT_PROP_HIGHOUT  255
#define DEFAULT_PROP_AUTO 0
#define DEFAULT_PROP_INTERVAL (GST_SECOND / 2)
#define DEFAULT_PROP_LOW_SAT 0.01
#define DEFAULT_PROP_HIGH_SAT 0.01
#define DEFAULT_PROP_ROI_X -1
#define DEFAULT_PROP_ROI_Y -1
#define DEFAULT_PROP_ROI_WIDTH 0
#define DEFAULT_PROP_ROI_HEIGHT 0

/* the capabilities of the inputs and outputs */
static GstStaticPadTemplate gst_videolevels_src_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ GRAY16_LE, GRAY16_BE, GRAY8 }") ";"
        GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER8 ("{ bggr, grbg, rggb, gbrg }") ";"
        GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16
        ("{ bggr16, grbg16, rggb16, gbrg16 }", "{1234, 4321}")
    )
    );

static GstStaticPadTemplate gst_videolevels_sink_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("GRAY8") ";"
        GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER8 ("{ bggr, grbg, rggb, gbrg }")
    )
    );

#define GST_TYPE_VIDEOLEVELS_AUTO (gst_videolevels_auto_get_type())
static GType
gst_videolevels_auto_get_type (void)
{
  static GType videolevels_auto_type = 0;
  static const GEnumValue videolevels_auto[] = {
    {GST_VIDEOLEVELS_AUTO_OFF, "off", "off"},
    {GST_VIDEOLEVELS_AUTO_SINGLE, "single", "single"},
    {GST_VIDEOLEVELS_AUTO_CONTINUOUS, "continuous", "continuous"},
    {0, NULL, NULL},
  };

  if (!videolevels_auto_type) {
    videolevels_auto_type =
        g_enum_register_static ("GstVideoLevelsAuto", videolevels_auto);
  }
  return videolevels_auto_type;
}

/* GObject vmethod declarations */
static void gst_videolevels_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_videolevels_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_videolevels_dispose (GObject * object);

/* GstBaseTransform vmethod declarations */
static GstCaps *gst_videolevels_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps);
static gboolean gst_videolevels_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_videolevels_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);

/* GstVideoLevels method declarations */
static void gst_videolevels_reset (GstVideoLevels * filter);
static gboolean gst_videolevels_calculate_lut (GstVideoLevels * videolevels);
static gboolean gst_videolevels_calculate_histogram (GstVideoLevels *
    videolevels, guint16 * data);
static gboolean gst_videolevels_auto_adjust (GstVideoLevels * videolevels,
    guint16 * data);
static void gst_videolevels_check_passthrough (GstVideoLevels * videolevels);

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (videolevels_debug);
#define GST_CAT_DEFAULT videolevels_debug

G_DEFINE_TYPE (GstVideoLevels, gst_videolevels, GST_TYPE_BASE_TRANSFORM);

/************************************************************************/
/* GObject vmethod implementations                                      */
/************************************************************************/

/**
 * gst_videolevels_dispose:
 * @object: #GObject.
 *
 */
static void
gst_videolevels_dispose (GObject * object)
{
  GstVideoLevels *videolevels = GST_VIDEOLEVELS (object);

  GST_DEBUG ("dispose");

  g_free (videolevels->lookup_table);

  gst_videolevels_reset (videolevels);

  /* chain up to the parent class */
  G_OBJECT_CLASS (gst_videolevels_parent_class)->dispose (object);
}

/**
 * gst_videolevels_class_init:
 * @object: #GstVideoLevelsClass.
 *
 */
static void
gst_videolevels_class_init (GstVideoLevelsClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *gstbasetransform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (videolevels_debug, "videolevels", 0,
      "Video Levels Filter");

  GST_DEBUG ("class init");

  /* Register GObject vmethods */
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_videolevels_dispose);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_videolevels_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_videolevels_get_property);

  /* Install GObject properties */
  properties[PROP_LOWIN] =
      g_param_spec_int ("lower-input-level", "Lower Input Level",
      "Lower Input Level", -1, DEFAULT_PROP_HIGHIN, DEFAULT_PROP_LOWIN,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  properties[PROP_HIGHIN] =
      g_param_spec_int ("upper-input-level", "Upper Input Level",
      "Upper Input Level", -1, DEFAULT_PROP_HIGHIN, DEFAULT_PROP_HIGHIN,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  properties[PROP_LOWOUT] =
      g_param_spec_int ("lower-output-level", "Lower Output Level",
      "Lower Output Level", 0, DEFAULT_PROP_HIGHOUT, DEFAULT_PROP_LOWOUT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  properties[PROP_HIGHOUT] =
      g_param_spec_int ("upper-output-level", "Upper Output Level",
      "Upper Output Level", 0, DEFAULT_PROP_HIGHOUT, DEFAULT_PROP_HIGHOUT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (gobject_class, PROP_LOWIN,
      properties[PROP_LOWIN]);
  g_object_class_install_property (gobject_class, PROP_HIGHIN,
      properties[PROP_HIGHIN]);
  g_object_class_install_property (gobject_class, PROP_LOWOUT,
      properties[PROP_LOWOUT]);
  g_object_class_install_property (gobject_class, PROP_HIGHOUT,
      properties[PROP_HIGHOUT]);
  g_object_class_install_property (gobject_class, PROP_AUTO,
      g_param_spec_enum ("auto", "Auto Adjust", "Auto adjust contrast",
          GST_TYPE_VIDEOLEVELS_AUTO, DEFAULT_PROP_AUTO, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_INTERVAL,
      g_param_spec_uint64 ("interval", "Interval",
          "Interval of time between adjustments (in nanoseconds)", 1,
          G_MAXUINT64, DEFAULT_PROP_INTERVAL, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_LOWER_SATURATION,
      g_param_spec_double ("lower-saturation", "Lower saturation",
          "The fraction of the histogram to saturate on the low end when auto is enabled",
          0, 0.99, DEFAULT_PROP_LOW_SAT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_UPPER_SATURATION,
      g_param_spec_double ("upper-saturation", "Upper saturation",
          "The fraction of the histogram to saturate on the upper end when auto is enabled",
          0, 0.99, DEFAULT_PROP_HIGH_SAT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ROI_X,
      g_param_spec_int ("roi-x", "ROI x",
          "Starting column of the ROI when auto is enabled (-1 centers ROI)",
          -1, G_MAXINT, DEFAULT_PROP_ROI_X, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ROI_Y,
      g_param_spec_int ("roi-y", "ROI y",
          "Starting row of the ROI when auto is enabled (-1 centers ROI)",
          -1, G_MAXINT, DEFAULT_PROP_ROI_Y, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ROI_WIDTH,
      g_param_spec_int ("roi-width", "ROI width",
          "Width of the ROI when auto is enabled (0 uses 1/2 of the image width)",
          0, G_MAXINT, DEFAULT_PROP_ROI_WIDTH, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_ROI_HEIGHT,
      g_param_spec_int ("roi-height", "ROI height",
          "Height of the ROI when auto is enabled (0 uses 1/2 of the image height)",
          0, G_MAXINT, DEFAULT_PROP_ROI_HEIGHT, G_PARAM_READWRITE));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_videolevels_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_videolevels_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Video levels adjustment", "Filter/Effect/Video",
      "Adjusts videolevels on a video stream",
      "Joshua M. Doe <oss@nvl.army.mil>");

  /* Register GstBaseTransform vmethods */
  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_videolevels_transform_caps);

  gstbasetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_videolevels_set_caps);
  gstbasetransform_class->transform =
      GST_DEBUG_FUNCPTR (gst_videolevels_transform);
}

/**
* gst_videolevels_init:
* @videolevels: GstVideoLevels
* @g_class: GstVideoLevelsClass
*
* Initialize the new element
*/
static void
gst_videolevels_init (GstVideoLevels * videolevels)
{
  GST_DEBUG_OBJECT (videolevels, "init class instance");

  videolevels->passthrough = FALSE;

  videolevels->lookup_table = g_new (guint8, G_MAXUINT16 + 1);

  gst_videolevels_reset (videolevels);
}

/**
 * gst_videolevels_set_property:
 * @object: #GObject
 * @prop_id: guint
 * @value: #GValue
 * @pspec: #GParamSpec
 *
 */
static void
gst_videolevels_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoLevels *videolevels = GST_VIDEOLEVELS (object);

  GST_DEBUG_OBJECT (videolevels, "setting property %s", pspec->name);

  switch (prop_id) {
    case PROP_LOWIN:
      videolevels->lower_input = g_value_get_int (value);
      gst_videolevels_calculate_lut (videolevels);
      break;
    case PROP_HIGHIN:
      videolevels->upper_input = g_value_get_int (value);
      gst_videolevels_calculate_lut (videolevels);
      break;
    case PROP_LOWOUT:
      videolevels->lower_output = g_value_get_int (value);
      gst_videolevels_calculate_lut (videolevels);
      break;
    case PROP_HIGHOUT:
      videolevels->upper_output = g_value_get_int (value);
      gst_videolevels_calculate_lut (videolevels);
      break;
    case PROP_AUTO:{
      videolevels->auto_adjust = g_value_get_enum (value);
      break;
    }
    case PROP_INTERVAL:
      videolevels->interval = g_value_get_uint64 (value);
      videolevels->last_auto_timestamp = GST_CLOCK_TIME_NONE;
      break;
    case PROP_LOWER_SATURATION:
      videolevels->lower_pix_sat = g_value_get_double (value);
      gst_videolevels_calculate_lut (videolevels);
      break;
    case PROP_UPPER_SATURATION:
      videolevels->upper_pix_sat = g_value_get_double (value);
      gst_videolevels_calculate_lut (videolevels);
      break;
    case PROP_ROI_X:
      videolevels->roi_x = g_value_get_int (value);
      videolevels->check_roi = TRUE;
      break;
    case PROP_ROI_Y:
      videolevels->roi_y = g_value_get_int (value);
      videolevels->check_roi = TRUE;
      break;
    case PROP_ROI_WIDTH:
      videolevels->roi_width = g_value_get_int (value);
      videolevels->check_roi = TRUE;
      break;
    case PROP_ROI_HEIGHT:
      videolevels->roi_height = g_value_get_int (value);
      videolevels->check_roi = TRUE;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * gst_videolevels_get_property:
 * @object: #GObject
 * @prop_id: guint
 * @value: #GValue
 * @pspec: #GParamSpec
 *
 */
static void
gst_videolevels_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVideoLevels *videolevels = GST_VIDEOLEVELS (object);

  GST_LOG_OBJECT (videolevels, "getting property %s", pspec->name);

  switch (prop_id) {
    case PROP_LOWIN:
      g_value_set_int (value, videolevels->lower_input);
      break;
    case PROP_HIGHIN:
      g_value_set_int (value, videolevels->upper_input);
      break;
    case PROP_LOWOUT:
      g_value_set_int (value, videolevels->lower_output);
      break;
    case PROP_HIGHOUT:
      g_value_set_int (value, videolevels->upper_output);
      break;
    case PROP_AUTO:
      g_value_set_enum (value, videolevels->auto_adjust);
      break;
    case PROP_INTERVAL:
      g_value_set_uint64 (value, videolevels->interval);
      break;
    case PROP_LOWER_SATURATION:
      g_value_set_double (value, videolevels->lower_pix_sat);
      break;
    case PROP_UPPER_SATURATION:
      g_value_set_double (value, videolevels->upper_pix_sat);
      break;
    case PROP_ROI_X:
      g_value_set_int (value, videolevels->roi_x);
      break;
    case PROP_ROI_Y:
      g_value_set_int (value, videolevels->roi_y);
      break;
    case PROP_ROI_WIDTH:
      g_value_set_int (value, videolevels->roi_width);
      break;
    case PROP_ROI_HEIGHT:
      g_value_set_int (value, videolevels->roi_height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/************************************************************************/
/* GstBaseTransform vmethod implementations                             */
/************************************************************************/

static gchar *
swap_format_string_bits (const gchar * oldstr)
{
  gchar *newstr;
  if (g_str_has_suffix (oldstr, "16")) {
    newstr = g_strdup (oldstr);
    g_assert (strlen (newstr) > 5);
    newstr[4] = '\0';
  } else {
    newstr = g_strconcat (oldstr, "16", NULL);
  }
  return newstr;
}

static void
swap_format_list (const GstStructure * st, GstStructure * newst)
{
  const GValue *value;
  value = gst_structure_get_value (st, "format");
  if (GST_VALUE_HOLDS_LIST (value)) {
    guint j;
    GValue newval = G_VALUE_INIT;
    GValue newlist = G_VALUE_INIT;
    g_value_init (&newlist, GST_TYPE_LIST);
    g_value_init (&newval, G_TYPE_STRING);
    for (j = 0; j < gst_value_list_get_size (value); ++j) {
      const gchar *oldstr;
      gchar *newstr;
      const GValue *oldval = gst_value_list_get_value (value, j);
      oldstr = g_value_get_string (oldval);
      newstr = swap_format_string_bits (oldstr);
      g_value_set_string (&newval, newstr);
      gst_value_list_append_value (&newlist, &newval);
      g_free (newstr);
    }
    gst_structure_set_value (newst, "format", &newlist);
  } else {
    const gchar *oldstr;
    gchar *newstr;
    GValue newval = G_VALUE_INIT;
    oldstr = gst_structure_get_string (st, "format");
    newstr = swap_format_string_bits (oldstr);
    g_value_init (&newval, G_TYPE_STRING);
    g_value_set_string (&newval, newstr);
    g_free (newstr);

    gst_structure_set_value (newst, "format", &newval);
  }
}

static void
copy_width_height_framerate (const GstStructure * st, GstStructure * newst)
{
  const GValue *value;
  value = gst_structure_get_value (st, "width");
  gst_structure_set_value (newst, "width", value);

  value = gst_structure_get_value (st, "height");
  gst_structure_set_value (newst, "height", value);

  value = gst_structure_get_value (st, "framerate");
  gst_structure_set_value (newst, "framerate", value);
}

/**
 * gst_videolevels_transform_caps:
 * @base: #GstBaseTransform
 * @direction: #GstPadDirection
 * @caps: #GstCaps
 *
 * Given caps on one side, what caps are allowed on the other
 *
 * Returns: #GstCaps allowed on other pad
 */
GstCaps *
gst_videolevels_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  GstVideoLevels *videolevels = GST_VIDEOLEVELS (trans);
  GstCaps *other_caps;
  GstStructure *st, *newst;
  gint i, n;
  const gchar *name;

  videolevels = GST_VIDEOLEVELS (trans);

  GST_LOG_OBJECT (videolevels, "transforming %s caps %" GST_PTR_FORMAT,
      direction == GST_PAD_SRC ? "SRC" : "SINK", caps);

  other_caps = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; ++i) {
    st = gst_caps_get_structure (caps, i);
    name = gst_structure_get_name (st);

    if (gst_caps_is_subset_structure (other_caps, st))
      continue;

    if (direction == GST_PAD_SRC) {
      if (g_strcmp0 (name, "video/x-raw") == 0) {
        newst =
            gst_structure_from_string
            ("video/x-raw,format={GRAY16_LE,GRAY16_BE,GRAY8}", NULL);
        copy_width_height_framerate (st, newst);
        gst_caps_append_structure (other_caps, newst);
      } else if (g_strcmp0 (name, "video/x-bayer") == 0) {
        /* we can handle same format, e.g. bggr->bggr */
        gst_caps_append_structure (other_caps, gst_structure_copy (st));

        /* and also with higher bit depth */
        newst = gst_structure_from_string ("video/x-bayer", NULL);
        swap_format_list (st, newst);
        copy_width_height_framerate (st, newst);
        gst_caps_append_structure (other_caps, newst);
      } else {
        g_assert_not_reached ();
      }
    } else {
      if (g_strcmp0 (name, "video/x-raw") == 0) {
        newst = gst_structure_from_string ("video/x-raw,format=GRAY8", NULL);
        copy_width_height_framerate (st, newst);
        gst_caps_append_structure (other_caps, newst);
      } else if (g_strcmp0 (name, "video/x-bayer") == 0) {
        const GValue *value;
        const gchar *str;
        newst = gst_structure_from_string ("video/x-bayer", NULL);
        value = gst_structure_get_value (st, "format");
        if (GST_VALUE_HOLDS_LIST (value)) {
          value = gst_value_list_get_value (value, 0);
        }
        str = g_value_get_string (value);
        if (g_str_has_suffix (str, "16")) {
          swap_format_list (st, newst);
        } else {
          gst_structure_set_value (newst, "format", gst_structure_get_value (st,
                  "format"));
        }
        copy_width_height_framerate (st, newst);
        gst_caps_append_structure (other_caps, newst);
      } else {
        g_assert_not_reached ();
      }
    }
  }

  if (!gst_caps_is_empty (other_caps) && filter_caps) {
    GstCaps *tmp = gst_caps_intersect_full (filter_caps, other_caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_replace (&other_caps, tmp);
    gst_caps_unref (tmp);
  }

  GST_LOG_OBJECT (videolevels, "transformed to %s caps %" GST_PTR_FORMAT,
      direction == GST_PAD_SINK ? "SRC" : "SINK", other_caps);

  return other_caps;
}

static gboolean
gst_videolevels_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstVideoLevels *levels = GST_VIDEOLEVELS (trans);
  GstStructure *st;
  gboolean res;
  GstVideoInfo invinfo, outvinfo;

  GST_DEBUG_OBJECT (levels,
      "set_caps: in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

  /* always assume 8-bit output */
  levels->bpp_out = 8;

  /* GstVideoInfo treats Bayer as encoded, but it's still useful */
  gst_video_info_from_caps (&invinfo, incaps);
  gst_video_info_from_caps (&outvinfo, outcaps);

  levels->width = GST_VIDEO_INFO_WIDTH (&invinfo);
  levels->height = GST_VIDEO_INFO_HEIGHT (&invinfo);

  /* these won't be valid for Bayer, we'll fix later */
  levels->stride_in = GST_VIDEO_INFO_COMP_STRIDE (&invinfo, 0);
  levels->stride_out = GST_VIDEO_INFO_COMP_STRIDE (&outvinfo, 0);
  levels->bpp_in = invinfo.finfo->bits;

  st = gst_caps_get_structure (incaps, 0);

  if (invinfo.finfo->format == GST_VIDEO_FORMAT_GRAY8) {
    // do nothing
  } else if (invinfo.finfo->format == GST_VIDEO_FORMAT_GRAY16_BE) {
    levels->endianness_in = G_BIG_ENDIAN;
  } else if (invinfo.finfo->format == GST_VIDEO_FORMAT_GRAY16_LE) {
    levels->endianness_in = G_LITTLE_ENDIAN;
  } else {
    const gchar *format = gst_structure_get_string (st, "format");
    if (g_str_has_suffix (format, "16")) {
      gst_structure_get_int (st, "endianness", &levels->endianness_in);
      levels->bpp_in = 16;
      levels->stride_in = GST_ROUND_UP_4 (levels->width * 2);
    } else {
      levels->bpp_in = 8;
      levels->stride_in = GST_ROUND_UP_4 (levels->width);
    }
    levels->stride_out = GST_ROUND_UP_4 (levels->width);
  }

  if (gst_structure_has_field (st, "bpp")) {
    gst_structure_get_int (st, "bpp", &levels->bpp_in);
  }

  g_assert (levels->bpp_in >= 1 && levels->bpp_in <= 16);

  levels->nbins = MIN (4096, 1 << levels->bpp_in);

  levels->check_roi = TRUE;

  res = gst_videolevels_calculate_lut (levels);

  return res;
}

/**
 * gst_videolevels_transform:
 * @base: #GstBaseTransform
 * @inbuf: #GstBuffer
 * @outbuf: #GstBuffer
 *
 * Transforms input buffer to output buffer.
 *
 * Returns: GST_FLOW_OK on success
 */
static GstFlowReturn
gst_videolevels_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstVideoLevels *videolevels = GST_VIDEOLEVELS (trans);
  GstClockTimeDiff elapsed;
  GstClockTime start =
      gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (videolevels)));
  gint r, c;
  guint8 *in_data, *out_data;
  guint8 *lut;
  GstMapInfo inminfo, outminfo;

  GST_LOG_OBJECT (videolevels, "Performing non-inplace transform");

  gst_buffer_map (inbuf, &inminfo, GST_MAP_READ);
  gst_buffer_map (outbuf, &outminfo, GST_MAP_WRITE);

  if (!inminfo.data || !outminfo.data) {
    GST_ELEMENT_ERROR (videolevels, STREAM, FAILED, ("Failed to map buffer"),
        (NULL));
    return GST_FLOW_ERROR;
  }
  in_data = inminfo.data;
  out_data = outminfo.data;

  if (videolevels->auto_adjust == 1) {
    GST_DEBUG_OBJECT (videolevels, "Auto adjusting levels (once)");
    gst_videolevels_auto_adjust (videolevels, (guint16 *) in_data);
    videolevels->auto_adjust = 0;
    g_object_notify (G_OBJECT (videolevels), "auto");
  } else if (videolevels->auto_adjust == 2) {
    elapsed =
        GST_CLOCK_DIFF (videolevels->last_auto_timestamp,
        GST_BUFFER_TIMESTAMP (inbuf));
    if (videolevels->last_auto_timestamp == GST_CLOCK_TIME_NONE
        || elapsed >= (GstClockTimeDiff) videolevels->interval || elapsed < 0) {
      GST_LOG_OBJECT (videolevels, "Auto adjusting levels (%d ns since last)",
          elapsed);
      gst_videolevels_auto_adjust (videolevels, (guint16 *) in_data);
      videolevels->last_auto_timestamp = GST_BUFFER_TIMESTAMP (inbuf);
    }
  }

  lut = videolevels->lookup_table;
  if (videolevels->bpp_in > 8) {
    for (r = 0; r < videolevels->height; r++) {
      guint16 *src = (guint16 *) in_data;
      guint8 *dst = out_data;

      for (c = 0; c < videolevels->width; c++) {
        //GST_LOG_OBJECT (videolevels, "Converting pixel (%d, %d), %d->%d", c, r, *src, lut[*src]);
        *dst++ = lut[*src++];
      }

      in_data += videolevels->stride_in;
      out_data += videolevels->stride_out;
    }
  } else {
    for (r = 0; r < videolevels->height; r++) {
      guint8 *src = (guint8 *) in_data;
      guint8 *dst = out_data;

      for (c = 0; c < videolevels->width; c++) {
        //GST_LOG_OBJECT (videolevels, "Converting pixel (%d, %d), %d->%d", c, r, *src, lut[*src]);
        *dst++ = lut[*src++];
      }

      in_data += videolevels->stride_in;
      out_data += videolevels->stride_out;
    }
  }

  gst_buffer_unmap (inbuf, &inminfo);
  gst_buffer_unmap (outbuf, &outminfo);

  GST_LOG_OBJECT (videolevels, "Processing took %" G_GINT64_FORMAT "ms",
      GST_TIME_AS_MSECONDS (GST_CLOCK_DIFF (start,
              gst_clock_get_time (gst_element_get_clock (GST_ELEMENT
                      (videolevels))))));

  return GST_FLOW_OK;
}

/************************************************************************/
/* GstVideoLevels method implementations                                */
/************************************************************************/

/**
 * gst_videolevels_reset:
 * @videolevels: #GstVideoLevels
 *
 * Reset instance variables and free memory
 */
static void
gst_videolevels_reset (GstVideoLevels * videolevels)
{
  videolevels->bpp_in = 0;

  videolevels->lower_input = DEFAULT_PROP_LOWIN;
  videolevels->upper_input = DEFAULT_PROP_HIGHIN;
  videolevels->lower_output = DEFAULT_PROP_LOWOUT;
  videolevels->upper_output = DEFAULT_PROP_HIGHOUT;
  videolevels->lower_pix_sat = DEFAULT_PROP_LOW_SAT;
  videolevels->upper_pix_sat = DEFAULT_PROP_HIGH_SAT;
  videolevels->roi_x = DEFAULT_PROP_ROI_X;
  videolevels->roi_y = DEFAULT_PROP_ROI_Y;
  videolevels->roi_width = DEFAULT_PROP_ROI_WIDTH;
  videolevels->roi_height = DEFAULT_PROP_ROI_HEIGHT;

  videolevels->auto_adjust = DEFAULT_PROP_AUTO;
  videolevels->interval = DEFAULT_PROP_INTERVAL;

  videolevels->check_roi = TRUE;
  videolevels->last_auto_timestamp = GST_CLOCK_TIME_NONE;

  /* if GRAY8, this will be set in set_info */
  videolevels->nbins = 4096;

  g_free (videolevels->histogram);
  videolevels->histogram = NULL;
}

#define GINT_CLAMP(x, low, high) ((gint)(CLAMP((x),(low),(high))))
#define GUINT8_CLAMP(x, low, high) ((guint8)(CLAMP((x),(low),(high))))

static gboolean
gst_videolevels_calculate_lut (GstVideoLevels * videolevels)
{
  gint i;
  gdouble m;
  gdouble b;
  guint8 *lut = (guint8 *) videolevels->lookup_table;
  const guint16 max_in = (1 << videolevels->bpp_in) - 1;
  guint16 low_in;
  guint16 high_in;
  const guint8 max_out = (1 << videolevels->bpp_out) - 1;
  const guint8 low_out = videolevels->lower_output;
  const guint8 high_out = videolevels->upper_output;

  if (videolevels->bpp_in == 0) {
    return FALSE;
  }

  GST_LOG_OBJECT (videolevels, "Calculating lookup table");

  if (videolevels->lower_input < 0 || videolevels->lower_input > max_in) {
    videolevels->lower_input = 0;
    g_object_notify_by_pspec (G_OBJECT (videolevels), properties[PROP_LOWIN]);
  }
  if (videolevels->upper_input < 0 || videolevels->upper_input > max_in) {
    videolevels->upper_input = max_in;
    g_object_notify_by_pspec (G_OBJECT (videolevels), properties[PROP_HIGHIN]);
  }

  gst_videolevels_check_passthrough (videolevels);

  if (!videolevels->passthrough) {
    low_in = videolevels->lower_input;
    high_in = videolevels->upper_input;

    GST_LOG_OBJECT (videolevels, "Make linear LUT mapping (%d, %d) -> (%d, %d)",
        low_in, high_in, low_out, high_out);

    if (low_in == high_in)
      m = 0.0;
    else
      m = (high_out - low_out) / (gdouble) (high_in - low_in);

    b = low_out - m * low_in;

    if (videolevels->endianness_in == G_LITTLE_ENDIAN)
      for (i = 0; i < G_MAXUINT16; i++)
        lut[i] = GUINT8_CLAMP (m * GUINT16_FROM_LE (i) + b, low_out, high_out);
    else if (videolevels->endianness_in == G_BIG_ENDIAN)
      for (i = 0; i < G_MAXUINT16; i++)
        lut[i] = GUINT8_CLAMP (m * GUINT16_FROM_BE (i) + b, low_out, high_out);
    else
      for (i = 0; i < G_MAXUINT16; i++)
        lut[i] = GUINT8_CLAMP (m * i + b, low_out, high_out);
  }

  return TRUE;
}


/**
* gst_videolevels_calculate_histogram
* @videolevels: #GstVideoLevels
* @data: input frame data
*
* Calculate histogram of input frame
*
* Returns: TRUE on success
*/
gboolean
gst_videolevels_calculate_histogram (GstVideoLevels * videolevels,
    guint16 * data)
{
  gint *hist;
  gint nbins = videolevels->nbins;
  gint r;
  gint c;
  gfloat factor;
  gint stride = videolevels->stride_in;
  gint endianness = videolevels->endianness_in;
  gint maxVal = (1 << videolevels->bpp_in) - 1;

  factor = (gfloat) ((nbins - 1.0) / maxVal);

  if (videolevels->histogram == NULL) {
    GST_DEBUG_OBJECT (videolevels,
        "First call, allocate memory for histogram (%d bins)", nbins);
    videolevels->histogram = g_new (gint, nbins);
  }

  hist = videolevels->histogram;

  /* reset histogram */
  memset (hist, 0, sizeof (gint) * nbins);

  GST_LOG_OBJECT (videolevels, "Calculating histogram");
  if (videolevels->bpp_in > 8) {
    if (endianness == G_BYTE_ORDER) {
      for (r = videolevels->roi_y;
          r < videolevels->roi_y + videolevels->roi_height; r++) {
        for (c = videolevels->roi_x;
            c < videolevels->roi_x + videolevels->roi_width; c++) {
          hist[GINT_CLAMP (data[c + r * stride / 2] * factor, 0, nbins - 1)]++;
        }
      }
    } else {
      for (r = videolevels->roi_y;
          r < videolevels->roi_y + videolevels->roi_height; r++) {
        for (c = videolevels->roi_x;
            c < videolevels->roi_x + videolevels->roi_width; c++) {
          hist[GINT_CLAMP (GUINT16_FROM_BE (data[c + r * stride / 2]) * factor,
                  0, nbins - 1)]++;
        }
      }
    }
  } else {
    guint8 *data8 = (guint8 *) data;
    for (r = videolevels->roi_y;
        r < videolevels->roi_y + videolevels->roi_height; r++) {
      for (c = videolevels->roi_x;
          c < videolevels->roi_x + videolevels->roi_width; c++) {
        hist[GINT_CLAMP (data8[c + r * stride / 2] * factor, 0, nbins - 1)]++;
      }
    }
  }

  return TRUE;
}


void
gst_videolevels_check_roi (GstVideoLevels * filt)
{
  GST_DEBUG_OBJECT (filt, "ROI before check is (%d, %d, %d, %d)", filt->roi_x,
      filt->roi_y, filt->roi_width, filt->roi_height);

  /* adjust ROI if defaults are set */
  if (filt->roi_width <= 0) {
    filt->roi_width = filt->width / 2;
  }
  if (filt->roi_height <= 0) {
    filt->roi_height = filt->height / 2;
  }
  if (filt->roi_x <= -1) {
    filt->roi_x = (filt->width - filt->roi_width) / 2;
  }
  if (filt->roi_y <= -1) {
    filt->roi_y = (filt->height - filt->roi_height) / 2;
  }

  /* ensure ROI is within image */
  filt->roi_x = GINT_CLAMP (filt->roi_x, 0, filt->width - 1);
  filt->roi_y = GINT_CLAMP (filt->roi_y, 0, filt->height - 1);
  filt->roi_width = GINT_CLAMP (filt->roi_width, 1, filt->width - filt->roi_x);
  filt->roi_height =
      GINT_CLAMP (filt->roi_height, 1, filt->height - filt->roi_y);

  GST_DEBUG_OBJECT (filt, "ROI after check is (%d, %d, %d, %d)", filt->roi_x,
      filt->roi_y, filt->roi_width, filt->roi_height);
}

/**
* gst_videolevels_auto_adjust
* @videolevels: #GstVideoLevels
* @data: input frame data
*
* Calculate lower and upper levels based on the histogram of the frame
*
* Returns: TRUE on success
*/
gboolean
gst_videolevels_auto_adjust (GstVideoLevels * filt, guint16 * data)
{
  guint npixsat;
  guint sum;
  gint i;
  gint pixel_count;
  gint minVal = 0;
  gint maxVal = (1 << filt->bpp_in) - 1;
  float factor = maxVal / (filt->nbins - 1.0f);

  if (filt->check_roi) {
    gst_videolevels_check_roi (filt);
    filt->check_roi = FALSE;
  }

  gst_videolevels_calculate_histogram (filt, data);

  pixel_count = filt->roi_width * filt->roi_height;

  /* pixels to saturate on low end */
  npixsat = (guint) (filt->lower_pix_sat * pixel_count);
  sum = 0;
  for (i = 0; i < filt->nbins; i++) {
    sum += filt->histogram[i];
    if (sum > npixsat) {
      filt->lower_input = (gint) CLAMP (i * factor, minVal, maxVal);
      break;
    }
  }

  /* pixels to saturate on high end */
  npixsat = (guint) (filt->upper_pix_sat * pixel_count);
  sum = 0;
  for (i = filt->nbins - 1; i >= 0; i--) {
    sum += filt->histogram[i];
    if (sum > npixsat) {
      filt->upper_input = (gint) CLAMP (i * factor, minVal, maxVal);
      break;
    }
  }

  gst_videolevels_calculate_lut (filt);

  GST_LOG_OBJECT (filt, "Contrast stretch with npixsat=%d, (%d, %d)",
      npixsat, filt->lower_input, filt->upper_input);

  g_object_notify_by_pspec (G_OBJECT (filt), properties[PROP_LOWIN]);
  g_object_notify_by_pspec (G_OBJECT (filt), properties[PROP_HIGHIN]);

  return TRUE;
}

static void
gst_videolevels_check_passthrough (GstVideoLevels * levels)
{
  gboolean passthrough;
  if (levels->bpp_in == 8 &&
      levels->auto_adjust == GST_VIDEOLEVELS_AUTO_OFF &&
      levels->lower_input == levels->lower_output &&
      levels->upper_input == levels->upper_output) {
    passthrough = TRUE;
  } else {
    passthrough = FALSE;
  }
  if (passthrough != levels->passthrough) {
    GST_DEBUG_OBJECT (levels, "Passthrough mode: %s",
        passthrough ? "ENABLED" : "DISABLED");
    levels->passthrough = passthrough;
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (levels),
        levels->passthrough);
  }
}
