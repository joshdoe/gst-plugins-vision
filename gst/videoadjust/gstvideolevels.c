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

#include "gstvideolevels.h"

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
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

#define DEFAULT_PROP_LOWIN  0
#define DEFAULT_PROP_HIGHIN  65535
#define DEFAULT_PROP_LOWOUT  0
#define DEFAULT_PROP_HIGHOUT  255
#define DEFAULT_PROP_AUTO 0
#define DEFAULT_PROP_INTERVAL (GST_SECOND / 2)

/* the capabilities of the inputs and outputs */
static GstStaticPadTemplate gst_videolevels_src_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ GRAY16_LE, GRAY16_BE, GRAY8 }"))
    //";"
    //    "video/x-bayer,format=(string){bggr16,grbg16,gbrg16,rggb16},"
    //    "bpp=(int){10,12,14,16},endianness={1234,4321},"
    //    "width=(int)[1,MAX],height=(int)[1,MAX],framerate=(fraction)[0/1,MAX]")
    );

static GstStaticPadTemplate gst_videolevels_sink_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("GRAY8"))
    //";"
    //    "video/x-bayer,format=(string){bggr,grbg,gbrg,rggb},"
    //    "width=(int)[1,MAX],height=(int)[1,MAX],framerate=(fraction)[0/1,MAX]")
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

/* GstVideoFilter vmethod declarations */
static gboolean gst_videolevels_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static GstFlowReturn gst_videolevels_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame);

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

G_DEFINE_TYPE (GstVideoLevels, gst_videolevels, GST_TYPE_VIDEO_FILTER);

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
  GstVideoFilterClass *gstvideofilter_class = GST_VIDEO_FILTER_CLASS (klass);

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

  gstvideofilter_class->set_info = GST_DEBUG_FUNCPTR (gst_videolevels_set_info);
  gstvideofilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_videolevels_transform_frame);
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

  GST_DEBUG_OBJECT (videolevels, "getting property %s", pspec->name);

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/************************************************************************/
/* GstBaseTransform vmethod implementations                             */
/************************************************************************/

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
  const GValue *value;

  videolevels = GST_VIDEOLEVELS (trans);

  GST_LOG_OBJECT (videolevels, "transforming caps %" GST_PTR_FORMAT, caps);

  other_caps = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; ++i) {
    st = gst_caps_get_structure (caps, i);

    if (gst_caps_is_subset_structure (other_caps, st))
      continue;

    if (direction == GST_PAD_SRC) {
      newst =
          gst_structure_from_string
          ("video/x-raw,format={GRAY16_LE,GRAY16_BE,GRAY8}", NULL);
    } else {
      newst = gst_structure_from_string ("video/x-raw,format=GRAY8", NULL);
    }

    value = gst_structure_get_value (st, "width");
    gst_structure_set_value (newst, "width", value);

    value = gst_structure_get_value (st, "height");
    gst_structure_set_value (newst, "height", value);

    value = gst_structure_get_value (st, "framerate");
    gst_structure_set_value (newst, "framerate", value);

    gst_caps_append_structure (other_caps, newst);
  }

  if (!gst_caps_is_empty (other_caps) && filter_caps) {
    GstCaps *tmp = gst_caps_intersect_full (filter_caps, other_caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_replace (&other_caps, tmp);
    gst_caps_unref (tmp);
  }

  return other_caps;
}

static gboolean
gst_videolevels_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstVideoLevels *levels = GST_VIDEOLEVELS (filter);
  GstStructure *s;
  gboolean res;

  GST_DEBUG_OBJECT (levels,
      "set_caps: in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

  memcpy (&levels->info_in, in_info, sizeof (GstVideoInfo));
  memcpy (&levels->info_out, out_info, sizeof (GstVideoInfo));

  s = gst_caps_get_structure (incaps, 0);

  if (levels->info_in.finfo->format == GST_VIDEO_FORMAT_GRAY8) {
    levels->bpp_in = 8;
    levels->nbins = 256;
  } else {
    if (!gst_structure_get_int (s, "bpp", &levels->bpp_in)) {
      levels->bpp_in = 16;
    }
    levels->nbins = 4096;
  }
  g_assert (levels->bpp_in >= 1 && levels->bpp_in <= 16);

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
gst_videolevels_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstVideoLevels *videolevels = GST_VIDEOLEVELS (filter);
  GstClockTimeDiff elapsed;
  GstClockTime start =
      gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (videolevels)));
  gint r, c;
  gint in_stride, out_stride;
  guint8 *in_data, *out_data;
  guint8 *lut;

  GST_LOG_OBJECT (videolevels, "Performing non-inplace transform");

  in_data = GST_VIDEO_FRAME_PLANE_DATA (in_frame, 0);
  out_data = GST_VIDEO_FRAME_PLANE_DATA (out_frame, 0);

  if (videolevels->auto_adjust == 1) {
    GST_DEBUG_OBJECT (videolevels, "Auto adjusting levels (once)");
    gst_videolevels_auto_adjust (videolevels, in_data);
    videolevels->auto_adjust = 0;
    g_object_notify (G_OBJECT (videolevels), "auto");
  } else if (videolevels->auto_adjust == 2) {
    elapsed =
        GST_CLOCK_DIFF (videolevels->last_auto_timestamp,
        GST_BUFFER_TIMESTAMP (in_frame->buffer));
    if (videolevels->last_auto_timestamp == GST_CLOCK_TIME_NONE
        || elapsed >= (GstClockTimeDiff) videolevels->interval || elapsed < 0) {
      GST_LOG_OBJECT (videolevels, "Auto adjusting levels (%d ns since last)",
          elapsed);
      gst_videolevels_auto_adjust (videolevels, in_data);
      videolevels->last_auto_timestamp =
          GST_BUFFER_TIMESTAMP (in_frame->buffer);
    }
  }

  in_stride = GST_VIDEO_FRAME_PLANE_STRIDE (in_frame, 0);
  out_stride = GST_VIDEO_FRAME_PLANE_STRIDE (out_frame, 0);

  lut = videolevels->lookup_table;
  if (videolevels->bpp_in > 8) {
    for (r = 0; r < in_frame->info.height; r++) {
      guint16 *src = (guint16 *) in_data;
      guint8 *dst = out_data;

      for (c = 0; c < in_frame->info.width; c++) {
        //GST_LOG_OBJECT (videolevels, "Converting pixel (%d, %d), %d->%d", c, r, *src, lut[*src]);
        *dst++ = lut[*src++];
      }

      in_data += in_stride;
      out_data += out_stride;
    }
  } else {
    for (r = 0; r < in_frame->info.height; r++) {
      guint8 *src = (guint8 *) in_data;
      guint8 *dst = out_data;

      for (c = 0; c < in_frame->info.width; c++) {
        //GST_LOG_OBJECT (videolevels, "Converting pixel (%d, %d), %d->%d", c, r, *src, lut[*src]);
        *dst++ = lut[*src++];
      }

      in_data += in_stride;
      out_data += out_stride;
    }
  }

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
  gst_video_info_init (&videolevels->info_in);
  gst_video_info_init (&videolevels->info_out);

  videolevels->lower_input = DEFAULT_PROP_LOWIN;
  videolevels->upper_input = DEFAULT_PROP_HIGHIN;
  videolevels->lower_output = DEFAULT_PROP_LOWOUT;
  videolevels->upper_output = DEFAULT_PROP_HIGHOUT;

  g_free (videolevels->lookup_table);
  videolevels->lookup_table = NULL;

  videolevels->auto_adjust = DEFAULT_PROP_AUTO;
  videolevels->interval = DEFAULT_PROP_INTERVAL;
  videolevels->last_auto_timestamp = GST_CLOCK_TIME_NONE;

  videolevels->lower_pix_sat = 0.01f;
  videolevels->upper_pix_sat = 0.01f;

  /* if GRAY8, this will be set in set_info */
  videolevels->nbins = 4096;

  g_free (videolevels->histogram);
  videolevels->histogram = NULL;
}

#define GINT_CLAMP(x, low, high) ((gint)(CLAMP((x),(low),(high))))
#define GUINT8_CLAMP(x, low, high) ((guint8)(CLAMP((x),(low),(high))))

/* TODO: rename this, as this handles uint8_to_uint8 as well */
static void
gst_videolevels_calculate_lut_uint16_to_uint8 (GstVideoLevels * videolevels,
    gint endianness)
{
  gint i;
  gdouble m;
  gdouble b;
  guint8 *lut = (guint8 *) videolevels->lookup_table;
  const guint16 max_in = (1 << videolevels->bpp_in) - 1;
  guint16 low_in;
  guint16 high_in;
  const guint8 max_out = (1 << videolevels->info_out.finfo->bits) - 1;
  const guint8 low_out = videolevels->lower_output;
  const guint8 high_out = videolevels->upper_output;

  if (videolevels->lower_input < 0 || videolevels->lower_input > max_in) {
    videolevels->lower_input = 0;
    g_object_notify_by_pspec (G_OBJECT (videolevels), properties[PROP_LOWIN]);
  }
  if (videolevels->upper_input < 0 || videolevels->upper_input > max_in) {
    videolevels->upper_input = max_in;
    g_object_notify_by_pspec (G_OBJECT (videolevels), properties[PROP_HIGHIN]);
  }

  gst_videolevels_check_passthrough (videolevels);

  low_in = videolevels->lower_input;
  high_in = videolevels->upper_input;

  GST_LOG_OBJECT (videolevels, "Make linear LUT mapping (%d, %d) -> (%d, %d)",
      low_in, high_in, low_out, high_out);

  if (low_in == high_in)
    m = 0.0;
  else
    m = (high_out - low_out) / (gdouble) (high_in - low_in);

  b = low_out - m * low_in;

  if (endianness == G_LITTLE_ENDIAN)
    for (i = 0; i < G_MAXUINT16; i++)
      lut[i] = GUINT8_CLAMP (m * GUINT16_FROM_LE (i) + b, low_out, high_out);
  else if (endianness == G_BIG_ENDIAN)
    for (i = 0; i < G_MAXUINT16; i++)
      lut[i] = GUINT8_CLAMP (m * GUINT16_FROM_BE (i) + b, low_out, high_out);
  else
    g_assert_not_reached ();
}

/**
 * gst_videolevels_calculate_lut
 * @videolevels: #GstVideoLevels
 *
 * Update lookup tables based on input and output levels
 */
static gboolean
gst_videolevels_calculate_lut (GstVideoLevels * videolevels)
{
  if (!videolevels->lookup_table) {
    videolevels->lookup_table = g_new (guint8, G_MAXUINT16);
  }

  switch (videolevels->info_in.finfo->format) {
    case GST_VIDEO_FORMAT_UNKNOWN:
      /* no format set yet, don't do anything */
      break;
    case GST_VIDEO_FORMAT_GRAY8:
      GST_LOG_OBJECT (videolevels, "Calculating lookup table uint8 -> uint8");
      gst_videolevels_calculate_lut_uint16_to_uint8 (videolevels,
          G_LITTLE_ENDIAN);
      break;
    case GST_VIDEO_FORMAT_GRAY16_LE:
      GST_LOG_OBJECT (videolevels,
          "Calculating lookup table uint16le -> uint8");
      gst_videolevels_calculate_lut_uint16_to_uint8 (videolevels,
          G_LITTLE_ENDIAN);
      break;
    case GST_VIDEO_FORMAT_GRAY16_BE:
      GST_LOG_OBJECT (videolevels,
          "Calculating lookup table uint16be -> uint8");
      gst_videolevels_calculate_lut_uint16_to_uint8 (videolevels, G_BIG_ENDIAN);
      break;
    default:
      g_assert_not_reached ();
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
  gint stride = GST_VIDEO_INFO_COMP_STRIDE (&videolevels->info_in, 0);
  gint endianness;
  gint maxVal = (1 << videolevels->bpp_in) - 1;
  if (videolevels->info_in.finfo->format == GST_VIDEO_FORMAT_GRAY16_BE)
    endianness = G_BIG_ENDIAN;
  else if (videolevels->info_in.finfo->format == GST_VIDEO_FORMAT_GRAY16_LE)
    endianness = G_LITTLE_ENDIAN;
  else
    endianness = G_BYTE_ORDER;

  factor = (nbins - 1.0) / maxVal;

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
      for (r = 0; r < videolevels->info_in.height; r++) {
        for (c = 0; c < videolevels->info_in.width; c++) {
          hist[GINT_CLAMP (data[c + r * stride / 2] * factor, 0, nbins - 1)]++;
        }
      }
    } else {
      for (r = 0; r < videolevels->info_in.height; r++) {
        for (c = 0; c < videolevels->info_in.width; c++) {
          hist[GINT_CLAMP (GUINT16_FROM_BE (data[c +
                          r * stride / 2]) * factor, 0, nbins - 1)]++;
        }
      }
    }
  } else {
    guint8 *data8 = (guint8 *) data;
    for (r = 0; r < videolevels->info_in.height; r++) {
      for (c = 0; c < videolevels->info_in.width; c++) {
        hist[GINT_CLAMP (data8[c + r * stride / 2] * factor, 0, nbins - 1)]++;
      }
    }
  }

  return TRUE;
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
gst_videolevels_auto_adjust (GstVideoLevels * videolevels, guint16 * data)
{
  guint npixsat;
  guint sum;
  gint i;
  gint size;
  gint minVal = 0;
  gint maxVal = (1 << videolevels->bpp_in) - 1;
  float factor = maxVal / (videolevels->nbins - 1.0f);
  gst_videolevels_calculate_histogram (videolevels, data);

  size = videolevels->info_in.width * videolevels->info_in.height;

  /* pixels to saturate on low end */
  npixsat = (guint) (videolevels->lower_pix_sat * size);
  sum = 0;
  for (i = 0; i < videolevels->nbins; i++) {
    sum += videolevels->histogram[i];
    if (sum > npixsat) {
      videolevels->lower_input = CLAMP (i * factor, minVal, maxVal);
      break;
    }
  }

  /* pixels to saturate on high end */
  npixsat = (guint) (videolevels->upper_pix_sat * size);
  sum = 0;
  for (i = videolevels->nbins - 1; i >= 0; i--) {
    sum += videolevels->histogram[i];
    if (sum > npixsat) {
      videolevels->upper_input = CLAMP (i * factor, minVal, maxVal);
      break;
    }
  }

  gst_videolevels_calculate_lut (videolevels);

  GST_LOG_OBJECT (videolevels, "Contrast stretch with npixsat=%d, (%d, %d)",
      npixsat, videolevels->lower_input, videolevels->upper_input);

  g_object_notify_by_pspec (G_OBJECT (videolevels), properties[PROP_LOWIN]);
  g_object_notify_by_pspec (G_OBJECT (videolevels), properties[PROP_HIGHIN]);

  return TRUE;
}

static void
gst_videolevels_check_passthrough (GstVideoLevels * levels)
{
  gboolean passthrough;
  if (levels->info_in.finfo->format == GST_VIDEO_FORMAT_GRAY8 &&
      levels->lower_input == levels->lower_output &&
      levels->upper_input == levels->upper_output) {
    passthrough = TRUE;
  } else {
    passthrough = FALSE;
  }
  if (passthrough != levels->passthrough) {
    levels->passthrough = passthrough;
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (levels),
        levels->passthrough);
  }
}
