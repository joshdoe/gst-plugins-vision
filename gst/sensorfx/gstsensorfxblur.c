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
* SECTION:element-sfxblur
*
* Convert grayscale video from one bpp/depth combination to another.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch videotestsrc ! sfxblur ! ffmpegcolorspace ! autovideosink
* ]|
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>

#include <gst/video/video.h>

#include <cv.h>

#include "gstopencvutils.h"
#include "gstsensorfxblur.h"

/* GstSensorFxBlur signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_LOWIN,
};

#define DEFAULT_PROP_LOWIN  0.0

static const GstElementDetails sfxblur_details =
GST_ELEMENT_DETAILS ("Blurs video",
    "Filter/Effect/Video",
    "Applies a blur kernel to video",
    "Joshua Doe <oss@nvl.army.mil");

/* the capabilities of the inputs and outputs */
static GstStaticPadTemplate gst_sfxblur_src_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
      "video/x-raw-gray, "                                            \
      "bpp = (int) 16, "                                              \
      "depth = (int) 16, "                                            \
      "endianness = (int) BYTE_ORDER, "                               \
      "width = " GST_VIDEO_SIZE_RANGE ", "                            \
      "height = " GST_VIDEO_SIZE_RANGE ", "                           \
      "framerate = " GST_VIDEO_FPS_RANGE ";"                          \
      "video/x-raw-gray-float, "                                      \
      "bpp = (int) 32, "                                              \
      "depth = (int) 32, "                                            \
      "endianness = (int) BYTE_ORDER, "                               \
      "width = " GST_VIDEO_SIZE_RANGE ", "                            \
      "height = " GST_VIDEO_SIZE_RANGE ", "                           \
      "framerate = " GST_VIDEO_FPS_RANGE
      )
);

static GstStaticPadTemplate gst_sfxblur_sink_template =
GST_STATIC_PAD_TEMPLATE ("src",
     GST_PAD_SRC,
     GST_PAD_ALWAYS,
     GST_STATIC_CAPS (
       "video/x-raw-gray, "                                            \
       "bpp = (int) 16, "                                              \
       "depth = (int) 16, "                                            \
       "endianness = (int) BYTE_ORDER, "                               \
       "width = " GST_VIDEO_SIZE_RANGE ", "                            \
       "height = " GST_VIDEO_SIZE_RANGE ", "                           \
       "framerate = " GST_VIDEO_FPS_RANGE ";"                          \
       "video/x-raw-gray-float, "                                      \
       "bpp = (int) 32, "                                              \
       "depth = (int) 32, "                                            \
       "endianness = (int) BYTE_ORDER, "                               \
       "width = " GST_VIDEO_SIZE_RANGE ", "                            \
       "height = " GST_VIDEO_SIZE_RANGE ", "                           \
       "framerate = " GST_VIDEO_FPS_RANGE
     )
);

/* GObject vmethod declarations */
static void gst_sfxblur_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_sfxblur_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_sfxblur_finalize (GObject *object);

/* GstBaseTransform vmethod declarations */
static gboolean gst_sfxblur_set_caps (GstBaseTransform * base,
    GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn gst_sfxblur_transform_ip (GstBaseTransform * base,
    GstBuffer * buf);

/* GstSensorFxBlur method declarations */
static void gst_sfxblur_reset(GstSensorFxBlur* filter);

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (sfxblur_debug);
#define GST_CAT_DEFAULT sfxblur_debug
#define DEBUG_INIT(bla) \
    GST_DEBUG_CATEGORY_INIT (sfxblur_debug, "sfxblur", 0, \
    "sfxblur");

GST_BOILERPLATE_FULL (GstSensorFxBlur, gst_sfxblur, GstVideoFilter,
    GST_TYPE_VIDEO_FILTER, DEBUG_INIT);\


/************************************************************************/
/* GObject vmethod implementations                                      */
/************************************************************************/

/**
 * gst_sfxblur_base_init:
 * @klass: #GstElementClass.
 *
 */
static void
gst_sfxblur_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG ("base init");
  
  gst_element_class_set_details (element_class, &sfxblur_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_sfxblur_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_sfxblur_src_template));
}

/**
 * gst_sfxblur_finalize:
 * @object: #GObject.
 *
 */
static void
gst_sfxblur_finalize (GObject *object)
{
  GstSensorFxBlur *sfxblur = GST_SENSORFXBLUR (object);

  GST_DEBUG ("finalize");
  
  gst_sfxblur_reset (sfxblur);
  
  /* chain up to the parent class */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gst_sfxblur_class_init:
 * @object: #GstSensorFxBlurClass.
 *
 */
static void
gst_sfxblur_class_init (GstSensorFxBlurClass * object)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (object);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (object);

  GST_DEBUG ("class init");
  
 
  /* Register GObject vmethods */
  obj_class->finalize = GST_DEBUG_FUNCPTR (gst_sfxblur_finalize);
  obj_class->set_property = GST_DEBUG_FUNCPTR (gst_sfxblur_set_property);
  obj_class->get_property = GST_DEBUG_FUNCPTR (gst_sfxblur_get_property);

  /* Install GObject properties */
  g_object_class_install_property (obj_class, PROP_LOWIN,
      g_param_spec_double ("lower-input-level", "Lower Input Level", "Lower Input Level",
      0.0, 1.0, DEFAULT_PROP_LOWIN, G_PARAM_READWRITE));

  /* Register GstBaseTransform vmethods */
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_sfxblur_set_caps);
  trans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_sfxblur_transform_ip);
}

/**
* gst_sfxblur_init:
* @sfxblur: GstSensorFxBlur
* @g_class: GstSensorFxBlurClass
*
* Initialize the new element
*/
static void
gst_sfxblur_init (GstSensorFxBlur * sfxblur,
    GstSensorFxBlurClass * g_class)
{
  GST_DEBUG_OBJECT (sfxblur, "init class instance");

  gst_sfxblur_reset (sfxblur);

}

/**
 * gst_sfxblur_set_property:
 * @object: #GObject
 * @prop_id: guint
 * @value: #GValue
 * @pspec: #GParamSpec
 *
 */
static void
gst_sfxblur_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSensorFxBlur *sfxblur = GST_SENSORFXBLUR (object);

  GST_DEBUG ("setting property %s", pspec->name);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * gst_sfxblur_get_property:
 * @object: #GObject
 * @prop_id: guint
 * @value: #GValue
 * @pspec: #GParamSpec
 *
 */
static void
gst_sfxblur_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstSensorFxBlur *sfxblur = GST_SENSORFXBLUR (object);

  GST_DEBUG ("getting property %s", pspec->name);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/************************************************************************/
/* GstBaseTransform vmethod implementations                             */
/************************************************************************/

/**
 * gst_sfxblur_set_caps:
 * base: #GstBaseTransform
 * incaps: #GstCaps
 * outcaps: #GstCaps
 * 
 * Notification of the actual caps set.
 *
 * Returns: TRUE on acceptance of caps
 */
static gboolean
gst_sfxblur_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstSensorFxBlur *levels;
  GstStructure *structure;
  gboolean res;

  levels = GST_SENSORFXBLUR (base);

  GST_DEBUG_OBJECT (levels,
      "set_caps: in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

  GST_DEBUG_OBJECT (incaps, "incaps");
  GST_DEBUG_OBJECT (outcaps, "outcaps");

  /* retrieve input caps info */
  structure = gst_caps_get_structure (incaps, 0);

  //mime = gst_structure_get_name (structure);
  //if (g_strcmp0 ("video/x-raw-gray", mime) == 0)
  //  levels->

  res = gst_structure_get (structure,
      "width", G_TYPE_INT, &levels->width,
      "height", G_TYPE_INT, &levels->height,
      "bpp", G_TYPE_INT, &levels->bpp,
      "depth", G_TYPE_INT, &levels->depth,
      "endianness", G_TYPE_INT, &levels->endianness,
      NULL);
  if (!res)
    return FALSE;

  levels->stride = GST_ROUND_UP_4 (levels->width * levels->depth/8);

  return res;
}

GstFlowReturn gst_sfxblur_transform_ip( GstBaseTransform * base, GstBuffer * buf )
{

  return GST_FLOW_OK;
}

/************************************************************************/
/* GstSensorFxBlur method implementations                                */
/************************************************************************/

/**
 * gst_sfxblur_reset:
 * @sfxblur: #GstSensorFxBlur
 *
 * Reset instance variables and free memory
 */
static void
gst_sfxblur_reset(GstSensorFxBlur* sfxblur)
{
  sfxblur->width = 0;
  sfxblur->height = 0;
  sfxblur->stride = 0;
  sfxblur->bpp = 0;
  sfxblur->depth = 0;
  sfxblur->endianness = 0;
}

gboolean
gst_sfxblur_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "sfxblur", GST_RANK_NONE,
    GST_TYPE_SENSORFXBLUR);
}
