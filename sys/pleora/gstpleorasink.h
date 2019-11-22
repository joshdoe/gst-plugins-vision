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

#ifndef _GST_PLEORASINK_H_
#define _GST_PLEORASINK_H_

#include <gst/base/gstbasesink.h>
#include <gst/video/video.h>

class GstStreamingChannelSource;
class PvSoftDeviceGEV;
class IPvRegister;

G_BEGIN_DECLS

#define GST_TYPE_PLEORASINK   (gst_pleorasink_get_type())
#define GST_PLEORASINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLEORASINK,GstPleoraSink))
#define GST_PLEORASINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PLEORASINK,GstPleoraSinkClass))
#define GST_IS_PLEORASINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLEORASINK))
#define GST_IS_PLEORASINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PLEORASINK))

typedef struct _GstPleoraSink GstPleoraSink;
typedef struct _GstPleoraSinkClass GstPleoraSinkClass;

struct _GstPleoraSink
{
  GstBaseSink base;

  GstStreamingChannelSource *source;
  PvSoftDeviceGEV *device;

  gint num_internal_buffers;
  gchar *address;
  gchar *manufacturer;
  gchar *model;
  gchar *version;
  gchar *info;
  gchar *serial;
  gchar *mac;
  gboolean output_klv;
  guint32 packet_size;
  gboolean auto_multicast;
  gchar *multicast_group;
  gint multicast_port;

  gboolean camera_connected;
  GstVideoInfo vinfo;
  
  GMutex mutex;
  GCond cond;
  gboolean acquisition_started;
  gboolean stop_requested;

  IPvRegister *register_SCDA0;
  IPvRegister *register_SCPS0;
  IPvRegister *register_SCP0;
  IPvRegister *register_AcquisitionStart0;
  IPvRegister *register_AcquisitionStop0;
};

struct _GstPleoraSinkClass
{
  GstBaseSinkClass base_class;
};

GType gst_pleorasink_get_type (void);

G_END_DECLS

#endif /* _GST_PLEORASINK_H_ */
