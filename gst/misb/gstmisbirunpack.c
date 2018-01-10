/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2003 Arwed v. Merkatz <v.merkatz@gmx.net>
 * Copyright (C) 2006 Mark Nauwelaerts <manauw@skynet.be>
 * Copyright (C) 2018 United States Government, Joshua M. Doe <oss@nvl.army.mil>
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
* SECTION:element-misbirunpack
*
* Unpack MISB IR packed video to GRAY16.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch videotestsrc ! misbirpack ! misbirunpack ! ffmpegcolorspace ! autovideosink
* ]|
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmisbirunpack.h"

#include <gst/video/video.h>

//#include "gstextractcolororc-dist.h"

/* GstMisbIrUnpack signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_OFFSET,
  PROP_LAST
};

#define DEFAULT_PROP_OFFSET 64

/* the capabilities of the inputs and outputs */
static GstStaticPadTemplate gst_misb_ir_unpack_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("v210"))
    );

static GstStaticPadTemplate gst_misb_ir_unpack_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("GRAY16_LE"))
    );


/* GObject vmethod declarations */
static void gst_misb_ir_unpack_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_misb_ir_unpack_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_misb_ir_unpack_dispose (GObject * object);

/* GstBaseTransform vmethod declarations */
static GstCaps *gst_misb_ir_unpack_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps);

/* GstVideoFilter vmethod declarations */
static gboolean gst_misb_ir_unpack_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static GstFlowReturn gst_misb_ir_unpack_transform_frame (GstVideoFilter *
    filter, GstVideoFrame * in_frame, GstVideoFrame * out_frame);

/* GstMisbIrUnpack method declarations */
static void gst_misb_ir_unpack_reset (GstMisbIrUnpack * filter);

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (misb_ir_unpack_debug);
#define GST_CAT_DEFAULT misb_ir_unpack_debug

G_DEFINE_TYPE (GstMisbIrUnpack, gst_misb_ir_unpack, GST_TYPE_VIDEO_FILTER);

/************************************************************************/
/* GObject vmethod implementations                                      */
/************************************************************************/

/**
 * gst_misb_ir_unpack_dispose:
 * @object: #GObject.
 *
 */
static void
gst_misb_ir_unpack_dispose (GObject * object)
{
  GstMisbIrUnpack *misb_ir_unpack = GST_MISB_IR_UNPACK (object);

  GST_DEBUG ("dispose");

  gst_misb_ir_unpack_reset (misb_ir_unpack);

  /* chain up to the parent class */
  G_OBJECT_CLASS (gst_misb_ir_unpack_parent_class)->dispose (object);
}

/**
 * gst_misb_ir_unpack_class_init:
 * @object: #GstMisbIrUnpackClass.
 *
 */
static void
gst_misb_ir_unpack_class_init (GstMisbIrUnpackClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *gstbasetransform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *gstvideofilter_class = GST_VIDEO_FILTER_CLASS (klass);

  GST_DEBUG ("class init");

  /* Register GObject vmethods */
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_misb_ir_unpack_dispose);
  gobject_class->set_property =
      GST_DEBUG_FUNCPTR (gst_misb_ir_unpack_set_property);
  gobject_class->get_property =
      GST_DEBUG_FUNCPTR (gst_misb_ir_unpack_get_property);

  /* Install GObject properties */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_OFFSET, g_param_spec_int ("offset",
          "Offset value",
          "Offset value to apply during unpacking", 0, 1023,
          DEFAULT_PROP_OFFSET, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_misb_ir_unpack_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_misb_ir_unpack_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Unpack MISB IR video", "Filter/Effect/Video",
      "Unpack MISB IR video according to ST 0402.2 Method 2",
      "Joshua M. Doe <oss@nvl.army.mil>");

  /* Register GstBaseTransform vmethods */
  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_misb_ir_unpack_transform_caps);

  gstvideofilter_class->set_info =
      GST_DEBUG_FUNCPTR (gst_misb_ir_unpack_set_info);
  gstvideofilter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_misb_ir_unpack_transform_frame);
}

static void
gst_misb_ir_unpack_init (GstMisbIrUnpack * filt)
{
  GST_DEBUG_OBJECT (filt, "init class instance");

  filt->offset_value = DEFAULT_PROP_OFFSET;
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (filt), FALSE);

  gst_misb_ir_unpack_reset (filt);
}

static void
gst_misb_ir_unpack_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMisbIrUnpack *filt = GST_MISB_IR_UNPACK (object);

  GST_DEBUG_OBJECT (filt, "setting property %s", pspec->name);

  switch (prop_id) {
    case PROP_OFFSET:
      filt->offset_value = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_misb_ir_unpack_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMisbIrUnpack *filt = GST_MISB_IR_UNPACK (object);

  GST_DEBUG_OBJECT (filt, "getting property %s", pspec->name);

  switch (prop_id) {
    case PROP_OFFSET:
      g_value_set_enum (value, filt->offset_value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

GstCaps *
gst_misb_ir_unpack_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  GstMisbIrUnpack *filt = GST_MISB_IR_UNPACK (trans);
  GstStructure *structure, *newstruct;
  GstCaps *newcaps;
  guint i, n;

  GST_LOG_OBJECT (filt, "transforming caps from %" GST_PTR_FORMAT, caps);

  newcaps = gst_caps_new_empty ();
  n = gst_caps_get_size (caps);
  for (i = 0; i < n; ++i) {
    structure = gst_caps_get_structure (caps, i);
    if (direction == GST_PAD_SINK) {
      newstruct =
          gst_structure_new_from_string ("video/x-raw,format=GRAY16_LE");
    } else {
      newstruct = gst_structure_new_from_string ("video/x-raw,format=v210");
    }

    gst_structure_set_value (newstruct, "width",
        gst_structure_get_value (structure, "width"));
    gst_structure_set_value (newstruct, "height",
        gst_structure_get_value (structure, "height"));
    gst_structure_set_value (newstruct, "framerate",
        gst_structure_get_value (structure, "framerate"));

    gst_caps_append_structure (newcaps, newstruct);
  }


  if (!gst_caps_is_empty (newcaps) && filter_caps) {
    GstCaps *tmp = gst_caps_intersect_full (filter_caps, newcaps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_replace (&newcaps, tmp);
    gst_caps_unref (tmp);
  }

  GST_LOG_OBJECT (filt, "transformed caps to %" GST_PTR_FORMAT, newcaps);

  return newcaps;
}

static gboolean
gst_misb_ir_unpack_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstMisbIrUnpack *filt = GST_MISB_IR_UNPACK (filter);
  gboolean res = TRUE;

  GST_DEBUG_OBJECT (filt,
      "set_caps: in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

  memcpy (&filt->info_in, in_info, sizeof (GstVideoInfo));
  memcpy (&filt->info_out, out_info, sizeof (GstVideoInfo));

  return res;
}

static GstFlowReturn
gst_misb_ir_unpack_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * in_frame, GstVideoFrame * out_frame)
{
  GstMisbIrUnpack *filt = GST_MISB_IR_UNPACK (filter);
  GTimer *timer = NULL;
  guint offset = filt->offset_value;
  gint x, y;
  guint32 *src;
  guint16 *dst;

  GST_LOG_OBJECT (filt, "Performing non-inplace transform");

#if 0
  timer = g_timer_new ();
#endif

  for (y = 0; y < GST_VIDEO_FRAME_COMP_HEIGHT (in_frame, 0); y++) {
    src = (guint32 *) (GST_VIDEO_FRAME_COMP_DATA (in_frame, 0) +
        y * GST_VIDEO_FRAME_COMP_STRIDE (in_frame, 0));
    dst = (guint16 *) (GST_VIDEO_FRAME_COMP_DATA (out_frame, 0) +
        y * GST_VIDEO_FRAME_COMP_STRIDE (out_frame, 0));
    for (x = 0; x < GST_VIDEO_FRAME_COMP_WIDTH (in_frame, 0);) {
      guint32 word0 = *src++;
      guint32 word1 = *src++;
      guint16 luma, chroma;

      chroma = word0 & 0x3ff;
      luma = (word0 & 0xffc00) >> 10;
      dst[x++] = ((chroma - offset) & 0xff) | (((luma - offset) & 0xff) << 8);

      chroma = (word0 & 0x3ff00000) >> 20;
      luma = word1 & 0x3ff;
      dst[x++] = (chroma - offset) & 0xff | ((luma - offset) & 0xff) << 8;

      chroma = (word1 & 0xffc00) >> 10;
      luma = (word1 & 0x3ff00000) >> 20;
      dst[x++] = (chroma - offset) & 0xff | ((luma - offset) & 0xff) << 8;
    }
  }

#if 0
  GST_LOG_OBJECT (filt, "Processing took %.3f ms", g_timer_elapsed (timer,
          NULL) * 1000);
  g_timer_destroy (timer);
#endif

  return GST_FLOW_OK;
}


static void
gst_misb_ir_unpack_reset (GstMisbIrUnpack * misb_ir_unpack)
{
  gst_video_info_init (&misb_ir_unpack->info_in);
  gst_video_info_init (&misb_ir_unpack->info_out);
}
