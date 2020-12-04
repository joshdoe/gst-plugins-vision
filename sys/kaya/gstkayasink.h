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

#ifndef _GST_KAYA_SINK_H_
#define _GST_KAYA_SINK_H_

#include <gst/base/gstbasesink.h>
#include <gst/video/video.h>

#include <KYFGLib.h>

#define KAYA_SINK_MAX_FG_HANDLES 16
#define KAYA_SINK_MAX_CAM_HANDLES 4

G_BEGIN_DECLS

#define GST_TYPE_KAYASINK   (gst_kayasink_get_type())
#define GST_KAYASINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_KAYASINK,GstKayaSink))
#define GST_KAYASINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_KAYASINK,GstKayaSinkClass))
#define GST_IS_KAYASINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_KAYASINK))
#define GST_IS_KAYASINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_KAYASINK))
#define GST_KAYA_SINK_GET_CLASS(klass) \
    (G_TYPE_INSTANCE_GET_CLASS ((klass), GST_TYPE_KAYA_SINK, GstKayaSinkClass))

typedef struct _GstKayaSink GstKayaSink;
typedef struct _GstKayaSinkClass GstKayaSinkClass;

typedef struct _GstKayaSinkFramegrabber GstKayaSinkFramegrabber;

struct _GstKayaSink
{
  GstBaseSink base;

  GstKayaSinkFramegrabber* fg_data;
  CAMHANDLE cam_handle;
  STREAM_HANDLE stream_handle;
  STREAM_BUFFER_HANDLE* buffer_handles;

  /* properties */
  guint interface_index;
  guint device_index;
  guint num_render_buffers;
  gint timeout;
  gchar* project_file;
  gchar* xml_file;
  gboolean wait_for_receiver;
  gint wait_timeout;

  gboolean receiver_connected;
  GstVideoInfo vinfo;

  GAsyncQueue* queue;
  
  GMutex mutex;
  GCond cond;
  gboolean acquisition_started;
  gboolean stop_requested;
};

struct _GstKayaSinkFramegrabber
{
    FGHANDLE fg_handle;
    guint32 ref_count;
    CAMHANDLE cam_handles[KAYA_SINK_MAX_CAM_HANDLES];
    int num_cams;
    GMutex fg_mutex;
};

struct _GstKayaSinkClass
{
  GstBaseSinkClass base_class;

  GstKayaSinkFramegrabber fg_data[KAYA_SINK_MAX_FG_HANDLES];
};

GType gst_kayasink_get_type (void);

G_END_DECLS

#endif /* _GST_KAYA_SINK_H_ */