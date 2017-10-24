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

#ifndef _GST_APTINA_SRC_H_
#define _GST_APTINA_SRC_H_

#include <gst/base/gstpushsrc.h>

#include "apbase.h"

G_BEGIN_DECLS

#define GST_TYPE_APTINA_SRC   (gst_aptinasrc_get_type())
#define GST_APTINA_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_APTINA_SRC,GstAptinaSrc))
#define GST_APTINA_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_APTINA_SRC,GstAptinaSrcClass))
#define GST_IS_APTINA_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_APTINA_SRC))
#define GST_IS_APTINA_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_APTINA_SRC))

typedef struct _GstAptinaSrc GstAptinaSrc;
typedef struct _GstAptinaSrcClass GstAptinaSrcClass;

struct _GstAptinaSrc
{
  GstPushSrc base_aptinasrc;

  AP_HANDLE apbase;
  gboolean is_started;

  /* properties */
  gint camera_index;
  gchar *config_file;
  gchar *config_preset;

  GstClockTime acq_start_time;
  guint32 last_frame_count;
  guint32 total_dropped_frames;

  GstCaps *caps;
  gint framesize;
  guint8 *buffer;

  gboolean stop_requested;
};

struct _GstAptinaSrcClass
{
  GstPushSrcClass base_aptinasrc_class;
};

GType gst_aptinasrc_get_type (void);

G_END_DECLS

#endif
