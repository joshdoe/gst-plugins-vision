/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2003 Arwed v. Merkatz <v.merkatz@gmx.net>
 * Copyright (C) 2006 Mark Nauwelaerts <manauw@skynet.be>
 * Copyright (C) 2020 United States Government, Joshua M. Doe <oss@nvl.army.mil>
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
* SECTION:element-bayer2gray
*
* Convert grayscale video from one bpp/depth combination to another.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch videotestsrc ! bayer2gray ! ffmpegcolorspace ! autovideosink
* ]|
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstbayer2gray.h"

#include <gst/video/video.h>

/* GstBayer2Gray signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_LAST
};

#define VIDEO_CAPS_MAKE_BAYER8(format)                       \
    "video/x-bayer, "                                        \
    "format = (string) " format ", "                         \
    "width = " GST_VIDEO_SIZE_RANGE ", "                     \
    "height = " GST_VIDEO_SIZE_RANGE ", "                    \
    "framerate = " GST_VIDEO_FPS_RANGE

#define VIDEO_CAPS_MAKE_BAYER16(format)                      \
    "video/x-bayer, "                                        \
    "format = (string) " format ", "                         \
    "endianness = (int) {1234, 4321}, "                      \
    "bpp = (int) {16, 14, 12, 10}, "                         \
    "width = " GST_VIDEO_SIZE_RANGE ", "                     \
    "height = " GST_VIDEO_SIZE_RANGE ", "                    \
    "framerate = " GST_VIDEO_FPS_RANGE

#define VIDEO_CAPS_BAYER8 VIDEO_CAPS_MAKE_BAYER8("{bggr,grbg,gbrg,rggb}")
#define VIDEO_CAPS_BAYER16 VIDEO_CAPS_MAKE_BAYER16("{bggr16,grbg16,gbrg16,rggb16}")

/* the capabilities of the inputs and outputs */
static GstStaticPadTemplate gst_bayer2gray_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS_BAYER8 ";" VIDEO_CAPS_BAYER16)
    );

static GstStaticPadTemplate gst_bayer2gray_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{GRAY16_LE, GRAY16_BE, GRAY8 }"))
    );


/* GObject vmethod declarations */
static void gst_bayer2gray_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_bayer2gray_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_bayer2gray_dispose (GObject * object);

/* GstBaseTransform vmethod declarations */
static GstCaps *gst_bayer2gray_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps);
static gboolean gst_bayer2gray_set_caps (GstBaseTransform * btrans,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_bayer2gray_transform (GstBaseTransform * btrans,
    GstBuffer * inbuf, GstBuffer * outbuf);
static GstFlowReturn gst_bayer2gray_transform_ip (GstBaseTransform * btrans,
    GstBuffer * buf);

/* GstBayer2Gray method declarations */
static void gst_bayer2gray_reset (GstBayer2Gray * filter);

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (bayer2gray_debug);
#define GST_CAT_DEFAULT bayer2gray_debug

G_DEFINE_TYPE (GstBayer2Gray, gst_bayer2gray, GST_TYPE_BASE_TRANSFORM);

/************************************************************************/
/* GObject vmethod implementations                                      */
/************************************************************************/

/**
 * gst_bayer2gray_dispose:
 * @object: #GObject.
 *
 */
static void
gst_bayer2gray_dispose (GObject * object)
{
  GstBayer2Gray *bayer2gray = GST_BAYER2GRAY (object);

  GST_DEBUG ("dispose");

  gst_bayer2gray_reset (bayer2gray);

  /* chain up to the parent class */
  G_OBJECT_CLASS (gst_bayer2gray_parent_class)->dispose (object);
}

/**
 * gst_bayer2gray_class_init:
 * @object: #GstBayer2GrayClass.
 *
 */
static void
gst_bayer2gray_class_init (GstBayer2GrayClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *gstbasetransform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (bayer2gray_debug, "bayer2gray", 0,
      "Bayer to Gray Filter");

  GST_DEBUG ("class init");

  /* Register GObject vmethods */
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_bayer2gray_dispose);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_bayer2gray_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_bayer2gray_get_property);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_bayer2gray_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_bayer2gray_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Bayer as gray", "Filter/Effect/Video",
      "Converts Bayer caps to gray caps", "Joshua M. Doe <oss@nvl.army.mil>");

  /* Register GstBaseTransform vmethods */
  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_bayer2gray_transform_caps);
  gstbasetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_bayer2gray_set_caps);
  gstbasetransform_class->transform =
      GST_DEBUG_FUNCPTR (gst_bayer2gray_transform);
  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_bayer2gray_transform_ip);
}

static void
gst_bayer2gray_init (GstBayer2Gray * filt)
{
  GST_DEBUG_OBJECT (filt, "init class instance");

  /* FIXME: fix in-place transform */
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (filt), FALSE);

  gst_bayer2gray_reset (filt);
}

static void
gst_bayer2gray_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBayer2Gray *filt = GST_BAYER2GRAY (object);

  GST_DEBUG_OBJECT (filt, "setting property %s", pspec->name);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_bayer2gray_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstBayer2Gray *filt = GST_BAYER2GRAY (object);

  GST_DEBUG_OBJECT (filt, "getting property %s", pspec->name);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

GstCaps *
gst_bayer2gray_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter_caps)
{
  GstBayer2Gray *filt = GST_BAYER2GRAY (trans);
  GstCaps *normalized_caps, *other_caps;
  GstCaps *bayer8_caps, *bayer16_caps, *gray8_caps, *gray16_caps;
  guint i, n;

  GST_LOG_OBJECT (filt, "transforming caps from %" GST_PTR_FORMAT, caps);

  other_caps = gst_caps_new_empty ();
  normalized_caps = gst_caps_normalize (gst_caps_ref (caps));
  gray8_caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("GRAY8"));
  gray16_caps =
      gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("{ GRAY16_LE, GRAY16_BE }"));
  bayer8_caps = gst_caps_from_string (VIDEO_CAPS_BAYER8);
  bayer16_caps = gst_caps_from_string (VIDEO_CAPS_BAYER16);

  n = gst_caps_get_size (normalized_caps);
  for (i = 0; i < n; ++i) {
    GstCaps *c = gst_caps_copy_nth (normalized_caps, i);
    GstStructure *s, *s_other;
    const GstCaps *tgt_caps = NULL;
    if (i > 0 && gst_caps_is_subset (other_caps, c))
      continue;

    if (direction == GST_PAD_SRC) {
      /* we're on gray side, return bayer caps */
      if (gst_caps_is_subset (c, gray8_caps)) {
        tgt_caps = bayer8_caps;
      } else {
        tgt_caps = bayer16_caps;
      }
    } else {
      /* we're on bayer side, return gray caps */
      if (gst_caps_is_subset (c, bayer8_caps)) {
        tgt_caps = gray8_caps;
      } else {
        tgt_caps = gray16_caps;
      }
    }
    s = gst_caps_get_structure (c, 0);
    s_other = gst_caps_get_structure (tgt_caps, 0);
    gst_structure_set_name (s, gst_structure_get_name (s_other));
    gst_structure_set_value (s, "format", gst_structure_get_value (s_other,
            "format"));

    gst_caps_merge (other_caps, c);
  }

  gst_caps_unref (gray8_caps);
  gst_caps_unref (gray16_caps);
  gst_caps_unref (bayer8_caps);
  gst_caps_unref (bayer16_caps);
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
gst_bayer2gray_set_caps (GstBaseTransform * btrans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstBayer2Gray *filt = GST_BAYER2GRAY (btrans);
  gboolean res = TRUE;
  GstVideoInfo vinfo;

  GST_DEBUG_OBJECT (filt,
      "set_caps: in '%" GST_PTR_FORMAT "' out '%" GST_PTR_FORMAT "'", incaps,
      outcaps);

  gst_video_info_from_caps (&filt->vinfo, outcaps);

  return res;
}

static GstFlowReturn
gst_bayer2gray_transform (GstBaseTransform * btrans,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstBayer2Gray *filt = GST_BAYER2GRAY (btrans);
  GstMapInfo minfo_in, minfo_out;

  GST_LOG_OBJECT (filt, "Doing non-inplace transform, copying data (FIX THIS)");

  gst_buffer_map (inbuf, &minfo_in, GST_MAP_READ);
  gst_buffer_map (outbuf, &minfo_out, GST_MAP_WRITE);
  memcpy (minfo_out.data, minfo_in.data, minfo_in.size);
  gst_buffer_unmap (inbuf, &minfo_in);
  gst_buffer_unmap (outbuf, &minfo_out);

  return GST_FLOW_OK;
}

/* FIXME: inplace transform doesn't work now due to buffer meta issue with
   videoconvert, or really at least all elements using videofilter base
   class */
static GstFlowReturn
gst_bayer2gray_transform_ip (GstBaseTransform * btrans, GstBuffer * buf)
{
  GstBayer2Gray *filt = GST_BAYER2GRAY (btrans);

  GST_LOG_OBJECT (filt, "in-place transform, doing nothing");

  gst_buffer_add_video_meta (buf, GST_VIDEO_FRAME_FLAG_NONE,
      filt->vinfo.finfo->format, filt->vinfo.width, filt->vinfo.height);
  GST_ERROR ("%d,%d,%d", filt->vinfo.finfo->format, filt->vinfo.width,
      filt->vinfo.height);
  return GST_FLOW_OK;
}

static void
gst_bayer2gray_reset (GstBayer2Gray * bayer2gray)
{
}

/* Register filters that make up the gstgl plugin */
static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "bayer2gray", 0, "bayer2gray");

  GST_DEBUG ("plugin_init");

  GST_CAT_INFO (GST_CAT_DEFAULT, "registering bayer2gray element");

  if (!gst_element_register (plugin, "bayer2gray", GST_RANK_NONE,
          GST_TYPE_BAYER2GRAY)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    bayerutils,
    "Plugins for working with Bayer video",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN);
