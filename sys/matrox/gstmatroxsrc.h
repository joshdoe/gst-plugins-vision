/* GStreamer
 * Copyright (C) 2017 FIXME <fixme@example.com>
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

#ifndef _GST_MATROX_SRC_H_
#define _GST_MATROX_SRC_H_

#include <gst/base/gstpushsrc.h>

#include <mil.h>

G_BEGIN_DECLS

#define GST_TYPE_MATROX_SRC   (gst_matroxsrc_get_type())
#define GST_MATROX_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MATROX_SRC,GstMatroxSrc))
#define GST_MATROX_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MATROX_SRC,GstMatroxSrcClass))
#define GST_IS_MATROX_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MATROX_SRC))
#define GST_IS_MATROX_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MATROX_SRC))

typedef struct _GstMatroxSrc GstMatroxSrc;
typedef struct _GstMatroxSrcClass GstMatroxSrcClass;

struct _GstMatroxSrc
{
  GstPushSrc base_matroxsrc;

  gboolean acq_started;

  /* MIL handles */
  MIL_ID MilApplication;
  MIL_ID MilSystem;
  MIL_ID MilDigitizer;
  MIL_ID *MilGrabBufferList;

  /* properties */
  gchar *device;
  gint board;
  gint channel;
  gchar *format;
  guint num_capture_buffers;
  gint timeout;

  GstBuffer *buffer;
  GstClockTime acq_start_time;

  GstCaps *caps;
  gint width;
  gint height;
  gint bpp;
  gint gst_stride;
  MIL_INT color_mode;
  GstVideoFormat video_format;

  GMutex mutex;
  GCond cond;
  gboolean stop_requested;
};

struct _GstMatroxSrcClass
{
  GstPushSrcClass base_matroxsrc_class;
};

GType gst_matroxsrc_get_type (void);

G_END_DECLS

#endif
