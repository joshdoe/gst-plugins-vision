/* GStreamer
 * Copyright (C) <2006> Eric Jonas <jonas@mit.edu>
 * Copyright (C) <2006> Antoine Tremblay <hexa00@gmail.com>
 * Copyright (C) 2010 United States Government, Joshua M. Doe <oss@nvl.army.mil>
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

#ifndef __GST_NIIMAQSRC_H__
#define __GST_NIIMAQSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <niimaq.h>

G_BEGIN_DECLS

#define GST_TYPE_NIIMAQSRC \
  (gst_niimaqsrc_get_type())
#define GST_NIIMAQSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NIIMAQSRC,GstNiImaqSrc))
#define GST_NIIMAQSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_NIIMAQSRC,GstNiImaqSrcClass))
#define GST_IS_NIIMAQSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NIIMAQSRC))
#define GST_IS_NIIMAQSRC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_NIIMAQSRC))
#define GST_NIIMAQSRC_GET_CLASS(klass) \
  (G_TYPE_INSTANCE_GET_CLASS ((klass), GST_TYPE_NIIMAQSRC, GstNiImaqSrcClass))

typedef struct _GstNiImaqSrc GstNiImaqSrc;
typedef struct _GstNiImaqSrcClass GstNiImaqSrcClass;

struct _GstNiImaqSrc {
  GstPushSrc element;

  /* video state */
  gint width;
  gint height;
  gint depth; 
  gint bpp;
  gint framesize;

  /* private */
  gint64 timestamp_offset;    /* base offset */
  GstClockTime running_time;    /* total running time */
  gint64 n_frames;      /* total frames sent */
  uInt32 cumbufnum;
  gint64 n_dropped_frames;
  gboolean segment;
  gint bufsize; 
  guint32** buflist;

  gchar *camera_name;
  gchar *interface_name;
  INTERFACE_ID iid;
  SESSION_ID sid;

  gboolean session_started;

  GstCaps *caps;

  GSList *timelist;
  GMutex *frametime_mutex;

  GstDateTime *start_time;
  gboolean start_time_sent;
};

struct _GstNiImaqSrcClass {
  GstPushSrcClass parent_class;

  /* probed interfaces */
  GList *interfaces;
};

GType gst_niimaqsrc_get_type (void);

G_END_DECLS

#endif /* __GST_NIIMAQSRC_H__ */
