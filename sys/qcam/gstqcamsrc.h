/* GStreamer
 * Copyright (C) 2021 FIXME <fixme@example.com>
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

#ifndef _GST_QCAM_SRC_H_
#define _GST_QCAM_SRC_H_

#include <gst/base/gstpushsrc.h>

#include <QCamApi.h>

G_BEGIN_DECLS

#define GST_TYPE_QCAM_SRC   (gst_qcamsrc_get_type())
#define GST_QCAM_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_QCAM_SRC,GstQcamSrc))
#define GST_QCAM_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_QCAM_SRC,GstQcamSrcClass))
#define GST_IS_QCAM_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_QCAM_SRC))
#define GST_IS_QCAM_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_QCAM_SRC))

typedef struct _GstQcamSrc GstQcamSrc;
typedef struct _GstQcamSrcClass GstQcamSrcClass;

struct _GstQcamSrc
{
  GstPushSrc base_qcamsrc;

  /* camera handle */
  QCam_Handle handle;
  gboolean send_settings;
  QCam_Settings qsettings;

  /* properties */
  gint device_index;
  guint num_capture_buffers;
  gint timeout;
  guint exposure;
  gdouble gain;
  gint offset;
  gint format;
  gint x;
  gint y;
  gint width;
  gint height;
  gint binning;

  GAsyncQueue *queue;
  GstClockTime base_time;

  guint32 last_frame_count;
  guint32 total_dropped_frames;

  GstCaps *caps;

  gboolean stop_requested;
};

struct _GstQcamSrcClass
{
  GstPushSrcClass base_qcamsrc_class;
};

GType gst_qcamsrc_get_type (void);

G_END_DECLS

#endif
