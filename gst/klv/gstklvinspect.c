/* GStreamer
 * Copyright (C) 2016 William Manley <will@williammanley.net>
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
/**
 * SECTION:element-gstklvinspect
 *
 * The klvinspect element inspects KLV metadata on passing buffers.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! klvinspect ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "gstklvinspect.h"
#include "klv.h"

GST_DEBUG_CATEGORY_STATIC (gst_klvinspect_debug_category);
#define GST_CAT_DEFAULT gst_klvinspect_debug_category

/* prototypes */
static GstFlowReturn gst_klvinspect_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);

enum
{
  PROP_0
};

/* pad templates */

#define SRC_CAPS "ANY"
#define SINK_CAPS "ANY"


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstKlvInspect, gst_klvinspect, GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_klvinspect_debug_category, "klvinspect", 0,
        "debug category for klvinspect element"));

static void
gst_klvinspect_class_init (GstKlvInspectClass * klass)
{
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (SRC_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Inspect KLV", "Filter", "Inspect KLV metadata",
      "Joshua M. Doe <oss@nvl.army.mil>");

  base_transform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_klvinspect_transform_ip);

  base_transform_class->passthrough_on_same_caps = TRUE;
  base_transform_class->transform_ip_on_passthrough = TRUE;
}

static void
gst_klvinspect_init (GstKlvInspect * filt)
{
}

static void
gst_klvinspect_dispose (GstKlvInspect * filt)
{
}

static GstFlowReturn
gst_klvinspect_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstKlvInspect *filt = GST_KLVINSPECT (trans);
  GstKLVMeta *klv_meta;
  gpointer iter = NULL;
  gint n_klv_meta_found = 0;

  while ((klv_meta = (GstKLVMeta *) gst_buffer_iterate_meta_filtered (buf,
              &iter, GST_KLV_META_API_TYPE))) {
    gsize klv_size;
    const guint8 *klv_data;
    klv_data = gst_klv_meta_get_data (klv_meta, &klv_size);
    if (klv_data) {
      GST_MEMDUMP_OBJECT (filt, "KLV data", klv_data, (guint) klv_size);
      ++n_klv_meta_found;
    }
  }

  GST_LOG_OBJECT (filt, "Found %d KLV meta", n_klv_meta_found);

  return GST_FLOW_OK;
}
