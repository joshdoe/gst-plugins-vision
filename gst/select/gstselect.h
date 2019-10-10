/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2003 Arwed v. Merkatz <v.merkatz@gmx.net>
 * Copyright (C) 2006 Mark Nauwelaerts <manauw@skynet.be>
 * Copyright (C) 2015 United States Government, Joshua M. Doe <oss@nvl.army.mil>
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


#ifndef __GST_SELECT_H__
#define __GST_SELECT_H__

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_SELECT \
  (gst_select_get_type())
#define GST_SELECT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SELECT,GstSelect))
#define GST_SELECT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SELECT,GstSelectClass))
#define GST_IS_SELECT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SELECT))
#define GST_IS_SELECT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SELECT))

typedef struct _GstSelect GstSelect;
typedef struct _GstSelectClass GstSelectClass;

/**
* GstSelect:
* @element: the parent element.
*
*
* The opaque GstSelect data structure.
*/
struct _GstSelect
{
  GstBaseTransform element;

  /* properties */
  gint offset;
  gint skip;
};

struct _GstSelectClass
{
  GstBaseTransformClass parent_class;
};

GType gst_select_get_type(void);

G_END_DECLS

#endif /* __GST_SELECT_H__ */
