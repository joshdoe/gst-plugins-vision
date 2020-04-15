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

#ifndef _GST_IDSUEYE_SRC_H_
#define _GST_IDSUEYE_SRC_H_

#include <gst/base/gstpushsrc.h>

#define _PURE_C
#include "ueye.h"

G_BEGIN_DECLS

#define GST_TYPE_IDSUEYE_SRC   (gst_idsueyesrc_get_type())
#define GST_IDSUEYE_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IDSUEYE_SRC,GstIdsueyeSrc))
#define GST_IDSUEYE_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IDSUEYE_SRC,GstIdsueyeSrcClass))
#define GST_IS_IDSUEYE_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IDSUEYE_SRC))
#define GST_IS_IDSUEYE_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IDSUEYE_SRC))

typedef struct _GstIdsueyeSrc GstIdsueyeSrc;
typedef struct _GstIdsueyeSrcClass GstIdsueyeSrcClass;

#define MAX_SEQ_BUFFERS 4096

struct _GstIdsueyeSrc
{
  GstPushSrc base_idsueyesrc;

  /* camera handle */
  HIDS hCam;
  gboolean is_started;

  char* seqImgMem[MAX_SEQ_BUFFERS];
  int seqMemId[MAX_SEQ_BUFFERS];

  /* properties */
  gint camera_id;
  gchar *config_file;
  gint num_capture_buffers;
  gint timeout;

  GstClockTime acq_start_time;
  guint32 last_frame_count;
  guint32 total_dropped_frames;

  GstCaps *caps;
  gint width;
  gint height;
  gint bitsPerPixel;

  gboolean stop_requested;
};

struct _GstIdsueyeSrcClass
{
  GstPushSrcClass base_idsueyesrc_class;
};

GType gst_idsueyesrc_get_type (void);

G_END_DECLS

#endif
