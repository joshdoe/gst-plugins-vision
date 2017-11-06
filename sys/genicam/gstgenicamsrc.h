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

#ifndef _GST_GENICAM_SRC_H_
#define _GST_GENICAM_SRC_H_

#include <gst/base/gstpushsrc.h>

#undef __cplusplus
#include "GenTL_v1_5.h"

#define MAX_ERROR_STRING_LEN 256

G_BEGIN_DECLS

#define GST_TYPE_GENICAM_SRC   (gst_genicamsrc_get_type())
#define GST_GENICAM_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GENICAM_SRC,GstGenicamSrc))
#define GST_GENICAM_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GENICAM_SRC,GstGenicamSrcClass))
#define GST_IS_GENICAM_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GENICAM_SRC))
#define GST_IS_GENICAM_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GENICAM_SRC))

typedef struct _GstGenicamSrc GstGenicamSrc;
typedef struct _GstGenicamSrcClass GstGenicamSrcClass;

struct _GstGenicamSrc
{
  GstPushSrc base_genicamsrc;

  /* camera handle */
  TL_HANDLE hTL;
  IF_HANDLE hIF;
  DEV_HANDLE hDEV;
  DS_HANDLE hDS;
  PORT_HANDLE hDevPort;
  EVENT_HANDLE hNewBufferEvent;
  char error_string[MAX_ERROR_STRING_LEN];

  /* properties */
  guint interface_index;
  gchar *interface_id;
  guint device_index;
  gchar *device_id;
  guint stream_index;
  gchar *stream_id;
  guint num_capture_buffers;
  gint timeout;

  GstClockTime acq_start_time;
  guint32 last_frame_count;
  guint32 total_dropped_frames;

  GstCaps *caps;
  gint height;
  gint gst_stride;

  gboolean stop_requested;
};

struct _GstGenicamSrcClass
{
  GstPushSrcClass base_genicamsrc_class;
};

GType gst_genicamsrc_get_type (void);

G_END_DECLS

#endif
