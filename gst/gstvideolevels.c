/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) 2003 Arwed v. Merkatz <v.merkatz@gmx.net>
 * Copyright (C) 2006 Mark Nauwelaerts <manauw@skynet.be>
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

/*
 * This file was (probably) generated from
 * gstvideotemplate.c,v 1.12 2004/01/07 21:07:12 ds Exp 
 * and
 * make_filter,v 1.6 2004/01/07 21:33:01 ds Exp 
 */

/**
 * SECTION:element-gamma
 *
 * Performs gamma correction on a video stream.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch videotestsrc ! gamma gamma=2.0 ! ffmpegcolorspace ! ximagesink
 * ]| This pipeline will make the image "brighter".
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvideolevels.h"
#ifdef HAVE_LIBOIL
#include <liboil/liboil.h>
#endif
#include <string.h>
#include <math.h>

#include <gst/video/video.h>


GST_DEBUG_CATEGORY_STATIC (videolevels_debug);
#define GST_CAT_DEFAULT videolevels_debug

/* GstVideoLevels signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_LOWIN,
  PROP_HIGHIN,
  PROP_LOWOUT,
  PROP_HIGHOUT
      /* FILL ME */
};

#define DEFAULT_PROP_LOWIN  0
#define DEFAULT_PROP_HIGHIN  65535
#define DEFAULT_PROP_LOWOUT  0
#define DEFAULT_PROP_HIGHOUT  255

static const GstElementDetails videolevels_details =
GST_ELEMENT_DETAILS ("Video videolevels adjustment",
    "Filter/Effect/Video",
    "Adjusts videolevels on a video stream",
    "Josh Doe <oss@nvl.army.mil");

static GstStaticPadTemplate gst_videolevels_src_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
		"video/x-raw-gray, "                                  \
		"bpp = (int) [10,16], "                                    \
		"depth = (int) 16, "                                  \
		"endianness = (int) BYTE_ORDER, "                     \
		"width = " GST_VIDEO_SIZE_RANGE ", "                  \
		"height = " GST_VIDEO_SIZE_RANGE ", "                 \
		"framerate = " GST_VIDEO_FPS_RANGE
	)
);

static GstStaticPadTemplate gst_videolevels_sink_template =
GST_STATIC_PAD_TEMPLATE ("src",
						 GST_PAD_SRC,
						 GST_PAD_ALWAYS,
						 GST_STATIC_CAPS (
						 "video/x-raw-gray, "                                  \
						 "bpp = (int) 8, "                                    \
						 "depth = (int) 8, "                                  \
						 "endianness = (int) BYTE_ORDER, "                     \
						 "width = " GST_VIDEO_SIZE_RANGE ", "                  \
						 "height = " GST_VIDEO_SIZE_RANGE ", "                 \
						 "framerate = " GST_VIDEO_FPS_RANGE
						 )
						 );

//static GstStaticPadTemplate gst_videolevels_sink_template =
//GST_STATIC_PAD_TEMPLATE ("src",
//    GST_PAD_SRC,
//    GST_PAD_ALWAYS,
//    GST_STATIC_CAPS (
//		"video/x-raw-gray, "                                  \
//		"bpp = (int) 8, "                                     \
//		"depth = (int) 8, "                                   \
//		"endianness = (int) BYTE_ORDER, "                     \
//		"width = " GST_VIDEO_SIZE_RANGE ", "                  \
//		"height = " GST_VIDEO_SIZE_RANGE ", "                 \
//		"framerate = " GST_VIDEO_FPS_RANGE
//	)
//);

static void gst_videolevels_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_videolevels_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_videolevels_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps);
static GstFlowReturn gst_videolevels_transform (GstBaseTransform * base, GstBuffer * inbuf,
												GstBuffer * outbuf);
static GstCaps * gst_videolevels_transform_caps (GstBaseTransform * trans,
												 GstPadDirection direction, GstCaps * caps);
static gboolean gst_videolevels_get_unit_size (GstBaseTransform * base,
											 GstCaps * caps, guint * size);

static void reset(GstVideoLevels* filter);
static void calculate_tables (GstVideoLevels * videolevels);
static void do_levels (GstVideoLevels * videolevels, guint16 * indata, guint8* outdata, gint size);

GST_BOILERPLATE (GstVideoLevels, gst_videolevels, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

static void
gst_videolevels_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  GST_CAT_INFO(videolevels_debug, "gst_videolevels_base_init");
  
  gst_element_class_set_details (element_class, &videolevels_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_videolevels_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_videolevels_src_template));
}

static void
gst_videolevels_finalize (GObject *object)
{
  GstVideoLevels *videolevels;

  GST_CAT_INFO (videolevels_debug, "gst_videolevels_finalize");
  
  g_return_if_fail (GST_IS_VIDEOLEVELS (object));
  videolevels = GST_VIDEOLEVELS (object);
  g_free(videolevels->levels_table);
  
  if(G_OBJECT_CLASS(parent_class)->finalize) {
    G_OBJECT_CLASS(parent_class)->finalize(object);
  }
}

static void
gst_videolevels_class_init (GstVideoLevelsClass * g_class)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *trans_class;
  
  GST_CAT_INFO (videolevels_debug, "gst_videolevels_class_init");
  
  gobject_class = G_OBJECT_CLASS (g_class);
  trans_class = GST_BASE_TRANSFORM_CLASS (g_class);
 
  // Register GObject virtual functions
  gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_videolevels_finalize);
  gobject_class->set_property = GST_DEBUG_FUNCPTR(gst_videolevels_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR(gst_videolevels_get_property);

  // Install GObject properties
  g_object_class_install_property (gobject_class, PROP_LOWIN,
      g_param_spec_int ("low_in", "Lower Input Level", "Lower Input Level",
          0, 65535, DEFAULT_PROP_LOWIN, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_HIGHIN,
	  g_param_spec_int ("upper_in", "Upper Input Level", "Upper Input Level",
	  0, 65535, DEFAULT_PROP_HIGHIN, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_LOWOUT,
	  g_param_spec_int ("low_out", "Lower Output Level", "Lower Output Level",
	  0, 255, DEFAULT_PROP_LOWOUT, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_HIGHOUT,
	  g_param_spec_int ("upper_out", "Upper Output Level", "Upper Output Level",
	  0, 255, DEFAULT_PROP_HIGHOUT, G_PARAM_READWRITE));

  // Register GstBaseTransform virtual functions
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_videolevels_set_caps);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_videolevels_transform);
  trans_class->transform_caps = GST_DEBUG_FUNCPTR (gst_videolevels_transform_caps);
  trans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_videolevels_get_unit_size);
}

static void
gst_videolevels_init (GstVideoLevels * videolevels, GstVideoLevelsClass * g_class)
{
  GST_DEBUG_OBJECT (videolevels, "gst_videolevels_init");

  videolevels->width=0;
  videolevels->height=0;
  videolevels->bpp=16;
  videolevels->depth=16;
  videolevels->levels_table = g_malloc(65536);
  reset(videolevels);
  calculate_tables (videolevels);
}

static void
gst_videolevels_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  GstVideoLevels *videolevels;

  g_return_if_fail (GST_IS_VIDEOLEVELS (object));
  videolevels = GST_VIDEOLEVELS (object);

  GST_DEBUG ("gst_videolevels_set_property");
  switch (prop_id) {
    case PROP_LOWIN:
      videolevels->lower_input = g_value_get_int (value);
      calculate_tables (videolevels);
      break;
	case PROP_HIGHIN:
		videolevels->upper_input = g_value_get_int (value);
		calculate_tables (videolevels);
		break;
	case PROP_LOWOUT:
		videolevels->lower_output = g_value_get_int (value);
		calculate_tables (videolevels);
		break;
	case PROP_HIGHOUT:
		videolevels->upper_output = g_value_get_int (value);
		calculate_tables (videolevels);
		break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_videolevels_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVideoLevels *videolevels;

  g_return_if_fail (GST_IS_VIDEOLEVELS (object));
  videolevels = GST_VIDEOLEVELS (object);

  GST_INFO_OBJECT (videolevels, "gst_videolevels_get_property");
  switch (prop_id) {
    case PROP_LOWIN:
      g_value_set_int (value, videolevels->lower_input);
      break;
	case PROP_HIGHIN:
		g_value_set_int (value, videolevels->upper_input);
		break;
	case PROP_LOWOUT:
		g_value_set_int (value, videolevels->lower_output);
		break;
	case PROP_HIGHOUT:
		g_value_set_int (value, videolevels->upper_output);
		break;
	default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_videolevels_set_caps (GstBaseTransform * base, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstVideoLevels *levels;
  GstStructure *structure;
  gboolean res;

  levels = GST_VIDEOLEVELS (base);

  GST_DEBUG_OBJECT (levels,
      "set_caps: in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps, outcaps);

  structure = gst_caps_get_structure (incaps, 0);

  res = gst_structure_get_int (structure, "width", &levels->width);
  res &= gst_structure_get_int (structure, "height", &levels->height);
  res &= gst_structure_get_int (structure, "bpp", &levels->bpp);
  res &= gst_structure_get_int (structure, "depth", &levels->depth);
  if (!res)
    goto done;

  levels->size = levels->width * levels->height;
  calculate_tables(levels);

done:
  return res;
}

static GstCaps *
gst_videolevels_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps)
{
  GstVideoLevels *videolevels;
  GstCaps *newcaps;
  GstStructure *structure;
  GstStructure *newstruct;
  int bpp;

  videolevels = GST_VIDEOLEVELS (trans);
  
  GST_DEBUG_OBJECT (caps, "transforming caps (from)");

  structure = gst_caps_get_structure (caps, 0);

  newcaps = gst_caps_new_simple ("video/x-raw-gray", NULL);

  newstruct = gst_caps_get_structure (newcaps, 0);

  gst_structure_set_value (newstruct, "width",
	  gst_structure_get_value (structure, "width"));
  gst_structure_set_value (newstruct, "height",
	  gst_structure_get_value (structure, "height"));
  gst_structure_set_value (newstruct, "framerate",
	  gst_structure_get_value (structure, "framerate"));

  if (direction == GST_PAD_SRC) {
	  GST_CAT_INFO(videolevels_debug, "direction=SRC");
	  bpp = 16;
  } else {
	  GST_CAT_INFO(videolevels_debug, "direction=SINK");
	  bpp = 8;
  }

  gst_structure_set (newstruct,
	  "bpp", G_TYPE_INT, bpp,
	  "depth", G_TYPE_INT, bpp,
	  NULL
  );

  GST_DEBUG_OBJECT (newcaps, "transforming caps (into)");

  return newcaps;
}

static gboolean gst_videolevels_get_unit_size (GstBaseTransform * base,
											   GstCaps * caps, guint * size)
{
	GstStructure *structure;
	int width;
	int height;
	int pixsize;

	structure = gst_caps_get_structure (caps, 0);

	if (gst_structure_get_int (structure, "width", &width) &&
		gst_structure_get_int (structure, "height", &height) &&
		gst_structure_get_int (structure, "bpp", &pixsize)) {
			*size = width * height * (pixsize/8);
			GST_CAT_DEBUG(videolevels_debug, "Get unit size width=%d,height=%d,size=%d",width,height,*size);
			return TRUE;
	}
	GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
		("Incomplete caps, some required field missing"));
	return FALSE;
}

static void reset(GstVideoLevels* filter)
{
	filter->width = 0;
	filter->height = 0;	
	filter->lower_input = DEFAULT_PROP_LOWIN;
	filter->upper_input = DEFAULT_PROP_HIGHIN;
	filter->lower_output = DEFAULT_PROP_LOWOUT;
	filter->upper_output = DEFAULT_PROP_HIGHOUT;
}

static GstFlowReturn
gst_videolevels_transform (GstBaseTransform * base, GstBuffer * inbuf,
						   GstBuffer * outbuf)
{
	GstVideoLevels *filter = GST_VIDEOLEVELS (base);
	guint16 *input;
	guint8 *output;

	/*
	* We need to lock our filter params to prevent changing
	* caps in the middle of a transformation (nice way to get
	* segfaults)
	*/
	GST_OBJECT_LOCK (filter);

	input = (guint16 *) GST_BUFFER_DATA (inbuf);
	output = (guint8 *) GST_BUFFER_DATA (outbuf);

	do_levels (filter, input, output,
		filter->height * filter->width);

	GST_OBJECT_UNLOCK (filter);
	return GST_FLOW_OK;
}

static void
calculate_tables (GstVideoLevels * videolevels)
{
	int i;
	guint16 loIn, hiIn;
	guint8 loOut, hiOut;
	double slope;

	GST_INFO_OBJECT (videolevels, "gst_videolevels_get_property");

	GST_BASE_TRANSFORM (videolevels)->passthrough = FALSE;

	loIn = videolevels->lower_input;
	hiIn = videolevels->upper_input;
	loOut = videolevels->lower_output;
	hiOut = videolevels->upper_output;


	if(hiIn==loIn)
		slope=0;
	else
		slope = (double)(hiOut-loOut)/(hiIn-loIn);

	for(i=0;i<loIn;i++)
		videolevels->levels_table[i] = loOut;
	for(i=loIn;i<hiIn;i++) {
		videolevels->levels_table[i] = loOut+(guint8)((i-loIn)*slope);
	}
	for(i=hiIn;i<65536;i++)
		videolevels->levels_table[i] = hiOut;
}

static void
do_levels (GstVideoLevels * videolevels, guint16 * indata, guint8* outdata, gint size)
{
  int i;
  guint8* dst = outdata;
  guint16* src = indata;
  for (i = 0; i < size; i++) {
    *dst++ = videolevels->levels_table[*src++];
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (videolevels_debug, "videolevels", 0, "videolevels");
  GST_CAT_INFO(videolevels_debug, "plugin_init");
  return gst_element_register (plugin, "videolevels", GST_RANK_NONE, GST_TYPE_VIDEOLEVELS);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "videolevels",
    "Changes videolevels on video images",
    plugin_init,
	VERSION,
	GST_LICENSE,
	GST_PACKAGE_NAME,
	GST_PACKAGE_ORIGIN
);
