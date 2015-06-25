/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2003 Arwed v. Merkatz <v.merkatz@gmx.net>
 * Copyright (C) 2006 Mark Nauwelaerts <manauw@skynet.be>
 * Copyright (C) 2015 United States Government, Joshua M. Doe <oss@nvl.army.mil>
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
* SECTION:element-extractcolor
*
* Convert grayscale video from one bpp/depth combination to another.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch videotestsrc ! extractcolor ! ffmpegcolorspace ! autovideosink
* ]|
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstextractcolor.h"

#include <gst/video/video.h>

/* GstExtractColor signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_COMPONENT,
  PROP_LAST
};

#define DEFAULT_PROP_COMPONENT GST_EXTRACT_COLOR_COMPONENT_RED

#define RGB8_FORMATS "{ RGBx, BGRx, xRGB, xBGR, RGBA, BGRA, ARGB, ABGR, RGB, BGR }"
#define RGB16_FORMATS "ARGB64"

/* the capabilities of the inputs and outputs */
static GstStaticPadTemplate gst_extract_color_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (RGB8_FORMATS) ";"
        GST_VIDEO_CAPS_MAKE (RGB16_FORMATS))
    );

static GstStaticPadTemplate gst_extract_color_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("GRAY8") ";"
        GST_VIDEO_CAPS_MAKE ("GRAY16_LE"))
    );


#define GST_TYPE_EXTRACT_COLOR_COMPONENT (gst_extract_color_component_get_type())
static GType
gst_extract_color_component_get_type (void)
{
  static GType extract_color_component_type = 0;
  static const GEnumValue extract_color_component[] = {
    {GST_EXTRACT_COLOR_COMPONENT_RED, "extract red component", "red"},
    {GST_EXTRACT_COLOR_COMPONENT_GREEN, "extract green component", "green"},
    {GST_EXTRACT_COLOR_COMPONENT_BLUE, "extract blue component", "blue"},
    {0, NULL, NULL},
  };

  if (!extract_color_component_type) {
    extract_color_component_type =
        g_enum_register_static ("GstExtractColorComponent",
        extract_color_component);
  }
  return extract_color_component_type;
}

/* GObject vmethod declarations */
static void gst_extract_color_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_extract_color_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_extract_color_dispose (GObject * object);

/* GstBaseTransform vmethod declarations */
static GstCaps *gst_extract_color_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps);

/* GstVideoFilter vmethod declarations */
static gboolean gst_extract_color_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static GstFlowReturn gst_extract_color_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame);

/* GstExtractColor method declarations */
static void gst_extract_color_reset (GstExtractColor * filter);

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (extract_color_debug);
#define GST_CAT_DEFAULT extract_color_debug

G_DEFINE_TYPE (GstExtractColor, gst_extract_color, GST_TYPE_VIDEO_FILTER);

/************************************************************************/
/* GObject vmethod implementations                                      */
/************************************************************************/

/**
 * gst_extract_color_dispose:
 * @object: #GObject.
 *
 */
static void
gst_extract_color_dispose (GObject * object)
{
  GstExtractColor *extract_color = GST_EXTRACT_COLOR (object);

  GST_DEBUG ("dispose");

  gst_extract_color_reset (extract_color);

  /* chain up to the parent class */
  G_OBJECT_CLASS (gst_extract_color_parent_class)->dispose (object);
}

/**
 * gst_extract_color_class_init:
 * @object: #GstExtractColorClass.
 *
 */
static void
gst_extract_color_class_init (GstExtractColorClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *gstbasetransform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *gstvideofilter_class = GST_VIDEO_FILTER_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (extract_color_debug, "extract_color", 0,
      "Video Levels Filter");

  GST_DEBUG ("class init");

  /* Register GObject vmethods */
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_extract_color_dispose);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_extract_color_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_extract_color_get_property);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_COMPONENT,
      g_param_spec_enum ("component", "Component", "Component to extract",
          GST_TYPE_EXTRACT_COLOR_COMPONENT, DEFAULT_PROP_COMPONENT,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_PLAYING));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_extract_color_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_extract_color_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Extract color filter", "Filter/Effect/Video",
      "Extracts single color component from RGB video",
      "Joshua M. Doe <oss@nvl.army.mil>");

  /* Register GstBaseTransform vmethods */
  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_extract_color_transform_caps);

  gstvideofilter_class->set_info =
      GST_DEBUG_FUNCPTR (gst_extract_color_set_info);
  gstvideofilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_extract_color_transform_frame);
}

static void
gst_extract_color_init (GstExtractColor * filt)
{
  GST_DEBUG_OBJECT (filt, "init class instance");

  filt->component = DEFAULT_PROP_COMPONENT;
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (filt), FALSE);

  gst_extract_color_reset (filt);
}

static void
gst_extract_color_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstExtractColor *filt = GST_EXTRACT_COLOR (object);

  GST_DEBUG_OBJECT (filt, "setting property %s", pspec->name);

  switch (prop_id) {
    case PROP_COMPONENT:
      filt->component = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_extract_color_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstExtractColor *filt = GST_EXTRACT_COLOR (object);

  GST_DEBUG_OBJECT (filt, "getting property %s", pspec->name);

  switch (prop_id) {
    case PROP_COMPONENT:
      g_value_set_enum (value, filt->component);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

GstCaps *
gst_extract_color_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  GstExtractColor *filt = GST_EXTRACT_COLOR (trans);
  GstCaps *normalized_caps, *other_caps;
  GstCaps *rgb8_caps, *rgb16_caps, *gray8_caps, *gray16_caps;
  guint i, n;

  GST_LOG_OBJECT (filt, "transforming caps from %" GST_PTR_FORMAT, caps);

  other_caps = gst_caps_new_empty ();
  normalized_caps = gst_caps_normalize (gst_caps_ref (caps));
  gray8_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("GRAY8"));
  gray16_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("GRAY16_LE"));
  rgb8_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE (RGB8_FORMATS));
  rgb16_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE (RGB16_FORMATS));

  n = gst_caps_get_size (normalized_caps);
  for (i = 0; i < n; ++i) {
    GstCaps *c = gst_caps_copy_nth (normalized_caps, i);
    GstCaps *tgt_caps = NULL;
    const GValue *format_value;
    if (i > 0 && gst_caps_is_subset (other_caps, c))
      continue;

    if (direction == GST_PAD_SRC) {
      /* we're on gray side, return color caps */
      if (gst_caps_is_subset (c, gray8_caps)) {
        tgt_caps = rgb8_caps;
      } else {
        tgt_caps = rgb16_caps;
      }
    } else {
      /* we're on color side, return gray caps */
      if (gst_caps_is_subset (c, rgb8_caps)) {
        tgt_caps = gray8_caps;
      } else {
        tgt_caps = gray16_caps;
      }
    }

    format_value =
        gst_structure_get_value (gst_caps_get_structure (tgt_caps, 0),
        "format");
    gst_structure_set_value (gst_caps_get_structure (c, 0), "format",
        format_value);

    gst_caps_merge (other_caps, c);
  }

  gst_caps_unref (gray8_caps);
  gst_caps_unref (gray16_caps);
  gst_caps_unref (rgb8_caps);
  gst_caps_unref (rgb16_caps);
  gst_caps_unref (normalized_caps);

  if (!gst_caps_is_empty (other_caps) && filter_caps) {
    GstCaps *tmp = gst_caps_intersect_full (filter_caps, other_caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_replace (&other_caps, tmp);
    gst_caps_unref (tmp);
  }

  GST_LOG_OBJECT (filt, "transformed caps to %" GST_PTR_FORMAT, other_caps);

  return other_caps;
}

static gboolean
gst_extract_color_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstExtractColor *filt = GST_EXTRACT_COLOR (filter);
  gboolean res = TRUE;

  GST_DEBUG_OBJECT (filt,
      "set_caps: in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

  memcpy (&filt->info_in, in_info, sizeof (GstVideoInfo));
  memcpy (&filt->info_out, out_info, sizeof (GstVideoInfo));

  return res;
}

static GstFlowReturn
gst_extract_color_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstExtractColor *filt = GST_EXTRACT_COLOR (filter);
  GstClockTime start =
      gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (filt)));
  guint comp = filt->component;

  GST_LOG_OBJECT (filt, "Performing non-inplace transform");

  /* TODO: create orc functions for these to speed them up */
  if (GST_VIDEO_FRAME_COMP_DEPTH (in_frame, comp) == 8) {
    gint x, y;
    guint8 *src = GST_VIDEO_FRAME_COMP_DATA (in_frame, comp);
    guint8 *dst = GST_VIDEO_FRAME_COMP_DATA (out_frame, 0);
    const guint pstride = GST_VIDEO_FRAME_COMP_PSTRIDE (in_frame, comp);
    for (y = 0; y < GST_VIDEO_FRAME_COMP_HEIGHT (in_frame, comp); y++) {
      for (x = 0; x < GST_VIDEO_FRAME_COMP_WIDTH (in_frame, comp); x++) {
        dst[x] = src[x * pstride];
      }
      src += GST_VIDEO_FRAME_COMP_STRIDE (in_frame, comp);
      dst += GST_VIDEO_FRAME_COMP_STRIDE (out_frame, 0);
    }
  } else {
    gint x, y;
    guint16 *src = (guint16 *) GST_VIDEO_FRAME_COMP_DATA (in_frame, comp);
    guint16 *dst = (guint16 *) GST_VIDEO_FRAME_COMP_DATA (out_frame, 0);
    const guint pstride = GST_VIDEO_FRAME_COMP_PSTRIDE (in_frame, comp) / 2;
    for (y = 0; y < GST_VIDEO_FRAME_COMP_HEIGHT (in_frame, comp); y++) {
      for (x = 0; x < GST_VIDEO_FRAME_COMP_WIDTH (in_frame, comp); x++) {
        dst[x] = src[x * pstride];
      }
      src += GST_VIDEO_FRAME_COMP_STRIDE (in_frame, comp) / 2;
      dst += GST_VIDEO_FRAME_COMP_STRIDE (out_frame, 0) / 2;
    }
  }

  GST_LOG_OBJECT (filt, "Processing took %" G_GINT64_FORMAT "ms",
      GST_TIME_AS_MSECONDS (GST_CLOCK_DIFF (start,
              gst_clock_get_time (gst_element_get_clock (GST_ELEMENT
                      (filt))))));

  return GST_FLOW_OK;
}


static void
gst_extract_color_reset (GstExtractColor * extract_color)
{
  gst_video_info_init (&extract_color->info_in);
  gst_video_info_init (&extract_color->info_out);
}

/* Register filters that make up the gstgl plugin */
static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "extractcolor", 0, "extractcolor");

  GST_DEBUG ("plugin_init");

  GST_CAT_INFO (GST_CAT_DEFAULT, "registering extractcolor element");

  if (!gst_element_register (plugin, "extractcolor", GST_RANK_NONE,
          GST_TYPE_EXTRACT_COLOR)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    extract_color,
    "Filter that applies various hacks to a video stream",
    plugin_init, VERSION, GST_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN);
