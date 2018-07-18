/* GStreamer
 * Copyright (c) 2018 outside US, United States Government, Joshua M. Doe <oss@nvl.army.mil>
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

#ifndef _GST_KAYA_SRC_H_
#define _GST_KAYA_SRC_H_

#include <gst/base/gstpushsrc.h>

#include <KYFGLib.h>

G_BEGIN_DECLS

#define GST_TYPE_KAYA_SRC   (gst_kayasrc_get_type())
#define GST_KAYA_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_KAYA_SRC,GstKayaSrc))
#define GST_KAYA_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_KAYA_SRC,GstKayaSrcClass))
#define GST_IS_KAYA_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_KAYA_SRC))
#define GST_IS_KAYA_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_KAYA_SRC))


typedef struct _GstKayaSrc GstKayaSrc;
typedef struct _GstKayaSrcClass GstKayaSrcClass;

struct _GstKayaSrc
{
  GstPushSrc base_kayasrc;

  /* handles */
  FGHANDLE fg_handle;
  CAMHANDLE cam_handle;
  STREAM_HANDLE stream_handle;
  STREAM_BUFFER_HANDLE *buffer_handles;
  size_t frame_size;

  /* properties */
  guint interface_index;
  guint device_index;
  guint num_capture_buffers;
  gint timeout;
  gchar *project_file;
  gchar *xml_file;
  gfloat exposure_time;

  gboolean acquisition_started;
  gboolean stop_requested;
  gint64 dropped_frames;

  GstCaps *caps;
  GAsyncQueue *queue;
};

struct _GstKayaSrcClass
{
  GstPushSrcClass base_kayasrc_class;
};

GType gst_kayasrc_get_type (void);

G_END_DECLS

#endif
