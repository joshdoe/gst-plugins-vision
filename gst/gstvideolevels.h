/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2003 Arwed v. Merkatz <v.merkatz@gmx.net>
 * Copyright (C) 2006 Mark Nauwelaerts <manauw@skynet.be>
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


#ifndef __GST_VIDEO_LEVELS_H__
#define __GST_VIDEO_LEVELS_H__

#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_VIDEOLEVELS \
  (gst_videolevels_get_type())
#define GST_VIDEOLEVELS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIDEOLEVELS,GstVideoLevels))
#define GST_VIDEOLEVELS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIDEOLEVELS,GstVideoLevelsClass))
#define GST_IS_VIDEOLEVELS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIDEOLEVELS))
#define GST_IS_VIDEOLEVELS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIDEOLEVELS))

typedef struct _GstVideoLevels GstVideoLevels;
typedef struct _GstVideoLevelsClass GstVideoLevelsClass;

/**
 * GstVideoLevels:
 *
 * Opaque data structure.
 */
struct _GstVideoLevels
{
  GstVideoFilter videofilter;

  /* format */
  gint width;
  gint height;
  gint bpp;
  gint depth;
  gint size;

  /* properties */
  guint16 lower_input;
  guint16 upper_input;
  guint8 lower_output;
  guint8 upper_output;

  /* tables */
  guint8* levels_table;
};

struct _GstVideoLevelsClass
{
  GstVideoFilterClass parent_class;
};

GType gst_videolevels_get_type(void);

G_END_DECLS

#endif /* __GST_VIDEO_LEVELS_H__ */
