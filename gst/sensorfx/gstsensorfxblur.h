/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2003 Arwed v. Merkatz <v.merkatz@gmx.net>
 * Copyright (C) 2006 Mark Nauwelaerts <manauw@skynet.be>
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


#ifndef __GST_SENSORFXBLUR_H__
#define __GST_SENSORFXBLUR_H__

#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_SENSORFXBLUR \
  (gst_sfxblur_get_type())
#define GST_SENSORFXBLUR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SENSORFXBLUR,GstSensorFxBlur))
#define GST_SENSORFXBLUR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SENSORFXBLUR,GstSensorFxBlurClass))
#define GST_IS_SENSORFXBLUR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SENSORFXBLUR))
#define GST_IS_SENSORFXBLUR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SENSORFXBLUR))

typedef struct _GstSensorFxBlur GstSensorFxBlur;
typedef struct _GstSensorFxBlurClass GstSensorFxBlurClass;

/**
* GstSensorFxBlur:
* @element: the parent element.
*
*
* The opaque GstSensorFxBlur data structure.
*/
struct _GstSensorFxBlur
{
  GstVideoFilter element;

  /* format */
  gint width;
  gint height;
  guint stride;
  gint bpp;
  gint depth;
  gint endianness;

  /* properties */

};

struct _GstSensorFxBlurClass
{
  GstVideoFilterClass parent_class;
};

GType gst_sfxblur_get_type(void);

gboolean gst_sfxblur_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_SENSORFXBLUR_H__ */
