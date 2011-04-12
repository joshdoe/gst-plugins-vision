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

#ifndef _GST_IOTECHDAQX_H_
#define _GST_IOTECHDAQX_H_

#include <gst/base/gstpushsrc.h>
#include <windows.h>
#include <DAQX.h>

G_BEGIN_DECLS

#define GST_TYPE_IOTECHDAQX   (gst_iotechdaqx_get_type())
#define GST_IOTECHDAQX(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IOTECHDAQX,GstIOtechDaqX))
#define GST_IOTECHDAQX_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IOTECHDAQX,GstIOtechDaqXClass))
#define GST_IS_IOTECHDAQX(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IOTECHDAQX))
#define GST_IS_IOTECHDAQX_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IOTECHDAQX))

typedef struct _GstIOtechDaqX GstIOtechDaqX;
typedef struct _GstIOtechDaqXClass GstIOtechDaqXClass;

struct _GstIOtechDaqX
{
  GstPushSrc base_iotechdaqx;

  GstPad *srcpad;

  GstCaps *caps;
  gint rate;
  gint channels;
  gint endianness;
  gint width;
  gint depth;
  gboolean is_signed;

  gboolean opened;

  DaqHandleT handle;
};

struct _GstIOtechDaqXClass
{
  GstPushSrcClass base_iotechdaqx_class;
};

GType gst_iotechdaqx_get_type (void);

G_END_DECLS

#endif
