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
* SECTION:element-select
*
* Selects buffers from offset and skip.
*
* <refsect2>
* <title>Example launch line</title>
* |[
* gst-launch videotestsrc ! select ! autovideosink
* ]|
* </refsect2>
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstselect.h"

enum
{
  PROP_0,
  PROP_OFFSET,
  PROP_SKIP,
  PROP_LAST
};

#define DEFAULT_PROP_OFFSET 0
#define DEFAULT_PROP_SKIP 0

/* the capabilities of the inputs and outputs */
static GstStaticPadTemplate gst_select_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate gst_select_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );


/* GObject vmethod declarations */
static void gst_select_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_select_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_select_dispose (GObject * object);

/* GstBaseTransform vmethod declarations */
static GstFlowReturn gst_select_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);

/* GstSelect method declarations */
static void gst_select_reset (GstSelect * filter);

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (select_debug);
#define GST_CAT_DEFAULT select_debug

G_DEFINE_TYPE (GstSelect, gst_select, GST_TYPE_BASE_TRANSFORM);

/************************************************************************/
/* GObject vmethod implementations                                      */
/************************************************************************/

/**
 * gst_select_dispose:
 * @object: #GObject.
 *
 */
static void
gst_select_dispose (GObject * object)
{
  GstSelect *select = GST_SELECT (object);

  GST_DEBUG ("dispose");

  gst_select_reset (select);

  /* chain up to the parent class */
  G_OBJECT_CLASS (gst_select_parent_class)->dispose (object);
}

/**
 * gst_select_class_init:
 * @object: #GstSelectClass.
 *
 */
static void
gst_select_class_init (GstSelectClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *gstbasetransform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (select_debug, "select", 0, "Video Levels Filter");

  GST_DEBUG ("class init");

  /* Register GObject vmethods */
  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_select_dispose);
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_select_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_select_get_property);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_OFFSET,
      g_param_spec_int ("offset", "Buffer offset",
          "First buffer offset to pass", 0, G_MAXINT, DEFAULT_PROP_OFFSET,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_PLAYING));
  g_object_class_install_property (gobject_class, PROP_SKIP,
      g_param_spec_int ("skip", "Buffers to skip", "Number of buffers to skip",
          0, G_MAXINT, DEFAULT_PROP_OFFSET,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_PLAYING));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_select_sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_select_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Select buffer filter", "Filter/Effect",
      "Selects buffers based on buffer offset",
      "Joshua M. Doe <oss@nvl.army.mil>");

  /* Register GstBaseTransform vmethods */
  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_select_transform_ip);
}

static void
gst_select_init (GstSelect * trans)
{
  GST_DEBUG_OBJECT (trans, "init class instance");

  trans->offset = DEFAULT_PROP_OFFSET;
  trans->skip = DEFAULT_PROP_SKIP;

  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (trans), TRUE);

  gst_select_reset (trans);
}

static void
gst_select_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSelect *filt = GST_SELECT (object);

  GST_DEBUG_OBJECT (filt, "setting property %s", pspec->name);

  switch (prop_id) {
    case PROP_OFFSET:
      filt->offset = g_value_get_int (value);
      break;
    case PROP_SKIP:
      filt->skip = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_select_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstSelect *filt = GST_SELECT (object);

  GST_DEBUG_OBJECT (filt, "getting property %s", pspec->name);

  switch (prop_id) {
    case PROP_OFFSET:
      g_value_set_int (value, filt->offset);
      break;
    case PROP_SKIP:
      g_value_set_int (value, filt->skip);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_select_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstSelect *filt = GST_SELECT (trans);
  guint64 buf_offset = GST_BUFFER_OFFSET (buf);

  if (buf_offset < filt->offset) {
    GST_LOG_OBJECT (filt,
        "Dropping buffer %d since it's before the chosen offset %d",
        buf_offset, filt->offset);
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
  }

  if ((filt->offset - buf_offset) % (filt->skip + 1)) {
    GST_LOG_OBJECT (filt,
        "Dropping buffer %d since it's been chosen to be skipped", buf_offset);
    return GST_BASE_TRANSFORM_FLOW_DROPPED;
  }

  return GST_FLOW_OK;
}


static void
gst_select_reset (GstSelect * filt)
{
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "select", 0, "select");

  GST_DEBUG ("plugin_init");

  GST_CAT_INFO (GST_CAT_DEFAULT, "registering select element");

  if (!gst_element_register (plugin, "select", GST_RANK_NONE, GST_TYPE_SELECT)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    select,
    "Filter that selects which buffers to pass",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN);
