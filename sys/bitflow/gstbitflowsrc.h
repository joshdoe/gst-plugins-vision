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

#ifndef _GST_BITFLOW_SRC_H_
#define _GST_BITFLOW_SRC_H_

#include <gst/base/gstpushsrc.h>

#include "BiApi.h"

G_BEGIN_DECLS

#define GST_TYPE_BITFLOW_SRC   (gst_bitflowsrc_get_type())
#define GST_BITFLOW_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BITFLOW_SRC,GstBitflowSrc))
#define GST_BITFLOW_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BITFLOW_SRC,GstBitflowSrcClass))
#define GST_IS_BITFLOW_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BITFLOW_SRC))
#define GST_IS_BITFLOW_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BITFLOW_SRC))

typedef struct _GstBitflowSrc GstBitflowSrc;
typedef struct _GstBitflowSrcClass GstBitflowSrcClass;

struct _GstBitflowSrc
{
  GstPushSrc base_bitflowsrc;

  /* camera handle */
  Bd board;
  BIBA buffer_array;
  BFCHAR error_string[MAX_STRING];

  /* properties */
  gchar *camera_file;
  guint num_capture_buffers;
  guint board_index;
  gint timeout;

  GstClockTime acq_start_time;
  guint32 last_frame_count;
  guint32 total_dropped_frames;

  GstCaps *caps;
  gint height;
  gint gst_stride;
  gint bf_stride;

  gboolean stop_requested;
};

struct _GstBitflowSrcClass
{
  GstPushSrcClass base_bitflowsrc_class;
};

GType gst_bitflowsrc_get_type (void);

G_END_DECLS

#endif
