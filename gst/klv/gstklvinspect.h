/* GStreamer
 * Copyright (C) 2016 William Manley <will@williammanley.net>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_KLVINSPECT_H_
#define _GST_KLVINSPECT_H_

#include <stdio.h>

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_KLVINSPECT   (gst_klvinspect_get_type())
#define GST_KLVINSPECT(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_KLVINSPECT,GstKlvInspect))
#define GST_KLVINSPECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_KLVINSPECT,GstKlvInspectClass))
#define GST_IS_KLVINSPECT(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_KLVINSPECT))
#define GST_IS_KLVINSPECT_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_KLVINSPECT))

typedef struct _GstKlvInspect GstKlvInspect;
typedef struct _GstKlvInspectClass GstKlvInspectClass;

struct _GstKlvInspect
{
  GstBaseTransform base_klvinspect;

  /* properties */
  gchar* dump_location;

  FILE* dump_file;
};

struct _GstKlvInspectClass
{
  GstBaseTransformClass base_klvinspect_class;
};

GType gst_klvinspect_get_type (void);

G_END_DECLS

#endif /* _GST_KLVINSPECT_H_ */
