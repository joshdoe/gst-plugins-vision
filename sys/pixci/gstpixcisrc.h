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

#ifndef _GST_PIXCI_SRC_H_
#define _GST_PIXCI_SRC_H_

#include <gst/base/gstpushsrc.h>

#ifdef WIN32
#include <windows.h>
#endif
#include <xcliball.h>

G_BEGIN_DECLS

#define GST_TYPE_PIXCI_SRC   (gst_pixcisrc_get_type())
#define GST_PIXCI_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PIXCI_SRC,GstPixciSrc))
#define GST_PIXCI_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PIXCI_SRC,GstPixciSrcClass))
#define GST_IS_PIXCI_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PIXCI_SRC))
#define GST_IS_PIXCI_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PIXCI_SRC))

typedef struct _GstPixciSrc GstPixciSrc;
typedef struct _GstPixciSrcClass GstPixciSrcClass;

/**
* GstPixciSrcConnector:
* @MC_Connector_VID<1..16>: channel is linked to camera at the VID<1..16> input
* @MC_Connector_YC: channel is linked to a camera at the YC input
* 
*
* Identifies the connector that the camera is connected to.
*/
#ifndef __GNUC__
typedef enum {
  
} GstPixciSrcConnector;
#endif

typedef enum {
    GST_PIXCI_VIDEO_FORMAT_CCIR,
    GST_PIXCI_VIDEO_FORMAT_CCIR_SQR,
    GST_PIXCI_VIDEO_FORMAT_NTSC,
    GST_PIXCI_VIDEO_FORMAT_NTSC_4_43,
    GST_PIXCI_VIDEO_FORMAT_NTSC_J,
    GST_PIXCI_VIDEO_FORMAT_NTSC_SQR,
    GST_PIXCI_VIDEO_FORMAT_NTSC_YC,
    GST_PIXCI_VIDEO_FORMAT_NTSC_YC_SQR,
    GST_PIXCI_VIDEO_FORMAT_PAL,
    GST_PIXCI_VIDEO_FORMAT_PAL_60,
    GST_PIXCI_VIDEO_FORMAT_PAL_M,
    GST_PIXCI_VIDEO_FORMAT_PAL_M_YC,
    GST_PIXCI_VIDEO_FORMAT_PAL_N,
    GST_PIXCI_VIDEO_FORMAT_PAL_N_YC,
    GST_PIXCI_VIDEO_FORMAT_PAL_SQR,
    GST_PIXCI_VIDEO_FORMAT_PAL_YC,
    GST_PIXCI_VIDEO_FORMAT_PAL_YC_SQR,
    GST_PIXCI_VIDEO_FORMAT_RS_170,
    GST_PIXCI_VIDEO_FORMAT_RS_170_SQR,
    GST_PIXCI_VIDEO_FORMAT_RS343_875i_60Hz,
    GST_PIXCI_VIDEO_FORMAT_RS343_875i_60Hz_RGB,
    GST_PIXCI_VIDEO_FORMAT_SECAM,
    GST_PIXCI_VIDEO_FORMAT_SECAM_YC,
    GST_PIXCI_VIDEO_FORMAT_SVGA_800x600_60Hz_RGB,
    GST_PIXCI_VIDEO_FORMAT_SXGA_1280x1024_60Hz_RGB,
    GST_PIXCI_VIDEO_FORMAT_VGA_640x480_60Hz_RGB,
    GST_PIXCI_VIDEO_FORMAT_Video_1280x720p_50Hz,
    GST_PIXCI_VIDEO_FORMAT_Video_1280x720p_50Hz_RGB,
    GST_PIXCI_VIDEO_FORMAT_Video_1280x720p_60Hz,
    GST_PIXCI_VIDEO_FORMAT_Video_1280x720p_60Hz_RGB,
    GST_PIXCI_VIDEO_FORMAT_Video_1920x1080i_50Hz,
    GST_PIXCI_VIDEO_FORMAT_Video_1920x1080i_50Hz_RGB,
    GST_PIXCI_VIDEO_FORMAT_Video_1920x1080i_60Hz,
    GST_PIXCI_VIDEO_FORMAT_Video_1920x1080i_60Hz_RGB,
    GST_PIXCI_VIDEO_FORMAT_Video_720x480i_60Hz,
    GST_PIXCI_VIDEO_FORMAT_Video_720x480i_60Hz_Color,
    GST_PIXCI_VIDEO_FORMAT_Video_720x480i_60Hz_RGB,
    GST_PIXCI_VIDEO_FORMAT_Video_720x480i_60Hz_Y_Color,
    GST_PIXCI_VIDEO_FORMAT_Video_720x576i_50Hz,
    GST_PIXCI_VIDEO_FORMAT_Video_720x576i_50Hz_Color,
    GST_PIXCI_VIDEO_FORMAT_Video_720x576i_50Hz_RGB,
    GST_PIXCI_VIDEO_FORMAT_Video_720x576i_50Hz_Y_Color,
    GST_PIXCI_VIDEO_FORMAT_XGA_1024x768_60Hz_RGB
} GstPixciVideoFormatEnum;

struct _GstPixciSrc
{
  GstPushSrc base_pixcisrc;

  gint dropped_frame_count;
  gboolean acq_started;

  /* camera handle */

  /* properties */
  GstPixciVideoFormatEnum format_name;
  gchar *format_file;
  gchar *driver_params;
  guint num_capture_buffers;
  guint board;
  guint channel;
  guint timeout;

  gboolean pixci_open;
  int unitmap;

  GstClockTime first_pixci_ts;
  guint64 *frame_start_times;
  guint64 *frame_end_times;
  gboolean buffer_ready;
  guint buffer_ready_count;
  guint frame_start_count;
  guint frame_end_count;
  guint buffer_processed_count;
  gboolean timeout_occurred;
  gboolean fifo_overflow_occurred;

  gint height;
  gint gst_stride;
  guint px_stride;

  GMutex mutex;
  GCond cond;
};

struct _GstPixciSrcClass
{
  GstPushSrcClass base_pixcisrc_class;
};

GType gst_pixcisrc_get_type (void);

G_END_DECLS

#endif
