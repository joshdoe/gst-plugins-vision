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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _GST_GENTL_SRC_H_
#define _GST_GENTL_SRC_H_

#include <gst/base/gstpushsrc.h>

#undef __cplusplus
#include "GenTL_v1_5.h"

#define MAX_ERROR_STRING_LEN 256

G_BEGIN_DECLS

#define GST_TYPE_GENTL_SRC   (gst_gentlsrc_get_type())
#define GST_GENTL_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GENTL_SRC,GstGenTlSrc))
#define GST_GENTL_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GENTL_SRC,GstGenTlSrcClass))
#define GST_IS_GENTL_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GENTL_SRC))
#define GST_IS_GENTL_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GENTL_SRC))
#define GST_GENTL_SRC_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS ((klass), GST_TYPE_GENTL_SRC, GstGenTlSrcClass))

typedef struct _GstGenTlSrc GstGenTlSrc;
typedef struct _GstGenTlSrcClass GstGenTlSrcClass;

typedef struct _GstGenTlProducer GstGenTlProducer;
struct _GstGenTlProducer
{
  gchar* cti_path;
  guint32 acquisition_mode_value;

  guint64 width;
  guint64 height;
  guint64 pixel_format;
  guint64 payload_size;
  guint64 acquisition_mode;
  guint64 acquisition_start;
  guint64 acquisition_stop;
  guint64 tick_frequency_low;
  guint64 tick_frequency_high;
  guint64 timestamp_control_latch;
  guint64 timestamp_low;
  guint64 timestamp_high;
};


/**
* GstGenTlSrcProducer:
* @GST_GENTLSRC_PRODUCER_BASLER: Basler producer
* @GST_GENTLSRC_PRODUCER_EVT: EVT producer
*
* Producer to use.
*/
typedef enum {
  GST_GENTLSRC_PRODUCER_BASLER,
  GST_GENTLSRC_PRODUCER_EVT,
} GstGenTlSrcProducer;


struct _GstGenTlSrc
{
  GstPushSrc base_gentlsrc;

  /* camera handle */
  TL_HANDLE hTL;
  IF_HANDLE hIF;
  DEV_HANDLE hDEV;
  DS_HANDLE hDS;
  PORT_HANDLE hDevPort;
  EVENT_HANDLE hNewBufferEvent;
  char error_string[MAX_ERROR_STRING_LEN];
  GstGenTlProducer producer;

  /* properties */
  GstGenTlSrcProducer producer_prop;
  guint interface_index;
  gchar *interface_id;
  guint device_index;
  gchar *device_id;
  guint stream_index;
  gchar *stream_id;
  guint num_capture_buffers;
  gint timeout;
  gchar* attributes;

  GstClockTime acq_start_time;
  guint32 last_frame_count;
  guint32 total_dropped_frames;

  guint64 tick_frequency;
  guint64 unix_latched_time;
  guint64 gentl_latched_ticks;

  GstCaps *caps;
  gint height;
  gint gst_stride;

  gboolean stop_requested;
};

struct _GstGenTlSrcClass
{
  GstPushSrcClass base_gentlsrc_class;
  TL_HANDLE hTL;
  GMutex tl_mutex;
  guint tl_refcount;
};

GType gst_gentlsrc_get_type (void);

G_END_DECLS

#endif
