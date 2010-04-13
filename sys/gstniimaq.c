/* GStreamer
 * Copyright (C) <2006> Eric Jonas <jonas@mit.edu>
 * Copyright (C) <2006> Antoine Tremblay <hexa00@gmail.com>
 * Copyright (C) 2010 United States Government, Joshua M. Doe <oss@nvl.army.mil>
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

/**
 * SECTION:element-niimaqsrc
 *
 * Source for IIDC (Instrumentation & Industrial Digital Camera) firewire
 * cameras.
 * 
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v niimaqsrc camera-number=0 ! xvimagesink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstniimaq.h"
//#include <sys/time.h>
#include <time.h>
#include <string.h>

GST_DEBUG_CATEGORY (niimaq_debug);
#define GST_CAT_DEFAULT niimaq_debug

static GstElementDetails niimaq_details =
GST_ELEMENT_DETAILS ("NI-IMAQ Video Source",
    "Source/Video",
    "National Instruments based source, supports CameraLink cameras",
    "Josh Doe <oss@nvl.army.mil>");

enum
{
  PROP_0,
  PROP_TIMESTAMP_OFFSET,
  PROP_BUFSIZE
      /* FILL ME */
};

#define DEFAULT_PROP_TIMESTAMP_OFFSET  0
#define DEFAULT_PROP_BUFSIZE  10

GST_BOILERPLATE (GstNiImaq, gst_niimaq, GstPushSrc, GST_TYPE_PUSH_SRC);

/* GObject virtual methods */
static void gst_niimaq_dispose (GObject * object);
static void gst_niimaq_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_niimaq_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* GstBaseSrc virtual methods */
static GstCaps *gst_niimaq_get_caps (GstBaseSrc * bsrc);
static gboolean gst_niimaq_set_caps (GstBaseSrc * bsrc, GstCaps * caps);
static void gst_niimaq_src_fixate (GstPad * pad, GstCaps * caps);
static void gst_niimaq_get_times (GstBaseSrc * basesrc,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static gboolean gst_niimaq_start (GstBaseSrc * src);
static gboolean gst_niimaq_stop (GstBaseSrc * src);

/* GstPushSrc virtual methods */
static GstFlowReturn gst_niimaq_create (GstPushSrc * psrc, GstBuffer ** buffer);

/* GstNiImaq methods */
static gboolean gst_niimaq_parse_caps (const GstCaps * caps,
    gint * width,
    gint * height,
    gint * rate_numerator, gint * rate_denominator, gint * depth, gint * bpp);

static gboolean gst_niimaq_set_caps_color (GstStructure * gs, int bpp, int depth);
static gboolean gst_niimaq_set_caps_framesize (GstStructure * gs, gint width,
    gint height);

static GstCaps *gst_niimaq_get_all_niimaq_caps ();
static GstCaps *gst_niimaq_get_cam_caps (GstNiImaq * src);

static void _____BEGIN_FUNCTIONS_____();

static void
gst_niimaq_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (element_class, &niimaq_details);

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_niimaq_get_all_niimaq_caps ()));

}

static void
gst_niimaq_class_init (GstNiImaqClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->dispose = gst_niimaq_dispose;
  gobject_class->set_property = gst_niimaq_set_property;
  gobject_class->get_property = gst_niimaq_get_property;

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_TIMESTAMP_OFFSET, g_param_spec_int64 ("timestamp-offset",
          "Timestamp offset",
          "An offset added to timestamps set on buffers (in ns)", G_MININT64,
          G_MAXINT64, DEFAULT_PROP_TIMESTAMP_OFFSET, G_PARAM_READWRITE));

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_BUFSIZE, g_param_spec_int ("buffer-size",
          "The number of frames in the dma ringbuffer",
          "The number of frames in the dma ringbuffer", 1,
          G_MAXINT, DEFAULT_PROP_BUFSIZE, G_PARAM_READWRITE));

  gstbasesrc_class->get_caps = gst_niimaq_get_caps;
  gstbasesrc_class->set_caps = gst_niimaq_set_caps;

  gstbasesrc_class->get_times = gst_niimaq_get_times;
  gstpushsrc_class->create = gst_niimaq_create;
  gstbasesrc_class->start = gst_niimaq_start;
  gstbasesrc_class->stop = gst_niimaq_stop;
}

static void
gst_niimaq_init (GstNiImaq * src, GstNiImaqClass * g_class)
{
  GstPad *srcpad = GST_BASE_SRC_PAD (src);

  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
  gst_pad_use_fixed_caps (srcpad);

  src->timestamp_offset = 0;
  src->caps = gst_niimaq_get_all_niimaq_caps ();
  src->bufsize = 10;
  src->n_frames = 0;
  src->cumbufnum = 0;
  src->n_dropped_frames = 0;
  src->buflist = 0;
  src->sid = 0;
  src->iid = 0;
  src->device_name = g_strdup_printf ("img2");

}

static void
gst_niimaq_dispose (GObject * object)
{
	GstNiImaq *src = GST_NIIMAQ (object);

	g_free (src->device_name);
	src->device_name = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_niimaq_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNiImaq *src = GST_NIIMAQ (object);

  switch (prop_id) {
    case PROP_TIMESTAMP_OFFSET:
      src->timestamp_offset = g_value_get_int64 (value);
      break;
    case PROP_BUFSIZE:
      src->bufsize = g_value_get_int (value);
    default:
      break;
  }
}

static void
gst_niimaq_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNiImaq *src = GST_NIIMAQ (object);

  switch (prop_id) {
    case PROP_TIMESTAMP_OFFSET:
      g_value_set_int64 (value, src->timestamp_offset);
      break;
    case PROP_BUFSIZE:
      g_value_set_int (value, src->bufsize);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_niimaq_get_caps (GstBaseSrc * bsrc)
{
  GstNiImaq *gsrc;

  gsrc = GST_NIIMAQ (bsrc);

  g_return_val_if_fail (gsrc->caps, NULL);

  return gst_caps_copy (gsrc->caps);
}


static gboolean
gst_niimaq_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{

  gboolean res = TRUE;
  GstNiImaq *niimaq;
  gint width, height, rate_denominator, rate_numerator;
  gint bpp, depth;

  niimaq = GST_NIIMAQ (bsrc);

  if (niimaq->caps) {
    gst_caps_unref (niimaq->caps);
  }

  niimaq->caps = gst_niimaq_get_cam_caps(niimaq);

  res = gst_niimaq_parse_caps (niimaq->caps, &width, &height,
      &rate_numerator, &rate_denominator, &depth, &bpp);

  if (res) {
    /* looks ok here */
    niimaq->width = width;
    niimaq->height = height;
    niimaq->rate_numerator = rate_numerator;
    niimaq->rate_denominator = rate_denominator;
	niimaq->depth = depth;
    niimaq->bpp = bpp;
	niimaq->framesize = width * height * (depth/8);
  }

  return res;
}

static void
gst_niimaq_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (basesrc)) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

static GstFlowReturn
gst_niimaq_create (GstPushSrc * psrc, GstBuffer ** buffer)
{
  GstNiImaq *src;
  gpointer data;
  GstCaps *caps;
  GstFlowReturn res = GST_FLOW_OK;
  uInt32 newval, *bufaddr;
  Int32 rval;
  uInt32 dropped;

  src = GST_NIIMAQ (psrc);

  data = g_malloc(src->framesize);

  GST_INFO_OBJECT(src, "Examining buffer %d", src->cumbufnum);
  rval=imgSessionExamineBuffer2(src->sid, src->cumbufnum, &newval, &bufaddr);
  if (rval) {
	  GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
		  ("failed to examine buffer %d", src->cumbufnum), ("failed to examine buffer %d", src->cumbufnum));
	  goto error;
  }

  memcpy (data, (guchar *) bufaddr,
      src->framesize);

  imgSessionReleaseBuffer(src->sid);
  GST_INFO_OBJECT(src, "Releasing buffer %d", newval);

  *buffer = gst_buffer_new ();
  GST_BUFFER_DATA (*buffer) = data;
  GST_BUFFER_MALLOCDATA (*buffer) = data;
  GST_BUFFER_SIZE (*buffer) = src->framesize;

  caps = gst_pad_get_caps (GST_BASE_SRC_PAD (psrc));
  gst_buffer_set_caps (*buffer, caps);
  gst_caps_unref (caps);

  //GST_BUFFER_TIMESTAMP (outbuf) = src->timestamp_offset + src->running_time;
  //if (src->rate_numerator != 0) {
  //  GST_BUFFER_DURATION (outbuf) = gst_util_uint64_scale_int (GST_SECOND,
  //      src->rate_denominator, src->rate_numerator);
  //}

  dropped = newval - src->cumbufnum;
  if(dropped) {
	  src->n_dropped_frames += dropped;
	  GST_WARNING_OBJECT(src, "Dropped %d frames (%d total)",dropped,src->n_dropped_frames);
  }

  src->cumbufnum = newval + 1;
  src->n_frames++;
  //if (src->rate_numerator != 0) {
  //  src->running_time = gst_util_uint64_scale_int (src->n_frames * GST_SECOND,
  //      src->rate_denominator, src->rate_numerator);
  //}

  return res;

error:
  {
    return GST_FLOW_ERROR;
  }
}


static gboolean
gst_niimaq_parse_caps (const GstCaps * caps,
    gint * width,
    gint * height,
    gint * rate_numerator, gint * rate_denominator, gint * depth, gint * bpp)
{
  const GstStructure *structure;
  GstPadLinkReturn ret;
  const GValue *framerate;

  if (gst_caps_get_size (caps) < 1)
    return FALSE;

  structure = gst_caps_get_structure (caps, 0);

  ret = gst_structure_get_int (structure, "width", width);
  ret &= gst_structure_get_int (structure, "height", height);

  framerate = gst_structure_get_value (structure, "framerate");

  ret &= gst_structure_get_int (structure, "depth", depth);

  ret &= gst_structure_get_int (structure, "bpp", bpp);


  if (framerate) {
    *rate_numerator = gst_value_get_fraction_numerator (framerate);
    *rate_denominator = gst_value_get_fraction_denominator (framerate);
  } else {
    ret = FALSE;
  }

  return ret;
}

/* Set color on caps */
static gboolean
gst_niimaq_set_caps_color (GstStructure * gs, int bpp, int depth)
{
  gboolean ret = TRUE;

  gst_structure_set_name (gs, "video/x-raw-gray");
  gst_structure_set (gs,
      "bpp", G_TYPE_INT, bpp,
	  "depth", G_TYPE_INT, depth, NULL);
  if(depth>8)
	  gst_structure_set(gs, "endianness", G_TYPE_INT, G_LITTLE_ENDIAN,NULL);

  return ret;
}


static gboolean
gst_niimaq_set_caps_framesize (GstStructure * gs, gint width, gint height)
{
  gst_structure_set (gs,
      "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);
  return TRUE;
}

GstCaps *
gst_niimaq_get_all_niimaq_caps ()
{
  /* 
     generate all possible caps

   */

  GstCaps *gcaps;
  GstStructure *gs;
  gint i = 0;

  gcaps = gst_caps_new_empty ();

  gs = gst_structure_empty_new ("video");
  gst_structure_set_name (gs, "video/x-raw-gray");
  gst_structure_set (gs,
	  "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
	  "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
	  "bpp", GST_TYPE_INT_RANGE, 10, 16,
	  "depth", G_TYPE_INT, 16,
	  "endianness", G_TYPE_INT, G_LITTLE_ENDIAN,
	  "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
  gst_caps_append_structure (gcaps, gs);

  gs = gst_structure_empty_new ("video");
  gst_structure_set_name (gs, "video/x-raw-gray");
  gst_structure_set (gs,
	  "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
	  "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
	  "bpp", G_TYPE_INT, 8,
	  "depth", G_TYPE_INT, 8,
	  "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
  gst_caps_append_structure (gcaps, gs);

  return gcaps;
}

GstCaps *
gst_niimaq_get_cam_caps (GstNiImaq * src)
{
  GstCaps *gcaps = NULL;
  Int32 rval;
  uInt32 val;
  int width, height, depth, bpp;
  GstStructure *gs;

  gcaps = gst_caps_new_empty ();

  if (!src->iid) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Camera interface not open"),
        ("Camera interface not open"));
    goto error;
  }

  rval &= imgGetAttribute(src->iid, IMG_ATTR_BITSPERPIXEL, &val);
  bpp = val;
  rval &= imgGetAttribute(src->iid, IMG_ATTR_BYTESPERPIXEL, &val);
  depth = val*8;
  rval &= imgGetAttribute(src->iid, IMG_ATTR_ROI_WIDTH, &val);
  width = val;
  rval &= imgGetAttribute(src->iid, IMG_ATTR_ROI_HEIGHT, &val);
  height = val;

  if (rval) {
	  GST_ELEMENT_ERROR (src, STREAM, FAILED,
		  ("attempt to read attributes failed"),
		  ("attempt to read attributes failed"));
	  goto error;
  }

  gs = gst_structure_empty_new ("video");
  if (!gst_niimaq_set_caps_color(gs, bpp, depth) ||
	  !gst_niimaq_set_caps_framesize(gs, width, height)) {
    GST_ELEMENT_ERROR (src, STREAM, FAILED,
        ("attempt to set caps %dx%dx%d (%d) failed", width,height,depth, bpp),
        ("attempt to set caps %dx%dx%d (%d) failed", width,height,depth, bpp));
    goto error;
  }

  gst_structure_set(gs, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);

  gst_caps_append_structure (gcaps, gs);

  return gcaps;

error:

  if (gcaps) {
    gst_caps_unref (gcaps);
  }

  return NULL;
}

static gboolean
gst_niimaq_start (GstBaseSrc * src)
{
  GstNiImaq* filter = GST_NIIMAQ(src);
  Int32 rval;
  int i;

  GST_LOG_OBJECT (filter, "Opening camera interface: %s", filter->device_name);

  filter->iid = 0;
  filter->sid = 0;

  rval=imgInterfaceOpen(filter->device_name,&(filter->iid));

  if (rval) {
	  GST_ELEMENT_ERROR (filter, RESOURCE, FAILED, ("Failed to open camera interface"),
		  ("Failed to open camera interface %d", filter->iid));
	  goto error;
  }

  GST_LOG_OBJECT (filter, "Opening camera session: %s", filter->device_name);

  rval=imgSessionOpen(filter->iid, &(filter->sid));
  if (rval) {
	  GST_ELEMENT_ERROR (filter, RESOURCE, FAILED, ("Failed to open camera session"),
		  ("Failed to open camera session %d", filter->sid));
	  goto error;
  }

  GST_LOG_OBJECT (filter, "Creating ring with %d buffers", filter->bufsize);

  filter->buflist = g_new(guint32*, filter->bufsize);
  for(i=0;i<filter->bufsize;i++) {
	  filter->buflist[i] = 0;
  }
  rval=imgRingSetup(filter->sid, filter->bufsize, (void**)(filter->buflist), 0, FALSE);
  if(rval) {
	  GST_ELEMENT_ERROR (filter, RESOURCE, FAILED, ("Failed to create ring buffer"),
		  ("Failed to create ring buffer with %d buffers", filter->bufsize));
	  goto error;
  }

  //GST_LOG_OBJECT (filter, "Registering callback functions");
  //rval=imgSessionWaitSignalAsync2(filter->sid, IMG_SIGNAL_STATUS, IMG_BUF_COMPLETE, IMG_SIGNAL_STATE_RISING, Imaq_BUF_COMPLETE, filter);
  //if(rval) {
	 // GST_ELEMENT_ERROR (filter, RESOURCE, FAILED, ("Failed to register BUF_COMPLETE callback"),
		//  ("Failed to register BUF_COMPLETE callback"));
	 // goto error;
  //}

  GST_LOG_OBJECT (filter, "Starting acquisition");

  rval=imgSessionStartAcquisition(filter->sid);

  i = 0;
  while (rval != 0 && i++ < 5) {
	  g_usleep (50000);
	  if (rval=imgSessionStartAcquisition(filter->sid)) {
		  if (rval != 0) {
			  GST_LOG_OBJECT (src, "camera is still off , retrying");
		  }
	  }
  }

  if (i >= 5) {
	  GST_ELEMENT_ERROR (filter, RESOURCE, FAILED,
		  ("Camera doesn't seem to want to turn on!"),
		  ("Camera doesn't seem to want to turn on!"));
	  goto error;
  }

  GST_LOG_OBJECT (src, "got transmision status ON");

  return TRUE;

error:

  if(filter->sid)
	  imgClose(filter->sid,TRUE);
  filter->sid = 0;
  if(filter->iid)
	  imgClose(filter->iid,TRUE);
  filter->iid = 0;

  return FALSE;;

}


gboolean gst_niimaq_stop( GstBaseSrc * src )
{
	GstNiImaq* filter = GST_NIIMAQ(src);
	Int32 rval;

	rval=imgSessionStopAcquisition(filter->sid);
	if (rval) {
		GST_ELEMENT_ERROR (filter, RESOURCE, FAILED, ("Unable to stop transmision"),
			("Unable to stop transmision"));
	}

	if(filter->sid)
		imgClose(filter->sid,TRUE);
	filter->sid = 0;
	if(filter->iid)
		imgClose(filter->iid,TRUE);
	filter->iid = 0;

	GST_DEBUG_OBJECT (filter, "Capture stoped");

	return TRUE;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
	GST_DEBUG_CATEGORY_INIT (niimaq_debug, "niimaq", 0, "NI-IMAQ interface");

	return gst_element_register (plugin, "niimaqsrc", GST_RANK_NONE,
		GST_TYPE_NIIMAQ);

}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
GST_VERSION_MINOR,
				   "niimaq",
				   "NI-IMAQ Video Source",
				   plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)



// Enumerate cams
//	rval=imgInterfaceQueryNames(idx, name);
//if(rval)
//break;
//
//rval=imgInterfaceOpen(name, &iid);
//if(rval)
//continue;
//
//imgGetAttribute(iid, IMG_ATTR_NUM_PORTS, &nPorts);
//imgClose(iid, TRUE);
//for(j=0;j<nPorts;j++) {
//	if(nPorts>1) {
//		char num[33];
//		itoa(j,num,10);
//		strcat(name,"::"); //FIXME: this is probably not safe
//		strcat(name,num); //FIXME: this is probably not safe
//	}
//	rval=imgInterfaceOpen(name, &iid);
//	if(rval)
//		continue;
