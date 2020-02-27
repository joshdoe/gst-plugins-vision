/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2003 Arwed v. Merkatz <v.merkatz@gmx.net>
 * Copyright (C) 2006 Mark Nauwelaerts <manauw@skynet.be>
 * Copyright (C) 2020 United States Government, Joshua M. Doe <oss@nvl.army.mil>
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


#ifndef __GST_BAYER2GRAY_H__
#define __GST_BAYER2GRAY_H__

#include <gst/video/gstvideofilter.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_BAYER2GRAY \
  (gst_bayer2gray_get_type())
#define GST_BAYER2GRAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BAYER2GRAY,GstBayer2Gray))
#define GST_BAYER2GRAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BAYER2GRAY,GstBayer2GrayClass))
#define GST_IS_BAYER2GRAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BAYER2GRAY))
#define GST_IS_BAYER2GRAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BAYER2GRAY))

typedef struct _GstBayer2Gray GstBayer2Gray;
typedef struct _GstBayer2GrayClass GstBayer2GrayClass;


/**
* GstBayer2Gray:
* @element: the parent element.
*
*
* The opaque GstBayer2Gray data structure.
*/
struct _GstBayer2Gray
{
  GstBaseTransform element;

  /* format */
  GstVideoInfo vinfo;
  gint width;
  gint height;
  gint depth;
  gint bpp;
  gint fps_n;
  gint fps_d;
};

struct _GstBayer2GrayClass
{
  GstBaseTransformClass parent_class;
};

GType gst_bayer2gray_get_type(void);

G_END_DECLS

#endif /* __GST_BAYER2GRAY_H__ */
