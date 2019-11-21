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

#ifndef _GST_PLEORA_SRC_H_
#define _GST_PLEORA_SRC_H_

#include <gst/base/gstpushsrc.h>

#include <PvDevice.h>
#include <PvPipeline.h>
#include <PvStream.h>

G_BEGIN_DECLS

#define GST_TYPE_PLEORA_SRC   (gst_pleorasrc_get_type())
#define GST_PLEORA_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLEORA_SRC,GstPleoraSrc))
#define GST_PLEORA_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PLEORA_SRC,GstPleoraSrcClass))
#define GST_IS_PLEORA_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLEORA_SRC))
#define GST_IS_PLEORA_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PLEORA_SRC))

typedef struct _GstPleoraSrc GstPleoraSrc;
typedef struct _GstPleoraSrcClass GstPleoraSrcClass;

struct _GstPleoraSrc
{
  GstPushSrc base_pleorasrc;

  /* camera handle */
  PvPipeline *pipeline;
  PvDevice *device;
  PvStream *stream;
  PvBuffer *pvbuffer;
  PvDeviceType device_type;

  /* properties */
  gchar *device_id;
  gint device_index;
  guint num_capture_buffers;
  gint timeout;
  gint detection_timeout;
  gchar *multicast_group;
  gint port;
  gboolean receiver_only;
  gint packet_size;
  gchar *config_file;
  gboolean config_file_connect;
  gboolean output_klv;

  guint32 last_frame_count;
  guint32 total_dropped_frames;

  GstCaps *caps;
  PvPixelType pv_pixel_type;
  gint width;
  gint height;
  gint gst_stride;
  gint pleora_stride;

  gboolean stop_requested;
};

struct _GstPleoraSrcClass
{
  GstPushSrcClass base_pleorasrc_class;
};

GType gst_pleorasrc_get_type (void);

G_END_DECLS

#endif
