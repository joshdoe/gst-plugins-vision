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

#ifndef _GST_EDT_PDV_SRC_H_
#define _GST_EDT_PDV_SRC_H_

#include <gst/base/gstpushsrc.h>

#include <edtinc.h>

G_BEGIN_DECLS

#define GST_TYPE_EDT_PDV_SRC   (gst_edt_pdv_src_get_type())
#define GST_EDT_PDV_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_EDT_PDV_SRC,GstEdtPdvSrc))
#define GST_EDT_PDV_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_EDT_PDV_SRC,GstEdtPdvSrcClass))
#define GST_IS_EDT_PDV_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_EDT_PDV_SRC))
#define GST_IS_EDT_PDV_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_EDT_PDV_SRC))

typedef struct _GstEdtPdvSrc GstEdtPdvSrc;
typedef struct _GstEdtPdvSrcClass GstEdtPdvSrcClass;

/**
* GstEdtPdvSrcConnector:
* @MC_Connector_VID<1..16>: channel is linked to camera at the VID<1..16> input
* @MC_Connector_YC: channel is linked to a camera at the YC input
* 
*
* Identifies the connector that the camera is connected to.
*/
typedef enum {
  
} GstEdtPdvSrcConnector;

struct _GstEdtPdvSrc
{
  GstPushSrc base_edt_pdv_src;

  /* properties */
  guint unit;
  guint channel;

  PdvDev *dev;
  gboolean acq_started;

  gint total_timeouts;

  gint height;
  gint gst_stride;
  guint px_stride;
};

struct _GstEdtPdvSrcClass
{
  GstPushSrcClass base_edt_pdv_src_class;
};

GType gst_edt_pdv_src_get_type (void);

G_END_DECLS

#endif
