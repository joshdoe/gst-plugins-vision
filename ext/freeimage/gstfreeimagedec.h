/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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


#ifndef __GST_FREEIMAGEDEC_H__
#define __GST_FREEIMAGEDEC_H__

#include <gst/gst.h>
#include <FreeImage.h>

G_BEGIN_DECLS

#define GST_TYPE_FREEIMAGEDEC (gst_freeimagedec_get_type())
#define GST_FREEIMAGEDEC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FREEIMAGEDEC,GstFreeImageDec))
#define GST_FREEIMAGEDEC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FREEIMAGEDEC,GstFreeImageDecClass))
#define GST_IS_FREEIMAGEDEC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FREEIMAGEDEC))
#define GST_IS_FREEIMAGEDEC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FREEIMAGEDEC))

typedef struct _GstFreeImageDec GstFreeImageDec;
typedef struct _GstFreeImageDecClass GstFreeImageDecClass;

struct _GstFreeImageDec
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gboolean need_newsegment;
  
  /* Pull range */
  long offset;


  guint64 in_timestamp;
  guint64 in_duration;

  gboolean framed;

  gint ret;

  FIBITMAP *dib;

  gboolean setup;

  gint width;
  gint height;
  gint bpp;
  gint fps_n;
  gint fps_d;

  GstSegment segment;
  gboolean image_ready;

  FreeImageIO fiio;
  guint64 length;
};

struct _GstFreeImageDecClass
{
  GstElementClass parent_class;
};

GType gst_freeimagedec_get_type(void);

G_END_DECLS

#endif /* __GST_FREEIMAGEDEC_H__ */
