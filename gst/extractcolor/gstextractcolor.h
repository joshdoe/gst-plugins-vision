/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2003 Arwed v. Merkatz <v.merkatz@gmx.net>
 * Copyright (C) 2006 Mark Nauwelaerts <manauw@skynet.be>
 * Copyright (C) 2015 United States Government, Joshua M. Doe <oss@nvl.army.mil>
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


#ifndef __GST_EXTRACT_COLOR_H__
#define __GST_EXTRACT_COLOR_H__

#include <gst/video/gstvideofilter.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_EXTRACT_COLOR \
  (gst_extract_color_get_type())
#define GST_EXTRACT_COLOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_EXTRACT_COLOR,GstExtractColor))
#define GST_EXTRACT_COLOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_EXTRACT_COLOR,GstExtractColorClass))
#define GST_IS_EXTRACT_COLOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_EXTRACT_COLOR))
#define GST_IS_EXTRACT_COLOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_EXTRACT_COLOR))

typedef struct _GstExtractColor GstExtractColor;
typedef struct _GstExtractColorClass GstExtractColorClass;

/**
* GstExtractColorComponent:
* @GST_EXTRACT_COLOR_COMPONENT_RED: extract red component
* @GST_EXTRACT_COLOR_COMPONENT_GREEN: extract green component
* @GST_EXTRACT_COLOR_COMPONENT_BLUE: extract blue component
*
* Component to extract.
*/
typedef enum {
  GST_EXTRACT_COLOR_COMPONENT_RED,
  GST_EXTRACT_COLOR_COMPONENT_GREEN,
  GST_EXTRACT_COLOR_COMPONENT_BLUE
} GstExtractColorComponent;

/**
* GstExtractColor:
* @element: the parent element.
*
*
* The opaque GstExtractColor data structure.
*/
struct _GstExtractColor
{
  GstVideoFilter element;

  /* format */
  GstVideoInfo info_in;
  GstVideoInfo info_out;

  /* properties */
  GstExtractColorComponent component;
};

struct _GstExtractColorClass
{
  GstVideoFilterClass parent_class;
};

GType gst_extract_color_get_type(void);

G_END_DECLS

#endif /* __GST_EXTRACT_COLOR_H__ */
