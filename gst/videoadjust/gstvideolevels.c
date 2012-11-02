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

#include <string.h>
#include <math.h>

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

#define DEFAULT_PROP_LOWIN  0.0
#define DEFAULT_PROP_HIGHIN  1.0
#define DEFAULT_PROP_LOWOUT  0.0
#define DEFAULT_PROP_HIGHOUT  1.0
#define DEFAULT_PROP_AUTO 0
#define DEFAULT_PROP_INTERVAL (GST_SECOND / 2)

static const GstElementDetails videolevels_details =
GST_ELEMENT_DETAILS ("Video videolevels adjustment",
    "Filter/Effect/Video",
    "Adjusts videolevels on a video stream",
    "Joshua Doe <oss@nvl.army.mil");

/* the capabilities of the inputs and outputs */
static GstStaticPadTemplate gst_videolevels_src_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_GRAY16 ("BIG_ENDIAN") ";"
        GST_VIDEO_CAPS_GRAY16 ("LITTLE_ENDIAN"))
    );

static GstStaticPadTemplate gst_videolevels_sink_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_GRAY8)
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
    GstPadDirection direction, GstCaps * caps);
static gboolean gst_videolevels_set_caps (GstBaseTransform * base,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_videolevels_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_videolevels_get_unit_size (GstBaseTransform * base,
    GstCaps * caps, guint * size);
static GstFlowReturn gst_videolevels_prepare_output_buffer (GstBaseTransform *
    base, GstBuffer * in_buf, gint size, GstCaps * caps, GstBuffer ** out_buf);

/* GstVideoLevels method declarations */
static void gst_videolevels_reset (GstVideoLevels * filter);
static gboolean gst_videolevels_calculate_lut (GstVideoLevels * videolevels);
static gboolean gst_videolevels_do_levels (GstVideoLevels * videolevels,
    gpointer indata, gpointer outdata);
static gboolean gst_videolevels_calculate_histogram (GstVideoLevels *
    videolevels, guint16 * data);
static gboolean gst_videolevels_auto_adjust (GstVideoLevels * videolevels,
    guint16 * data);

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (videolevels_debug);
#define GST_CAT_DEFAULT videolevels_debug
#define DEBUG_INIT(bla) \
    GST_DEBUG_CATEGORY_INIT (videolevels_debug, "videolevels", 0, \
    "Video Levels Filter");

GST_BOILERPLATE_FULL (GstVideoLevels, gst_videolevels, GstVideoFilter,
    GST_TYPE_VIDEO_FILTER, DEBUG_INIT);

/************************************************************************/
/* GObject vmethod implementations                                      */
/************************************************************************/

/**
 * gst_videolevels_base_init:
 * @klass: #GstElementClass.
 *
 */
static void
gst_videolevels_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG ("base init");

  gst_element_class_set_details (element_class, &videolevels_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_videolevels_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_videolevels_src_template));
}

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
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

/**
 * gst_videolevels_class_init:
 * @object: #GstVideoLevelsClass.
 *
 */
static void
gst_videolevels_class_init (GstVideoLevelsClass * object)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (object);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (object);

  GST_DEBUG ("class init");


  /* Register GObject vmethods */
  obj_class->dispose = GST_DEBUG_FUNCPTR (gst_videolevels_dispose);
  obj_class->set_property = GST_DEBUG_FUNCPTR (gst_videolevels_set_property);
  obj_class->get_property = GST_DEBUG_FUNCPTR (gst_videolevels_get_property);

  /* Install GObject properties */
  properties[PROP_LOWIN] =
      g_param_spec_double ("lower-input-level", "Lower Input Level",
      "Lower Input Level", 0.0, 1.0, DEFAULT_PROP_LOWIN,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  properties[PROP_HIGHIN] =
      g_param_spec_double ("upper-input-level", "Upper Input Level",
      "Upper Input Level", 0.0, 1.0, DEFAULT_PROP_HIGHIN,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  properties[PROP_LOWOUT] =
      g_param_spec_double ("lower-output-level", "Lower Output Level",
      "Lower Output Level", 0.0, 1.0, DEFAULT_PROP_LOWOUT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  properties[PROP_HIGHOUT] =
      g_param_spec_double ("upper-output-level", "Upper Output Level",
      "Upper Output Level", 0.0, 1.0, DEFAULT_PROP_HIGHOUT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (obj_class, PROP_LOWIN,
      properties[PROP_LOWIN]);
  g_object_class_install_property (obj_class, PROP_HIGHIN,
      properties[PROP_HIGHIN]);
  g_object_class_install_property (obj_class, PROP_LOWOUT,
      properties[PROP_LOWOUT]);
  g_object_class_install_property (obj_class, PROP_HIGHOUT,
      properties[PROP_HIGHOUT]);
  g_object_class_install_property (obj_class, PROP_AUTO,
      g_param_spec_enum ("auto", "Auto Adjust", "Auto adjust contrast",
          GST_TYPE_VIDEOLEVELS_AUTO, DEFAULT_PROP_AUTO, G_PARAM_READWRITE));
  g_object_class_install_property (obj_class, PROP_INTERVAL,
      g_param_spec_uint64 ("interval", "Interval",
          "Interval of time between adjustments (in nanoseconds)", 1,
          G_MAXUINT64, DEFAULT_PROP_INTERVAL, G_PARAM_READWRITE));

  /* Register GstBaseTransform vmethods */
  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_videolevels_transform_caps);
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_videolevels_set_caps);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_videolevels_transform);
  trans_class->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_videolevels_get_unit_size);
  trans_class->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_videolevels_prepare_output_buffer);
}

/**
* gst_videolevels_init:
* @videolevels: GstVideoLevels
* @g_class: GstVideoLevelsClass
*
* Initialize the new element
*/
static void
gst_videolevels_init (GstVideoLevels * videolevels,
    GstVideoLevelsClass * g_class)
{
  GST_DEBUG_OBJECT (videolevels, "init class instance");

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

  GST_DEBUG ("setting property %s", pspec->name);

  switch (prop_id) {
    case PROP_LOWIN:
      videolevels->lower_input = g_value_get_double (value);
      gst_videolevels_calculate_lut (videolevels);
      break;
    case PROP_HIGHIN:
      videolevels->upper_input = g_value_get_double (value);
      gst_videolevels_calculate_lut (videolevels);
      break;
    case PROP_LOWOUT:
      videolevels->lower_output = g_value_get_double (value);
      gst_videolevels_calculate_lut (videolevels);
      break;
    case PROP_HIGHOUT:
      videolevels->upper_output = g_value_get_double (value);
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

  GST_DEBUG ("getting property %s", pspec->name);

  switch (prop_id) {
    case PROP_LOWIN:
      g_value_set_double (value, videolevels->lower_input);
      break;
    case PROP_HIGHIN:
      g_value_set_double (value, videolevels->upper_input);
      break;
    case PROP_LOWOUT:
      g_value_set_double (value, videolevels->lower_output);
      break;
    case PROP_HIGHOUT:
      g_value_set_double (value, videolevels->upper_output);
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
static GstCaps *
gst_videolevels_transform_caps (GstBaseTransform * base,
    GstPadDirection direction, GstCaps * caps)
{
  GstVideoLevels *videolevels;
  GstCaps *newcaps;
  GstStructure *structure;

  videolevels = GST_VIDEOLEVELS (base);

  GST_DEBUG_OBJECT (caps, "transforming caps (from)");

  newcaps = gst_caps_copy (caps);

  /* finish settings caps of the opposite pad */
  if (direction == GST_PAD_SINK) {
    GST_DEBUG ("Pad direction is sink");
    gst_caps_set_simple (newcaps,
        "bpp", G_TYPE_INT, 8, "depth", G_TYPE_INT, 8, NULL);
    structure = gst_caps_get_structure (newcaps, 0);
    gst_structure_remove_field (structure, "endianness");
  } else {
    GValue endianness = { 0 };
    GValue ival = { 0 };

    GST_DEBUG ("Pad direction is src");

    gst_caps_set_simple (newcaps,
        "bpp", GST_TYPE_INT_RANGE, 1, 16, "depth", G_TYPE_INT, 16, NULL);
    structure = gst_caps_get_structure (newcaps, 0);

    /* add BIG/LITTLE endianness to caps */
    g_value_init (&ival, G_TYPE_INT);
    g_value_init (&endianness, GST_TYPE_LIST);
    g_value_set_int (&ival, G_LITTLE_ENDIAN);
    gst_value_list_append_value (&endianness, &ival);
    g_value_set_int (&ival, G_BIG_ENDIAN);
    gst_value_list_append_value (&endianness, &ival);
    gst_structure_set_value (structure, "endianness", &endianness);
  }
  GST_DEBUG_OBJECT (newcaps, "allowed caps are");

  return newcaps;
}

/**
 * gst_videolevels_set_caps:
 * base: #GstBaseTransform
 * incaps: #GstCaps
 * outcaps: #GstCaps
 * 
 * Notification of the actual caps set.
 *
 * Returns: TRUE on success
 */
static gboolean
gst_videolevels_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstVideoLevels *levels = GST_VIDEOLEVELS (base);
  GstStructure *structure;
  gboolean res;

  GST_DEBUG_OBJECT (levels,
      "set_caps: in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

  /* retrieve caps info */
  res = gst_video_format_parse_caps (incaps, &levels->format_in, &levels->width,
      &levels->height);
  res &= gst_video_format_parse_caps (outcaps, &levels->format_out, NULL, NULL);

  /* FIXME: gst_video_format_get_component_depth is broken in 0.10.36
     levels->bpp_in = gst_video_format_get_component_depth (levels->format_in, 0);
     levels->bpp_out = gst_video_format_get_component_depth (levels->format_out, 0); */
  structure = gst_caps_get_structure (incaps, 0);
  res &= gst_structure_get_int (structure, "bpp", &levels->bpp_in);
  structure = gst_caps_get_structure (outcaps, 0);
  res &= gst_structure_get_int (structure, "bpp", &levels->bpp_out);

  levels->framesize = gst_video_format_get_size (levels->format_out,
      levels->width, levels->height);

  if (!res || levels->framesize <= 0) {
    GST_ERROR_OBJECT (levels, "Failed to parse caps");
  }

  gst_videolevels_calculate_lut (levels);

  return res;
}

/**
 * gst_videolevels_get_unit_size:
 * @base: #GstBaseTransform
 * @caps: #GstCaps
 * @size: guint size of unit (one frame for video)
 *
 * Tells GstBaseTransform the size in bytes of an output frame from the given
 * caps.
 *
 * Returns: TRUE on success
 */
static gboolean
gst_videolevels_get_unit_size (GstBaseTransform * base, GstCaps * caps,
    guint * size)
{
  if (!gst_video_get_size_from_caps (caps, size)) {
    GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
        ("Unable to determine frame size from caps"));
    return FALSE;
  }

  GST_DEBUG ("Frame size is %d bytes", *size);
  return TRUE;
}

static GstFlowReturn
gst_videolevels_prepare_output_buffer (GstBaseTransform * base,
    GstBuffer * in_buf, gint size, GstCaps * caps, GstBuffer ** out_buf)
{
  GstVideoLevels *levels = GST_VIDEOLEVELS (base);
  GstFlowReturn ret = GST_FLOW_OK;

  ret = gst_pad_alloc_buffer (base->srcpad, 0, size, caps, out_buf);

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (levels,
        "Couldn't get pad to alloc buffer, creating one directly");
    *out_buf = gst_buffer_new_and_alloc (size);
  }

  return ret;
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
gst_videolevels_transform (GstBaseTransform * base, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstVideoLevels *videolevels = GST_VIDEOLEVELS (base);
  gpointer input;
  gpointer output;
  gboolean ret;
  GstClockTimeDiff elapsed;
  GstClockTime start =
      gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (base)));

  GST_DEBUG_OBJECT (videolevels, "Performing non-inplace transform");

  input = GST_BUFFER_DATA (inbuf);
  output = GST_BUFFER_DATA (outbuf);

  if (videolevels->auto_adjust == 1) {
    GST_DEBUG_OBJECT (videolevels, "Auto adjusting levels (once)");
    gst_videolevels_auto_adjust (videolevels, input);
    videolevels->auto_adjust = 0;
    g_object_notify (G_OBJECT (videolevels), "auto");
  } else if (videolevels->auto_adjust == 2) {
    elapsed =
        GST_CLOCK_DIFF (videolevels->last_auto_timestamp, inbuf->timestamp);
    if (videolevels->last_auto_timestamp == GST_CLOCK_TIME_NONE
        || elapsed >= (GstClockTimeDiff) videolevels->interval || elapsed < 0) {
      GST_DEBUG_OBJECT (videolevels, "Auto adjusting levels (%d ns since last)",
          elapsed);
      gst_videolevels_auto_adjust (videolevels, input);
      videolevels->last_auto_timestamp = GST_BUFFER_TIMESTAMP (inbuf);
    }
  }

  ret = gst_videolevels_do_levels (videolevels, input, output);

  GST_DEBUG_OBJECT (videolevels, "Processing took %" G_GINT64_FORMAT "ms",
      GST_TIME_AS_MSECONDS (GST_CLOCK_DIFF (start,
              gst_clock_get_time (gst_element_get_clock (GST_ELEMENT
                      (videolevels))))));

  if (ret)
    return GST_FLOW_OK;
  else
    return GST_FLOW_ERROR;
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
  videolevels->width = 0;
  videolevels->height = 0;
  videolevels->format_in = GST_VIDEO_FORMAT_UNKNOWN;
  videolevels->format_out = GST_VIDEO_FORMAT_UNKNOWN;
  videolevels->bpp_in = 0;
  videolevels->bpp_out = 0;

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

  videolevels->nbins = 4096;

  g_free (videolevels->histogram);
  videolevels->histogram = NULL;
}

#define GINT_CLAMP(x, low, high) ((gint)(CLAMP((x),(low),(high))))
#define GUINT8_CLAMP(x, low, high) ((guint8)(CLAMP((x),(low),(high))))

static void
gst_videolevels_calculate_lut_uint16_to_uint8 (GstVideoLevels * videolevels,
    gint endianness)
{
  gint i;
  gdouble m;
  gdouble b;
  guint8 *lut = (guint8 *) videolevels->lookup_table;
  const guint16 max_in = (1 << videolevels->bpp_in) - 1;
  const guint16 low_in = (guint16) (videolevels->lower_input * max_in);
  const guint16 high_in = (guint16) (videolevels->upper_input * max_in);
  const guint8 max_out = (1 << videolevels->bpp_out) - 1;
  const guint8 low_out = (guint8) (videolevels->lower_output * max_out);
  const guint8 high_out = (guint8) (videolevels->upper_output * max_out);


  GST_DEBUG ("Applying linear mapping (%d, %d) -> (%d, %d)",
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

  if (videolevels->format_in == GST_VIDEO_FORMAT_GRAY16_LE) {
    GST_DEBUG ("Calculating lookup table uint16le -> uint8");
    gst_videolevels_calculate_lut_uint16_to_uint8 (videolevels,
        G_LITTLE_ENDIAN);
  } else if (videolevels->format_in == GST_VIDEO_FORMAT_GRAY16_BE) {
    GST_DEBUG ("Calculating lookup table uint16be -> uint8");
    gst_videolevels_calculate_lut_uint16_to_uint8 (videolevels, G_BIG_ENDIAN);
  } else
    return FALSE;

  return TRUE;
}

/**
 * gst_videolevels_do_levels
 * @videolevels: #GstVideoLevels
 * @indata: input data
 * @outdata: output data
 * @size: size of data
 *
 * Convert frame using previously calculated LUT
 *
 * Returns: TRUE on success
 */
static gboolean
gst_videolevels_do_levels (GstVideoLevels * videolevels, gpointer indata,
    gpointer outdata)
{
  guint8 *dst = (guint8 *) outdata;
  guint16 *src = (guint16 *) indata;
  guint8 *lut = (guint8 *) videolevels->lookup_table;
  int i;

  GST_DEBUG ("Converting frame using LUT");

  for (i = 0; i < videolevels->framesize; i++)
    dst[i] = lut[src[i]];

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
  gint stride = gst_video_format_get_row_stride (videolevels->format_in, 0,
      videolevels->width);
  gint endianness;

  /* TODO: add gst_video_format_get_endianness to video library */
  if (videolevels->format_in == GST_VIDEO_FORMAT_GRAY16_BE)
    endianness = G_BIG_ENDIAN;
  else if (videolevels->format_in == GST_VIDEO_FORMAT_GRAY16_LE)
    endianness = G_LITTLE_ENDIAN;
  else
    endianness = G_BYTE_ORDER;


  factor = nbins / (gfloat) (1 << videolevels->bpp_in);

  if (videolevels->histogram == NULL) {
    GST_DEBUG ("First call, allocate memory for histogram (%d bins)", nbins);
    videolevels->histogram = g_new (gint, nbins);
  }

  hist = videolevels->histogram;

  /* reset histogram */
  memset (hist, 0, sizeof (gint) * nbins);

  GST_DEBUG ("Calculating histogram");
  if (endianness == G_BYTE_ORDER) {
    for (r = 0; r < videolevels->height; r++) {
      for (c = 0; c < videolevels->width; c++) {
        hist[GINT_CLAMP (data[c + r * stride / 2] * factor, 0, nbins - 1)]++;
      }
    }
  } else {
    for (r = 0; r < videolevels->height; r++) {
      for (c = 0; c < videolevels->width; c++) {
        hist[GINT_CLAMP (GUINT16_FROM_BE (data[c +
                        r * stride / 2]) * factor, 0, nbins - 1)]++;
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
  gdouble min = 0.0;
  gdouble max = (1 << videolevels->bpp_in) - 1.0;

  gst_videolevels_calculate_histogram (videolevels, data);

  size = videolevels->width * videolevels->height;

  /* pixels to saturate on low end */
  npixsat = (guint) (videolevels->lower_pix_sat * size);
  sum = 0;
  for (i = 0; i < videolevels->nbins; i++) {
    sum += videolevels->histogram[i];
    if (sum > npixsat) {
      videolevels->lower_input =
          CLAMP (i / (gdouble) videolevels->nbins, 0.0, 1.0);
      break;
    }
  }

  /* pixels to saturate on high end */
  npixsat = (guint) (videolevels->upper_pix_sat * size);
  sum = 0;
  for (i = videolevels->nbins - 1; i >= 0; i--) {
    sum += videolevels->histogram[i];
    if (sum > npixsat) {
      videolevels->upper_input =
          CLAMP ((i + 1) / (gdouble) videolevels->nbins, 0.0, 1.0);
      break;
    }
  }

  gst_videolevels_calculate_lut (videolevels);

  GST_LOG_OBJECT (videolevels, "Contrast stretch with npixsat=%d, (%.6f, %.6f)",
      npixsat, videolevels->lower_input, videolevels->upper_input);

  g_object_notify_by_pspec (G_OBJECT (videolevels), properties[PROP_LOWIN]);
  g_object_notify_by_pspec (G_OBJECT (videolevels), properties[PROP_HIGHIN]);

  return TRUE;
}
