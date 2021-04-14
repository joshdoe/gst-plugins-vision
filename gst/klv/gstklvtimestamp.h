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

#ifndef _GST_KLVTIMESTAMP_H_
#define _GST_KLVTIMESTAMP_H_

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_KLVTIMESTAMP   (gst_klvtimestamp_get_type())
#define GST_KLVTIMESTAMP(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_KLVTIMESTAMP,GstKlvTimestamp))
#define GST_KLVTIMESTAMP_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_KLVTIMESTAMP,GstKlvTimestampClass))
#define GST_IS_KLVTIMESTAMP(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_KLVTIMESTAMP))
#define GST_IS_KLVTIMESTAMP_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_KLVTIMESTAMP))

typedef struct _GstKlvTimestamp GstKlvTimestamp;
typedef struct _GstKlvTimestampClass GstKlvTimestampClass;

struct _GstKlvTimestamp
{
  GstBaseTransform base_klvtimestamp;
};

struct _GstKlvTimestampClass
{
  GstBaseTransformClass base_klvtimestamp_class;
};

GType gst_klvtimestamp_get_type (void);

G_END_DECLS

#endif /* _GST_KLVTIMESTAMP_H_ */
