/* GStreamer
 * Copyright (C) <2006> Eric Jonas <jonas@mit.edu>
 * Copyright (C) <2006> Antoine Tremblay <hexa00@gmail.com>
 * Copyright (C) 2013 United States Government, Joshua M. Doe <oss@nvl.army.mil>
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

#ifndef __GST_NIIMAQDXSRC_H__
#define __GST_NIIMAQDXSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#include <niimaqdx.h>

G_BEGIN_DECLS

#define GST_TYPE_NIIMAQDXSRC \
  (gst_niimaqdxsrc_get_type())
#define GST_NIIMAQDXSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_NIIMAQDXSRC,GstNiImaqDxSrc))
#define GST_NIIMAQDXSRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_NIIMAQDXSRC,GstNiImaqDxSrcClass))
#define GST_IS_NIIMAQDXSRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_NIIMAQDXSRC))
#define GST_IS_NIIMAQDXSRC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_NIIMAQDXSRC))
#define GST_NIIMAQDXSRC_GET_CLASS(klass) \
  (G_TYPE_INSTANCE_GET_CLASS ((klass), GST_TYPE_NIIMAQDXSRC, GstNiImaqDxSrcClass))

typedef struct _GstNiImaqDxSrc GstNiImaqDxSrc;
typedef struct _GstNiImaqDxSrcClass GstNiImaqDxSrcClass;

typedef struct
{
    const char *pixel_format;
    int endianness;
    const char *gst_caps_string;
    int bpp;
    int depth;
    int row_multiple;
} ImaqDxCapsInfo;

struct _GstNiImaqDxSrc {
  GstPushSrc element;

  /* properties */
  gchar *device_name;
  gint ringbuffer_count;
  gchar *attributes;
  gboolean bayer_as_gray;

  /* image info */
  char pixel_format[IMAQDX_MAX_API_STRING_LENGTH];
  int width;
  int height;
  int dx_row_stride;
  gint dx_framesize;
  guint8 *temp_buffer;
  const ImaqDxCapsInfo *caps_info;

  uInt32 cumbufnum;
  gint64 n_dropped_frames;
  
  IMAQdxSession session;

  gboolean session_started;

  GAsyncQueue *time_queue;
};

struct _GstNiImaqDxSrcClass {
  GstPushSrcClass parent_class;

  /* probed interfaces */
  GList *devices;
};

GType gst_niimaqdxsrc_get_type (void);

G_END_DECLS

#endif /* __GST_NIIMAQDXSRC_H__ */
