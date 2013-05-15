/* GStreamer
 * Copyright (C) 2011 FIXME <fixme@example.com>
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
 * SECTION:element-gstedtpdvsink
 *
 * The edtpdvsink element is a sink for Camera Link simulators supported by the EDT PDV driver.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc ! ffmpegcolorspace ! edtpdvsink
 * ]|
 * Outputs test video on the default EDT Camera Link channel.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include <edtinc.h>
#include <clsim_lib.h>

#include "gstedtpdvsink.h"

/* GObject prototypes */
static void gst_edt_pdv_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_edt_pdv_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_edt_pdv_sink_dispose (GObject * object);

/* GstBaseSink prototypes */
static gboolean gst_edt_pdv_sink_start (GstBaseSink * basesink);
static gboolean gst_edt_pdv_sink_stop (GstBaseSink * basesink);
static GstCaps *gst_edt_pdv_sink_get_caps (GstBaseSink * basesink,
    GstCaps * filter_caps);
static gboolean gst_edt_pdv_sink_set_caps (GstBaseSink * basesink,
    GstCaps * caps);
static GstFlowReturn gst_edt_pdv_sink_render (GstBaseSink * basesink,
    GstBuffer * buffer);

enum
{
  PROP_0
};


/* pad templates */

static GstStaticPadTemplate gst_edt_pdv_sink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ GRAY8 }"))
    );

/* class initialization */

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (edtpdvsink_debug);
#define GST_CAT_DEFAULT edtpdvsink_debug

G_DEFINE_TYPE (GstEdtPdvSink, gst_edt_pdv_sink, GST_TYPE_BASE_SINK);

static void
gst_edt_pdv_sink_class_init (GstEdtPdvSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "edtpdvsink", 0,
      "EDT PDV Camera Link simulator sink");

  gobject_class->set_property = gst_edt_pdv_sink_set_property;
  gobject_class->get_property = gst_edt_pdv_sink_get_property;
  gobject_class->dispose = gst_edt_pdv_sink_dispose;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_edt_pdv_sink_sink_template));

  gst_element_class_set_details_simple (gstelement_class,
      "EDT PDV Sink", "Sink/Video",
      "EDT PDV Sink for Camera Link simulator boards",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_edt_pdv_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_edt_pdv_sink_stop);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_edt_pdv_sink_set_caps);
  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_edt_pdv_sink_get_caps);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_edt_pdv_sink_render);
}

static void
gst_edt_pdv_sink_init (GstEdtPdvSink * pdvsink)
{
  pdvsink->dev = NULL;
  pdvsink->buffers = NULL;
  pdvsink->n_buffers = 2;
  pdvsink->cur_buffer = 0;

  /* TODO: put these in properties */
  pdvsink->unit = 0;
  pdvsink->channel = 0;
}

void
gst_edt_pdv_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstEdtPdvSink *pdvsink;

  g_return_if_fail (GST_IS_EDT_PDV_SINK (object));
  pdvsink = GST_EDT_PDV_SINK (object);

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_edt_pdv_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstEdtPdvSink *pdvsink;

  g_return_if_fail (GST_IS_EDT_PDV_SINK (object));
  pdvsink = GST_EDT_PDV_SINK (object);

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_edt_pdv_sink_dispose (GObject * object)
{
  GstEdtPdvSink *pdvsink;

  g_return_if_fail (GST_IS_EDT_PDV_SINK (object));
  pdvsink = GST_EDT_PDV_SINK (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_edt_pdv_sink_parent_class)->dispose (object);
}

gboolean
gst_edt_pdv_sink_start (GstBaseSink * basesink)
{
  GstEdtPdvSink *pdvsink = GST_EDT_PDV_SINK (basesink);

  GST_DEBUG_OBJECT (pdvsink, "Starting");

  pdvsink->dev = pdv_open_channel ("pdv", pdvsink->unit, pdvsink->channel);
  if (pdvsink->dev == NULL) {
    GST_ELEMENT_ERROR (pdvsink, RESOURCE, OPEN_WRITE,
        ("Unable to open unit %d, channel %d", pdvsink->unit, pdvsink->channel),
        (NULL));
    return FALSE;
  }

  if (!pdv_is_simulator (pdvsink->dev)) {
    GST_ELEMENT_ERROR (pdvsink, RESOURCE, OPEN_WRITE,
        ("EDT unit is not a simulator."), (NULL));
    pdv_close (pdvsink->dev);
  }

  /* FIXME: set timeout to wait forever, shouldn't do this of course */
  edt_set_wtimeout (pdvsink->dev, 0);
  edt_set_rtimeout (pdvsink->dev, 0);

  pdv_flush_fifo (pdvsink->dev);

  /* turn off counter data so we can DMA our own image data */
  pdv_cls_set_datacnt (pdvsink->dev, 0);

  pdvsink->cur_buffer = 0;

  return TRUE;
}

gboolean
gst_edt_pdv_sink_stop (GstBaseSink * basesink)
{
  GstEdtPdvSink *pdvsink = GST_EDT_PDV_SINK (basesink);

  if (pdvsink->dev)
    pdv_close (pdvsink->dev);

  return TRUE;
}

GstCaps *
gst_edt_pdv_sink_get_caps (GstBaseSink * basesink, GstCaps * filter_caps)
{
  GstEdtPdvSink *pdvsink = GST_EDT_PDV_SINK (basesink);
  gint depth;
  GstVideoFormat format;
  GstVideoInfo vinfo;

  if (!pdvsink->dev) {
    return gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD
            (pdvsink)));
  }

  gst_video_info_init (&vinfo);
  vinfo.width = pdv_get_width (pdvsink->dev);
  vinfo.height = pdv_get_height (pdvsink->dev);
  depth = pdv_get_depth (pdvsink->dev);

  switch (depth) {
    case 8:
      format = GST_VIDEO_FORMAT_GRAY8;
      break;
    case 16:
      /* TODO: will this be host order or always one of BE/LE? */
      format = GST_VIDEO_FORMAT_GRAY16_BE;
      break;
    default:
      format = GST_VIDEO_FORMAT_UNKNOWN;
  }
  vinfo.finfo = gst_video_format_get_info (format);

  /* TODO: handle filter_caps */
  return gst_video_info_to_caps (&vinfo);
}

gboolean
gst_edt_pdv_sink_set_caps (GstBaseSink * basesink, GstCaps * caps)
{
  GstEdtPdvSink *pdvsink = GST_EDT_PDV_SINK (basesink);
  int buffer_size;
  gint depth;
  int taps;
  GstVideoInfo vinfo;

  GST_DEBUG_OBJECT (pdvsink, "Caps being set");

  gst_video_info_from_caps (&vinfo, caps);

  depth = GST_VIDEO_INFO_COMP_DEPTH (&vinfo, 0);
  buffer_size = vinfo.height * pdv_bytes_per_line (vinfo.width, depth);

  GST_DEBUG_OBJECT (pdvsink,
      "Configuring EDT ring buffer with %d buffers each of size %d",
      pdvsink->n_buffers, buffer_size);

  /* we'll use just two buffers and ping pong between them */
  edt_configure_ring_buffers (pdvsink->dev, buffer_size, pdvsink->n_buffers,
      EDT_WRITE, NULL);

  pdvsink->buffers = edt_buffer_addresses (pdvsink->dev);

  taps = pdvsink->dev->dd_p->cls.taps;

  /* TODO: handle RGB correctly */
  if (depth == 24) {
    taps = 1;
    depth = 32;
  }

  if (taps == 0) {
    GST_WARNING_OBJECT (pdvsink, "Taps set to 0, changing to 1");
    taps = 1;
  }

  GST_DEBUG_OBJECT (pdvsink, "Configuring simulator with %d taps", taps);

  /* configure simulator */
  pdv_cls_set_size (pdvsink->dev, taps, depth, vinfo.width, vinfo.height, PDV_CLS_DEFAULT_HGAP, (vinfo.width / taps) + PDV_CLS_DEFAULT_HGAP,    // taps=1
      PDV_CLS_DEFAULT_VGAP, vinfo.height + PDV_CLS_DEFAULT_VGAP);

  GST_DEBUG ("Configured simulator");

  return TRUE;
}

GstFlowReturn
gst_edt_pdv_sink_render (GstBaseSink * basesink, GstBuffer * buffer)
{
  GstEdtPdvSink *pdvsink = GST_EDT_PDV_SINK (basesink);
  GstMapInfo minfo;

  GST_LOG_OBJECT (pdvsink, "Rendering buffer");

  gst_buffer_map (buffer, &minfo, GST_MAP_WRITE);
  /* TODO: fix stride? */
  memcpy (pdvsink->buffers[pdvsink->cur_buffer], minfo.data, minfo.size);
  gst_buffer_unmap (buffer, &minfo);

  edt_start_buffers (pdvsink->dev, 1);
  pdvsink->cur_buffer = (pdvsink->cur_buffer + 1) % pdvsink->n_buffers;

  return GST_FLOW_OK;
}
