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

#ifndef _GST_SAPERA_SRC_H_
#define _GST_SAPERA_SRC_H_

#include <SapClassBasic.h>

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_SAPERA_SRC   (gst_saperasrc_get_type())
#define GST_SAPERA_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SAPERA_SRC,GstSaperaSrc))
#define GST_SAPERA_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SAPERA_SRC,GstSaperaSrcClass))
#define GST_IS_SAPERA_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SAPERA_SRC))
#define GST_IS_SAPERA_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SAPERA_SRC))

typedef struct _GstSaperaSrc GstSaperaSrc;
typedef struct _GstSaperaSrcClass GstSaperaSrcClass;

class SapMyProcessing;

struct _GstSaperaSrc
{
  GstPushSrc base_saperasrc;

  guint last_buffer_number;
  gint dropped_frame_count;
  gboolean acq_started;

  /* Sapera objects */
  SapAcquisition *sap_acq;
  SapBuffer      *sap_buffers;
  SapBayer       *sap_bayer;
  SapTransfer    *sap_xfer;
  SapMyProcessing*sap_pro;

  /* properties */
  gchar *format_file;
  guint num_capture_buffers;
  gint server_index;
  gint resource_index;

  GstBuffer *buffer;

  GstCaps *caps;
  gint height;
  gint gst_stride;

  GMutex buffer_mutex;
  GCond buffer_cond;
};

struct _GstSaperaSrcClass
{
  GstPushSrcClass base_saperasrc_class;
};

GType gst_saperasrc_get_type (void);

G_END_DECLS

#endif
