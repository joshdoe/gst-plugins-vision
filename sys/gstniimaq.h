/* GStreamer
 * Copyright (C) <2006> Eric Jonas <jonas@mit.edu>
 * Copyright (C) <2006> Antoine Tremblay <hexa00@gmail.com>
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

#ifndef __GST_NIIMAQ_H__
#define __GST_NIIMAQ_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <niimaq.h>

G_BEGIN_DECLS

#define GST_TYPE_NIIMAQ \
  (gst_niimaq_get_type())
#define GST_NIIMAQ(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NIIMAQ,GstNiImaq))
#define GST_NIIMAQ_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_NIIMAQ,GstNiImaq))
#define GST_IS_NIIMAQ(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NIIMAQ))
#define GST_IS_NIIMAQ_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_NIIMAQ))

typedef struct _GstNiImaq GstNiImaq;
typedef struct _GstNiImaqClass GstNiImaqClass;

struct _GstNiImaq {
  GstPushSrc element;

  /* video state */
  gint width;
  gint height;
  gint depth; 
  gint bpp;
  gint framesize;

  gint rate_numerator;
  gint rate_denominator;

  /* private */
  gint64 timestamp_offset;		/* base offset */
  GstClockTime running_time;		/* total running time */
  gint64 n_frames;			/* total frames sent */
  uInt32 cumbufnum;
  gint64 n_dropped_frames;
  gboolean segment;
  gint bufsize; 
  guint32** buflist;

  gchar *device_name;
  INTERFACE_ID iid;
  SESSION_ID sid;

  GstCaps *caps;
};

struct _GstNiImaqClass {
  GstPushSrcClass parent_class;
};

GType gst_niimaq_get_type (void);

G_END_DECLS

#endif /* __GST_NIIMAQ_H__ */
