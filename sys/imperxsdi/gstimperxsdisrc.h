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

#ifndef _GST_IMPERX_SDI_SRC_H_
#define _GST_IMPERX_SDI_SRC_H_

#include <gst/base/gstpushsrc.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define bool gboolean
#include <VCESDI.h>

G_BEGIN_DECLS

#define GST_TYPE_IMPERX_SDI_SRC   (gst_imperxsdisrc_get_type())
#define GST_IMPERX_SDI_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IMPERX_SDI_SRC,GstImperxSdiSrc))
#define GST_IMPERX_SDI_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IMPERX_SDI_SRC,GstImperxSdiSrcClass))
#define GST_IS_IMPERX_SDI_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IMPERX_SDI_SRC))
#define GST_IS_IMPERX_SDI_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IMPERX_SDI_SRC))

typedef struct _GstImperxSdiSrc GstImperxSdiSrc;
typedef struct _GstImperxSdiSrcClass GstImperxSdiSrcClass;

struct _GstImperxSdiSrc
{
  GstPushSrc base_imperxsdisrc;

  gboolean acq_started;

  /* camera handle */
  VCESDI_GRABBER grabber;
  VCESDI_CameraData camera_data;

  /* properties */
  guint num_capture_buffers;
  guint board;
  gint timeout;

  GstBuffer *buffer;
  GstClockTime acq_start_time;

  GstCaps *caps;
  GstVideoFormat format;
  gint width;
  gint height;
  gfloat framerate;
  gboolean is_interlaced;
  gint gst_stride;
  gint imperx_stride;

  GMutex mutex;
  GCond cond;
  gboolean stop_requested;
};

struct _GstImperxSdiSrcClass
{
  GstPushSrcClass base_imperxsdisrc_class;
};

GType gst_imperxsdisrc_get_type (void);

G_END_DECLS

#endif
