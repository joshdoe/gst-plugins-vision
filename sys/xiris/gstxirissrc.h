/* GStreamer
 * Copyright (C) 2022 MinhQuan Tran <minhquan.tran@adlinktech.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef _GST_XIRISSRC_H_
#define _GST_XIRISSRC_H_

#include <gst/base/gstpushsrc.h>
#include "gstxiris.hpp"

G_BEGIN_DECLS

#define GST_TYPE_XIRISSRC           (gst_xirissrc_get_type())
#define GST_XIRISSRC(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_XIRISSRC,GstXirisSrc))
#define GST_XIRISSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_XIRISSRC,GstXirisSrcClass))
#define GST_IS_XIRISSRC(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_XIRISSRC))
#define GST_IS_XIRISSRC_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_XIRISSRC))

typedef struct _GstXirisSrc GstXirisSrc;
typedef struct _GstXirisSrcClass GstXirisSrcClass;

struct _GstXirisSrc
{
  GstPushSrc base_xirissrc;
  DemoCameraEventSink *sinkEvents;
  DemoCameraDetectorEventSink *detectorEvents;
  GstCaps *caps;
  gboolean camera_connected;
  gchar *shutter_mode;
  float global_exposure;
  float global_frame_rate_limit;
  gboolean global_frame_rate_limit_enabled;
  double rolling_frame_rate;
  gchar *pixel_depth;
  XPixelType pixel_type;
};

struct _GstXirisSrcClass
{
  GstPushSrcClass base_xirissrc_class;
};

GType gst_xirissrc_get_type (void);

G_END_DECLS

#endif /* _GST_XIRISSRC_H_ */