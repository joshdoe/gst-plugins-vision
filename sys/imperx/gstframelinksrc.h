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

#ifndef _GST_FRAMELINK_SRC_H_
#define _GST_FRAMELINK_SRC_H_

#include <gst/base/gstpushsrc.h>

#include <Windows.h>
#include <VCECLB.h>

G_BEGIN_DECLS

#define GST_TYPE_FRAMELINK_SRC   (gst_framelinksrc_get_type())
#define GST_FRAMELINK_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FRAMELINK_SRC,GstFramelinkSrc))
#define GST_FRAMELINK_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FRAMELINK_SRC,GstFramelinkSrcClass))
#define GST_IS_FRAMELINK_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FRAMELINK_SRC))
#define GST_IS_FRAMELINK_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FRAMELINK_SRC))

typedef struct _GstFramelinkSrc GstFramelinkSrc;
typedef struct _GstFramelinkSrcClass GstFramelinkSrcClass;

struct _GstFramelinkSrc
{
  GstPushSrc base_framelinksrc;

  guint last_buffer_number;
  gint dropped_frame_count;
  gboolean acq_started;

  /* camera handle */
  HANDLE grabber;

  /* properties */
  gchar *format_file;
  guint num_capture_buffers;
  guint board;
  guint channel;

  GstBuffer *buffer;

  GstCaps *caps;
  gint height;
  gint widthBytesPreValid;
  gint widthBytes;
  gint heightPreValid;
  gint gst_stride;
  guint flex_stride;

  GMutex mutex;
  GCond cond;
};

struct _GstFramelinkSrcClass
{
  GstPushSrcClass base_framelinksrc_class;
};

GType gst_framelinksrc_get_type (void);

G_END_DECLS

#endif
