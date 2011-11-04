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

#ifndef _GST_PHOENIX_SRC_H_
#define _GST_PHOENIX_SRC_H_

#include <gst/base/gstpushsrc.h>

// TODO: if/elif for linux/mac/etc
#define _PHX_WIN32
#include <phx_api.h>

G_BEGIN_DECLS

#define GST_TYPE_PHOENIX_SRC   (gst_phoenixsrc_get_type())
#define GST_PHOENIX_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PHOENIX_SRC,GstPhoenixSrc))
#define GST_PHOENIX_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PHOENIX_SRC,GstPhoenixSrcClass))
#define GST_IS_PHOENIX_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PHOENIX_SRC))
#define GST_IS_PHOENIX_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PHOENIX_SRC))

typedef struct _GstPhoenixSrc GstPhoenixSrc;
typedef struct _GstPhoenixSrcClass GstPhoenixSrcClass;

/**
* GstPhoenixSrcConnector:
* @MC_Connector_VID<1..16>: channel is linked to camera at the VID<1..16> input
* @MC_Connector_YC: channel is linked to a camera at the YC input
* 
*
* Identifies the connector that the camera is connected to.
*/
typedef enum {
  
} GstPhoenixSrcConnector;

struct _GstPhoenixSrc
{
  GstPushSrc base_phoenixsrc;

  GstPad *srcpad;

  GstCaps *caps;

  gint dropped_frame_count;
  gboolean acq_started;

  /* camera handle */
  tHandle hCamera;
  
  guint32 buffer_size;
  //INT32 last_time_code;
  //INT32 boardType;
  //INT32 boardIdx;
  //INT32 cameraType;
  //INT32 connector;

  gchar *config_filepath;
  gboolean buffer_ready;
  guint buffer_ready_count;
  gboolean timeout_occurred;
  gboolean fifo_overflow_occurred;

  GMutex *mutex;
  GCond *cond;
};

struct _GstPhoenixSrcClass
{
  GstPushSrcClass base_phoenixsrc_class;
};

GType gst_phoenixsrc_get_type (void);

G_END_DECLS

#endif
