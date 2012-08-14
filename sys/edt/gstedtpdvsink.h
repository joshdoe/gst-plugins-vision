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

#ifndef _GST_EDT_PDV_SINK_H_
#define _GST_EDT_PDV_SINK_H_

#include <gst/base/gstbasesink.h>
#include <edtinc.h>

G_BEGIN_DECLS

#define GST_TYPE_EDT_PDV_SINK   (gst_edt_pdv_sink_get_type())
#define GST_EDT_PDV_SINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_EDT_PDV_SINK,GstEdtPdvSink))
#define GST_EDT_PDV_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_EDT_PDV_SINK,GstEdtPdvSinkClass))
#define GST_IS_EDT_PDV_SINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_EDT_PDV_SINK))
#define GST_IS_EDT_PDV_SINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_EDT_PDV_SINK))

typedef struct _GstEdtPdvSink GstEdtPdvSink;
typedef struct _GstEdtPdvSinkClass GstEdtPdvSinkClass;

struct _GstEdtPdvSink
{
  GstBaseSink base;

  PdvDev *dev;
  int unit;
  int channel;

  unsigned char **buffers;
  int n_buffers;
  int cur_buffer;
};

struct _GstEdtPdvSinkClass
{
  GstBaseSinkClass base_class;
};

GType gst_edt_pdv_sink_get_type (void);

G_END_DECLS

#endif /* _GST_EDT_PDV_SINK_H_ */