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

#ifndef _GST_GIGESIM_H_
#define _GST_GIGESIM_H_

#include <gst/base/gstbasesink.h>
#include <GigeSimSDK.h>

G_BEGIN_DECLS

#define GST_TYPE_GIGESIMSINK   (gst_gigesimsink_get_type())
#define GST_GIGESIMSINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GIGESIMSINK,GstGigesimSink))
#define GST_GIGESIMSINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GIGESIMSINK,GstGigesimSinkClass))
#define GST_IS_GIGESIMSINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GIGESIMSINK))
#define GST_IS_GIGESIMSINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GIGESIMSINK))

typedef struct _GstGigesimSink GstGigesimSink;
typedef struct _GstGigesimSinkClass GstGigesimSinkClass;

struct _GstGigesimSink
{
  GstBaseSink base;

  CGevCamera* pCamera;

  gint timeout;
  gchar *address;
  gchar *manufacturer;
  gchar *model;
  gchar *version;
  gchar *info;
  gchar *serial;
  gchar *mac;

  gboolean camera_connected;
  GstVideoInfo vinfo;
  
  GMutex mutex;
  GCond cond;
  gboolean acquisition_started;
  gboolean stop_requested;
};

struct _GstGigesimSinkClass
{
  GstBaseSinkClass base_class;
};

GType gst_gigesimsink_get_type (void);

G_END_DECLS

#endif /* _GST_GIGESIM_H_ */