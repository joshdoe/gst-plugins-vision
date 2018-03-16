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

#ifndef _GST_EURESYS_H_
#define _GST_EURESYS_H_

#include <gst/base/gstpushsrc.h>
#include <multicam.h>

G_BEGIN_DECLS

#define GST_TYPE_EURESYS   (gst_euresys_get_type())
#define GST_EURESYS(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_EURESYS,GstEuresys))
#define GST_EURESYS_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_EURESYS,GstEuresysClass))
#define GST_IS_EURESYS(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_EURESYS))
#define GST_IS_EURESYS_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_EURESYS))

typedef struct _GstEuresys GstEuresys;
typedef struct _GstEuresysClass GstEuresysClass;

/**
* GstEuresysConnector:
* @MC_Connector_VID<1..16>: channel is linked to camera at the VID<1..16> input
* @MC_Connector_YC: channel is linked to a camera at the YC input
* 
*
* Identifies the connector that the camera is connected to.
*/
typedef enum {
  GST_EURESYS_CONNECTOR_VID1,
  GST_EURESYS_CONNECTOR_VID2,
  GST_EURESYS_CONNECTOR_VID3,
  GST_EURESYS_CONNECTOR_VID4
} GstEuresysConnectorEnum;

typedef enum {
  GST_EURESYS_COLOR_FORMAT_Y8,
  GST_EURESYS_COLOR_FORMAT_RGB24,
  GST_EURESYS_COLOR_FORMAT_RGB32,
  GST_EURESYS_COLOR_FORMAT_ARGB32
} GstEuresysColorFormatEnum;

typedef enum {
  GST_EURESYS_CAMERA_EIA,
  GST_EURESYS_CAMERA_NTSC,
  GST_EURESYS_CAMERA_CCIR,
  GST_EURESYS_CAMERA_PAL
} GstEuresysCameraEnum;

typedef enum {
    GST_EURESYS_PIXEL_TIMING_SQUARE,
    GST_EURESYS_PIXEL_TIMING_BROADCAST
} GstEuresysPixelTimingEnum;

struct _GstEuresys
{
  GstPushSrc base_euresys;

  gint dropped_frame_count;
  gboolean acq_started;

  INT32 last_time_code;
  MCHANDLE hChannel;
  INT32 boardType;

  /* properties */
  INT32 boardIdx;
  GstEuresysCameraEnum cameraType;
  GstEuresysConnectorEnum connector;
  GstEuresysColorFormatEnum colorFormat;
  GstEuresysPixelTimingEnum pixelTiming;
  gint num_capture_buffers;
};

struct _GstEuresysClass
{
  GstPushSrcClass base_euresys_class;
};

GType gst_euresys_get_type (void);

G_END_DECLS

#endif
