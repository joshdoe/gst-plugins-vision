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
static void gst_edt_pdv_sink_finalize (GObject * object);

/* GstBaseSink prototypes */
static gboolean gst_edt_pdv_sink_start (GstBaseSink * basesink);
static gboolean gst_edt_pdv_sink_stop (GstBaseSink * basesink);
static GstCaps *gst_edt_pdv_sink_get_caps (GstBaseSink * basesink);
static gboolean gst_edt_pdv_sink_set_caps (GstBaseSink * basesink,
    GstCaps * caps);
static GstFlowReturn gst_edt_pdv_sink_render (GstBaseSink * basesink,
    GstBuffer * buffer);

enum
{
  PROP_0
      /* FILL ME */
};


/* pad templates */

static GstStaticPadTemplate gst_edt_pdv_sink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_GRAY8)
    );

/* class initialization */

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (edtpdvsink_debug);
#define GST_CAT_DEFAULT edtpdvsink_debug
#define DEBUG_INIT(bla) \
    GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "edtpdvsink", 0, \
    "EDT PDV Camera Link simulator sink");

GST_BOILERPLATE_FULL (GstEdtPdvSink, gst_edt_pdv_sink, GstBaseSink,
    GST_TYPE_BASE_SINK, DEBUG_INIT);

static void
gst_edt_pdv_sink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_edt_pdv_sink_sink_template));

  gst_element_class_set_details_simple (element_class,
      "EDT PDV Sink", "Sink/Video",
      "EDT PDV Sink for Camera Link simulator boards",
      "Joshua M. Doe <oss@nvl.army.mil>");
}

static void
gst_edt_pdv_sink_class_init (GstEdtPdvSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->set_property = gst_edt_pdv_sink_set_property;
  gobject_class->get_property = gst_edt_pdv_sink_get_property;
  gobject_class->dispose = gst_edt_pdv_sink_dispose;
  gobject_class->finalize = gst_edt_pdv_sink_finalize;

  base_sink_class->start = gst_edt_pdv_sink_start;
  base_sink_class->stop = gst_edt_pdv_sink_stop;
  base_sink_class->set_caps = gst_edt_pdv_sink_set_caps;
  base_sink_class->get_caps = gst_edt_pdv_sink_get_caps;
  base_sink_class->render = gst_edt_pdv_sink_render;
}

static void
gst_edt_pdv_sink_init (GstEdtPdvSink * pdvsink,
    GstEdtPdvSinkClass * pdvsink_class)
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

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

void
gst_edt_pdv_sink_finalize (GObject * object)
{
  GstEdtPdvSink *pdvsink;

  g_return_if_fail (GST_IS_EDT_PDV_SINK (object));
  pdvsink = GST_EDT_PDV_SINK (object);

  /* clean up object here */

  G_OBJECT_CLASS (parent_class)->finalize (object);
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
gst_edt_pdv_sink_get_caps (GstBaseSink * basesink)
{
  GstEdtPdvSink *pdvsink = GST_EDT_PDV_SINK (basesink);
  int width, height, depth;
  GstVideoFormat format;

  if (!pdvsink->dev) {
    return gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD
            (pdvsink)));
  }

  width = pdv_get_width (pdvsink->dev);
  height = pdv_get_height (pdvsink->dev);
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

  return gst_video_format_new_caps (format, width, height, 30, 1, 1, 1);
}

gboolean
gst_edt_pdv_sink_set_caps (GstBaseSink * basesink, GstCaps * caps)
{
  GstEdtPdvSink *pdvsink = GST_EDT_PDV_SINK (basesink);
  GstVideoFormat format;
  int width, height, depth;
  int buffer_size;
  int depth_bytes;
  int taps;

  GST_DEBUG_OBJECT (pdvsink, "Caps being set");

  gst_video_format_parse_caps (caps, &format, &width, &height);

  switch (format) {
    case GST_VIDEO_FORMAT_GRAY8:
      depth = 8;
      depth_bytes = 1;
      break;
    case GST_VIDEO_FORMAT_GRAY16_BE:
    case GST_VIDEO_FORMAT_GRAY16_LE:
      depth = 16;
      depth_bytes = 2;
      break;
    default:
      GST_ERROR_OBJECT (pdvsink, "Unsupported video format");
      return FALSE;
  }

  buffer_size = height * pdv_bytes_per_line (width, depth);

  GST_DEBUG_OBJECT (pdvsink,
      "Configuring EDT ring buffer with %d buffers each of size %d",
      pdvsink->n_buffers, buffer_size);

  /* we'll use just two buffers and ping pong between them */
  edt_configure_ring_buffers (pdvsink->dev, buffer_size, pdvsink->n_buffers,
      EDT_WRITE, NULL);

  pdvsink->buffers = edt_buffer_addresses (pdvsink->dev);

  taps = pdvsink->dev->dd_p->cls.taps;

  /* TODO: handle RGB correctly */
  if (depth_bytes == 3) {
    taps = 1;
    depth_bytes = 4;
  }

  if (taps == 0) {
    GST_WARNING_OBJECT (pdvsink, "Taps set to 0, changing to 1");
    taps = 1;
  }

  GST_DEBUG_OBJECT (pdvsink, "Configuring simulator with %d taps", taps);

  /* configure simulator */
  pdv_cls_set_size (pdvsink->dev, taps, depth, width, height, PDV_CLS_DEFAULT_HGAP, (width / taps) + PDV_CLS_DEFAULT_HGAP,      // taps=1
      PDV_CLS_DEFAULT_VGAP, height + PDV_CLS_DEFAULT_VGAP);

  GST_DEBUG ("Configured simulator");

  return TRUE;
}

GstFlowReturn
gst_edt_pdv_sink_render (GstBaseSink * basesink, GstBuffer * buffer)
{
  GstEdtPdvSink *pdvsink = GST_EDT_PDV_SINK (basesink);

  GST_LOG_OBJECT (pdvsink, "Rendering buffer");

  memcpy (pdvsink->buffers[pdvsink->cur_buffer], GST_BUFFER_DATA (buffer),
      GST_BUFFER_SIZE (buffer));
  edt_start_buffers (pdvsink->dev, 1);
  pdvsink->cur_buffer = (pdvsink->cur_buffer + 1) % pdvsink->n_buffers;

  return GST_FLOW_OK;
}
