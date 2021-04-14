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
 * SECTION:element-gstklvtimestamp
 *
 * The klvtimestamp element parses KLV to place timestamps on passing buffers.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v fakesrc ! klvtimestamp ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/base/gstbytewriter.h>
#include "gstklvtimestamp.h"
#include "klv.h"

GST_DEBUG_CATEGORY_STATIC (gst_klvtimestamp_debug_category);
#define GST_CAT_DEFAULT gst_klvtimestamp_debug_category

/* prototypes */
static GstFlowReturn gst_klvtimestamp_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);

enum
{
  PROP_0
};

/* pad templates */

#define SRC_CAPS "ANY"
#define SINK_CAPS "ANY"


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstKlvTimestamp, gst_klvtimestamp,
    GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_klvtimestamp_debug_category, "klvtimestamp", 0,
        "debug category for klvtimestamp element"));

static void
gst_klvtimestamp_class_init (GstKlvTimestampClass * klass)
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
      "KLV Timestamp", "Filter", "KLV Timestamp Conversion",
      "Joshua M. Doe <oss@nvl.army.mil>");

  base_transform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_klvtimestamp_transform_ip);

  base_transform_class->passthrough_on_same_caps = TRUE;
  base_transform_class->transform_ip_on_passthrough = TRUE;
}

static void
gst_klvtimestamp_init (GstKlvTimestamp * filt)
{
}

static void
gst_klvtimestamp_dispose (GstKlvTimestamp * filt)
{
}

static GstStaticCaps unix_reference = GST_STATIC_CAPS ("timestamp/x-unix");

static void
gst_klvtimestamp_parse_klv_timestamp (GstKlvTimestamp * filt, GstBuffer * buf)
{
  /* Add KLV meta for testing, here: Motion Imagery Standards Board (MISB)
   * Engineering Guideline MISB EG 0902 - MISB Minimum Metadata Set.
   * Also see: SMPTE S336M for KLV specification, also ITU-R BT.1563-1 */
  const guint8 klv_header[16] = { 0x06, 0x0e, 0x2b, 0x34, 0x02, 0x0b, 0x01,
    0x01, 0x0e, 0x01, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00
  };

  GstKLVMeta *klv_meta;
  gsize klv_size;
  const guint8 *klv_data;
  GstByteReader br;
  guint8 *header_data;

  /* FIXME: MISB defines MISP time, which is NOT UTC, but use UTC for now */
  guint64 utc_us = -1;


  klv_meta = gst_buffer_get_klv_meta (buf);
  if (!klv_meta) {
    GST_DEBUG_OBJECT (filt, "Buffer has no KLV metadata");
    return;
  }

  klv_data = gst_klv_meta_get_data (klv_meta, &klv_size);

  if (!klv_data || klv_size == 0) {
    GST_WARNING_OBJECT (filt, "KLV metadata appears to be empty");
    return;
  }

  gst_byte_reader_init (&br, klv_data, (guint) klv_size);

  if (!gst_byte_reader_get_data (&br, sizeof (klv_header), &header_data)) {
    GST_WARNING_OBJECT (filt, "KLV data too small to contain header");
    return;
  }

  if (memcmp (header_data, klv_header, sizeof (klv_header)) != 0) {
    GST_WARNING_OBJECT (filt, "KLV header doesn't match");
    GST_MEMDUMP_OBJECT (filt, "KLV header found", header_data,
        (guint) sizeof (klv_header));
    return;
  }

  guint8 klv_len;
  if (!gst_byte_reader_get_uint8 (&br, &klv_len))
    goto not_enough_data;

  guint8 tag;
  if (!gst_byte_reader_get_uint8 (&br, &tag))
    goto not_enough_data;
  if (tag != 2) {
    GST_WARNING_OBJECT (filt,
        "First tag is %d, not 2 as expected for timestamp", tag);
    return;
  }

  guint8 data_len;
  if (!gst_byte_reader_get_uint8 (&br, &data_len))
    goto not_enough_data;
  if (data_len != 8) {
    GST_WARNING_OBJECT (filt,
        "Data length for time tag is %d, not 8 as expected", data_len);
  }

  if (!gst_byte_reader_get_uint64_be (&br, &utc_us))
    goto not_enough_data;

  GST_LOG_OBJECT (filt, "Found timestamp of %d.%06d s", utc_us / 1000000,
      utc_us % 1000000);

  GstReferenceTimestampMeta *time_meta;
  time_meta =
      gst_buffer_add_reference_timestamp_meta (buf,
      gst_static_caps_get (&unix_reference), utc_us * 1000,
      GST_CLOCK_TIME_NONE);
  if (!time_meta) {
    GST_WARNING_OBJECT (filt, "Failed to generate or attach timestamp meta");
  }

  return;

not_enough_data:
  GST_WARNING_OBJECT (filt, "Not enough data to continue parsing KLV");
  return;
}

static GstFlowReturn
gst_klvtimestamp_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstKlvTimestamp *filt = GST_KLVTIMESTAMP (trans);

  GST_LOG_OBJECT (filt, "Parsing KLV metadata for timestamp");
  gst_klvtimestamp_parse_klv_timestamp (filt, buf);

  return GST_FLOW_OK;
}
