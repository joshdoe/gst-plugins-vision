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
 * Source for National Instruments IMAQ frame grabber (Camera Link and analog cameras)
 * 
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v niimaqsrc ! ffmpegcolorspace ! autovideosink
 * ]|
 * </refsect2>
 */

/* FIXME: timestamps sent in GST_TAG_DATE_TIME are off, need to adjust for time of first buffer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstniimaq.h"

#include <time.h>
#include <string.h>

#include <gst/video/video.h>

/* prototype for private function to disable 32-bit memory check */
USER_FUNC niimaquDisable32bitPhysMemLimitEnforcement (SESSION_ID sid);

GST_DEBUG_CATEGORY (niimaqsrc_debug);
#define GST_CAT_DEFAULT niimaqsrc_debug

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_RING_BUFFER_COUNT,
  PROP_IS_SIGNED,
  PROP_TIMEOUT
};

#define DEFAULT_PROP_DEVICE "img0"
#define DEFAULT_PROP_RING_BUFFER_COUNT  2
#define DEFAULT_PROP_IS_SIGNED FALSE
#define DEFAULT_PROP_TIMEOUT 0

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ GRAY8, GRAY16_LE, GRAY16_BE }"))
    );

G_DEFINE_TYPE (GstNiImaqSrc, gst_niimaqsrc, GST_TYPE_PUSH_SRC);

/* GObject virtual methods */
static void gst_niimaqsrc_dispose (GObject * object);
static void gst_niimaqsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_niimaqsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* GstBaseSrc virtual methods */
static gboolean gst_niimaqsrc_start (GstBaseSrc * bsrc);
static gboolean gst_niimaqsrc_stop (GstBaseSrc * bsrc);
static gboolean gst_niimaqsrc_query (GstBaseSrc * bsrc, GstQuery * query);
static GstCaps *gst_niimaqsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter);
static gboolean gst_niimaqsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps);

/* GstPushSrc virtual methods */
static GstFlowReturn gst_niimaqsrc_create (GstPushSrc * psrc,
    GstBuffer ** buffer);

/* GstNiImaq methods */
static GstCaps *gst_niimaqsrc_get_cam_caps (GstNiImaqSrc * src);
static gboolean gst_niimaqsrc_close_interface (GstNiImaqSrc * src);

#define gst_niimaqsrc_report_imaq_error(code)                    \
{                                                                \
  static char imaq_error_string[256];                            \
  if (code) {                                                    \
    imgShowError (code, imaq_error_string);                      \
    GST_ERROR_OBJECT (src, "IMAQ error: %s", imaq_error_string); \
  }                                                  \
}

uInt32
gst_niimaqsrc_aq_in_progress_callback (SESSION_ID sid, IMG_ERR err,
    IMG_SIGNAL_TYPE signal_type, uInt32 signal_identifier, void *userdata)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (userdata);
  if (src->session_started)
    GST_ERROR_OBJECT (src, "Session already started");
  src->session_started = TRUE;
  return 0;                     /* don't re-arm */
}

uInt32
gst_niimaqsrc_aq_done_callback (SESSION_ID sid, IMG_ERR err,
    IMG_SIGNAL_TYPE signal_type, uInt32 signal_identifier, void *userdata)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (userdata);
  if (!src->session_started)
    GST_ERROR_OBJECT (src, "Session not started");
  src->session_started = FALSE;
  return 0;                     /* don't re-arm */
}

typedef struct _GstNiImaqSrcTimeEntry GstNiImaqSrcTimeEntry;
struct _GstNiImaqSrcTimeEntry
{
  guint64 frame_index;
  GstClockTime clock_time;
};

/* This will be called "at the start of acquisition into each image buffer."
 * If acquisition blocks because we don't copy buffers fast enough, the number
 * of times this function is called will be less than the IMAQ cumulative
 * buffer count. */
uInt32
gst_niimaqsrc_frame_start_callback (SESSION_ID sid, IMG_ERR err,
    IMG_SIGNAL_TYPE signal_type, uInt32 signal_identifier, void *userdata)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (userdata);
  static guint32 index = 0;
  GstNiImaqSrcTimeEntry *time_entry;

  time_entry = g_new (GstNiImaqSrcTimeEntry, 1);

  /* get clock time */
  time_entry->clock_time =
      gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src)));
  time_entry->frame_index = index;

  g_async_queue_push (src->time_queue, time_entry);

  index++;

  /* return 1 to rearm the callback */
  return 1;
}

/* TODO: reimplement this when device discovery is added, see #678402 */
#if 0
/**
* gst_niimaqsrc_class_probe_interfaces:
* @klass: #GstNiImaqClass
* @check: whether to enumerate interfaces
*
* Probes NI-IMAQ driver for available interfaces
*
* Returns: TRUE always
*/
static gboolean
gst_niimaqsrc_class_probe_interfaces (GstNiImaqSrcClass * klass, gboolean check)
{
  if (!check) {
    guint32 n;
    gchar name[256];

    /* clear interface list */
    while (interfaces) {
      gchar *iface = interfaces->data;
      interfaces = g_list_remove (interfaces, iface);
      g_free (iface);
    }

    GST_LOG_OBJECT (klass, "About to probe for IMAQ interfaces");

    /* enumerate interfaces, limiting ourselves to the first 64 */
    for (n = 0; n < 64; n++) {
      guint32 iid;
      guint32 nports;
      guint32 port;
      gchar *iname;
      uInt32 rval;

      /* get interface names until there are no more */
      if (rval = imgInterfaceQueryNames (n, name) != 0) {
        gst_niimaqsrc_report_imaq_error (rval);
        break;
      }

      /* ignore NICFGen */
      if (g_strcmp0 (name, "NICFGen.iid") == 0)
        continue;

      /* try and open the interface */
      if (rval = imgInterfaceOpen (name, &iid) != 0) {
        gst_niimaqsrc_report_imaq_error (rval);
        continue;
      }

      /* find how many ports the interface provides */
      rval = imgGetAttribute (iid, IMG_ATTR_NUM_PORTS, &nports);
      gst_niimaqsrc_report_imaq_error (rval);
      rval = imgClose (iid, TRUE);
      gst_niimaqsrc_report_imaq_error (rval);

      /* iterate over all the available ports */
      for (port = 0; port < nports; port++) {
        /* if the there are multiple ports append the port number */
        if (nports > 1)
          iname = g_strdup_printf ("%s::%d", name, port);
        else
          iname = g_strdup (name);

        /* TODO: should check to see if a camera is actually attached */
        interfaces = g_list_append (interfaces, iname);

        GST_DEBUG_OBJECT (klass, "Adding interface '%s' to list", iname);
      }
    }

    init = TRUE;
  }

  klass->interfaces = interfaces;

  return init;
}
#endif

/**
* gst_niimaqsrc_class_init:
* klass: #GstNiImaqClass to initialize
*
* Initialize #GstNiImaqClass, which occurs only once no matter how many
* instances of the class there are
*/
static void
gst_niimaqsrc_class_init (GstNiImaqSrcClass * klass)
{
  /* get pointers to base classes */
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  /* install GObject vmethod implementations */
  gobject_class->dispose = gst_niimaqsrc_dispose;
  gobject_class->set_property = gst_niimaqsrc_set_property;
  gobject_class->get_property = gst_niimaqsrc_get_property;

  /* install GObject properties */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_DEVICE, g_param_spec_string ("device",
          "Device",
          "NI-IMAQ interface to open (e.g., img0::0)", DEFAULT_PROP_DEVICE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_RING_BUFFER_COUNT, g_param_spec_int ("ring-buffer-count",
          "Number of frames in the IMAQ ringbuffer",
          "The number of frames in the IMAQ ringbuffer", 1, G_MAXINT,
          DEFAULT_PROP_RING_BUFFER_COUNT,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_IS_SIGNED,
      g_param_spec_boolean ("is-signed", "Image is signed 16-bit",
          "Image is signed 16-bit, shift to unsigned 16-bit",
          DEFAULT_PROP_IS_SIGNED, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_TIMEOUT, g_param_spec_int ("timeout",
          "Timeout (ms)",
          "Timeout in ms (0 to use default)", 0, G_MAXINT,
          DEFAULT_PROP_TIMEOUT, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));

  gst_element_class_set_static_metadata (gstelement_class,
      "NI-IMAQ Video Source", "Source/Video",
      "National Instruments IMAQ based source, supports Camera Link and analog cameras",
      "Joshua M. Doe <oss@nvl.army.mil>");

  /* install GstBaseSrc vmethod implementations */
  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_niimaqsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_niimaqsrc_stop);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_niimaqsrc_query);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_niimaqsrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_niimaqsrc_set_caps);

  /* install GstPushSrc vmethod implementations */
  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_niimaqsrc_create);
}

static void
gst_niimaqsrc_init (GstNiImaqSrc * src)
{
  GstPad *srcpad = GST_BASE_SRC_PAD (src);

  GST_DEBUG_OBJECT (src, "init");

  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  src->time_queue = g_async_queue_new ();

  /* initialize properties */
  src->bufsize = DEFAULT_PROP_RING_BUFFER_COUNT;
  src->interface_name = g_strdup (DEFAULT_PROP_DEVICE);
  src->is_signed = DEFAULT_PROP_IS_SIGNED;
  src->timeout = DEFAULT_PROP_TIMEOUT;
}

/**
* gst_niimaqsrc_dispose:
* object: #GObject to dispose
*
* Disposes of the #GObject as part of object destruction
*/
static void
gst_niimaqsrc_dispose (GObject * object)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (object);

  GST_DEBUG_OBJECT (src, "dispose");

  gst_niimaqsrc_close_interface (src);

  /* free memory allocated */
  g_free (src->interface_name);
  src->interface_name = NULL;

  g_async_queue_unref (src->time_queue);

  /* chain dispose fuction of parent class */
  G_OBJECT_CLASS (gst_niimaqsrc_parent_class)->dispose (object);
}

static void
gst_niimaqsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      if (src->interface_name)
        g_free (src->interface_name);
      src->interface_name = g_strdup (g_value_get_string (value));
      break;
    case PROP_RING_BUFFER_COUNT:
      src->bufsize = g_value_get_int (value);
      break;
    case PROP_IS_SIGNED:
      src->is_signed = g_value_get_boolean (value);
      break;
    case PROP_TIMEOUT:
      src->timeout = g_value_get_int (value);
      break;
    default:
      break;
  }
}

static void
gst_niimaqsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, src->interface_name);
      break;
    case PROP_RING_BUFFER_COUNT:
      g_value_set_int (value, src->bufsize);
      break;
    case PROP_IS_SIGNED:
      g_value_set_boolean (value, src->is_signed);
      break;
    case PROP_TIMEOUT:
      g_value_set_int (value, src->timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

gboolean
gst_niimaqsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (bsrc);
  gboolean res = TRUE;
  int depth, ncomps;
  GstVideoInfo vinfo;

  res = gst_video_info_from_caps (&vinfo, caps);
  if (!res) {
    GST_WARNING_OBJECT (src, "Unable to parse video info from caps");
    return res;
  }
  src->format = GST_VIDEO_INFO_FORMAT (&vinfo);
  src->width = GST_VIDEO_INFO_WIDTH (&vinfo);
  src->height = GST_VIDEO_INFO_HEIGHT (&vinfo);

  /* this will handle byte alignment (i.e. row multiple of 4 bytes) */
  src->framesize = GST_VIDEO_INFO_SIZE (&vinfo);

  gst_base_src_set_blocksize (bsrc, src->framesize);

  ncomps = GST_VIDEO_INFO_N_COMPONENTS (&vinfo);
  depth = GST_VIDEO_INFO_COMP_DEPTH (&vinfo, 0);

  /* use this so NI can give us proper byte alignment */
  src->rowpixels =
      GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0) / (ncomps * depth / 8);

  GST_LOG_OBJECT (src, "Caps set, framesize=%d, rowpixels=%d",
      src->framesize, src->rowpixels);

  return res;
}

static void
gst_niimaqsrc_reset (GstNiImaqSrc * src)
{
  GST_LOG_OBJECT (src, "Resetting instance");

  /* initialize member variables */
  src->cumbufnum = 0;
  src->n_dropped_frames = 0;
  src->sid = 0;
  src->iid = 0;
  src->session_started = FALSE;
  src->format = GST_VIDEO_FORMAT_UNKNOWN;
  src->width = 0;
  src->height = 0;
  src->rowpixels = 0;

  g_free (src->buflist);
  src->buflist = NULL;

  if (src->time_queue) {
    g_async_queue_unref (src->time_queue);
  }
  src->time_queue = g_async_queue_new ();
}

static gboolean
gst_niimaqsrc_start_acquisition (GstNiImaqSrc * src)
{
  int i;
  gint32 rval;

  g_assert (!src->session_started);

  GST_DEBUG_OBJECT (src, "Starting acquisition");

  /* try to open the camera five times */
  for (i = 0; i < 5; i++) {
    rval = imgSessionStartAcquisition (src->sid);
    if (rval == IMG_ERR_GOOD) {
      return TRUE;
    } else {
      gst_niimaqsrc_report_imaq_error (rval);
      GST_LOG_OBJECT (src, "camera is still off , wait 50ms and retry");
      g_usleep (50000);
    }
  }

  /* we tried five times and failed, so we error */
  gst_niimaqsrc_close_interface (src);

  return FALSE;
}

static GstFlowReturn
gst_niimaqsrc_create (GstPushSrc * psrc, GstBuffer ** buffer)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (psrc);
  GstFlowReturn ret = GST_FLOW_OK;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  uInt32 copied_number;
  uInt32 copied_index;
  Int32 rval;
  uInt32 dropped;
  GstMapInfo minfo;

  GST_LOG_OBJECT (src, "create");

  /* start the IMAQ acquisition session if we haven't done so yet */
  if (!src->session_started) {
    if (!gst_niimaqsrc_start_acquisition (src)) {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Unable to start acquisition."), (NULL));
      return GST_FLOW_ERROR;
    }
  }

  GST_LOG_OBJECT (src, "Allocating memory for IMAQ buffer #%d, size %d",
      src->cumbufnum, src->framesize);
  ret =
      GST_BASE_SRC_CLASS (gst_niimaqsrc_parent_class)->alloc (GST_BASE_SRC
      (src), 0, src->framesize, buffer);
  if (ret != GST_FLOW_OK) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to allocate buffer"),
        ("Failed to get downstream pad to allocate buffer"));
    goto error;
  }

  gst_buffer_map (*buffer, &minfo, GST_MAP_WRITE);
  rval = imgSessionCopyAreaByNumber (src->sid, src->cumbufnum, 0, 0,
      src->height, src->width, minfo.data, src->rowpixels,
      IMG_OVERWRITE_GET_OLDEST, &copied_number, &copied_index);
  gst_buffer_unmap (*buffer, &minfo);
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("failed to copy buffer %d", src->cumbufnum),
        ("failed to copy buffer %d", src->cumbufnum));
    goto error;
  }

  while (timestamp == GST_CLOCK_TIME_NONE) {
    /* wait 100 ms, shouldn't be needed if callback is working as expected */
    GstNiImaqSrcTimeEntry *entry =
        (GstNiImaqSrcTimeEntry *) g_async_queue_timeout_pop (src->time_queue,
        100000);
    if (entry == NULL) {
      GST_WARNING_OBJECT (src, "No timestamps received, callback failed?");
      break;
    }

    if (entry->frame_index < copied_number) {
      GST_DEBUG_OBJECT (src,
          "Got clocktime for frame %d while handling frame %d, frames dropped?",
          entry->frame_index, copied_number);
      g_free (entry);
      continue;
    } else if (entry->frame_index > copied_number) {
      GST_DEBUG_OBJECT (src,
          "Failed to get clocktime for frame %d, got one for frame %d instead",
          copied_number, entry->frame_index);
      g_free (entry);
      break;
    }
    timestamp = entry->clock_time;
    g_free (entry);
  }

  /* TODO: do this above to reduce copying overhead */
  if (src->is_signed) {
    gint16 *srcp;
    guint16 *dstp;
    guint i;
    gst_buffer_map (*buffer, &minfo, GST_MAP_READWRITE);
    srcp = minfo.data;
    dstp = minfo.data;

    GST_DEBUG_OBJECT (src, "Shifting signed to unsigned");

    /* TODO: make this faster */
    for (i = 0; i < minfo.size / 2; i++)
      *dstp++ = *srcp++ + 32768;

    gst_buffer_unmap (*buffer, &minfo);
  }

  GST_BUFFER_OFFSET (*buffer) = copied_number;
  GST_BUFFER_OFFSET_END (*buffer) = copied_number + 1;
  GST_BUFFER_TIMESTAMP (*buffer) =
      timestamp - gst_element_get_base_time (GST_ELEMENT (src));

  dropped = copied_number - src->cumbufnum;
  if (dropped > 0) {
    src->n_dropped_frames += dropped;
    GST_WARNING_OBJECT (src,
        "Asked to copy buffer #%d but was given #%d; just dropped %d frames (%d total)",
        src->cumbufnum, copied_number, dropped, src->n_dropped_frames);
  }

  /* set cumulative buffer number to get next frame */
  src->cumbufnum = copied_number + 1;

  return ret;

error:
  {
    /* make sure we return an error */
    if (ret == GST_FLOW_OK)
      ret = GST_FLOW_ERROR;
    return ret;
  }
}

/**
* gst_niimaqsrc_get_cam_caps:
* src: #GstNiImaq instance
*
* Get caps of camera attached to open IMAQ interface
*
* Returns: the #GstCaps of the src pad. Unref the caps when you no longer need it.
*/
GstCaps *
gst_niimaqsrc_get_cam_caps (GstNiImaqSrc * src)
{
  GstCaps *gcaps = NULL;
  Int32 rval;
  uInt32 val;
  gint depth, bpp;
  GstVideoInfo vinfo;

  if (!src->iid) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Camera interface not open"), ("Camera interface not open"));
    goto error;
  }

  gst_video_info_init (&vinfo);

  GST_LOG_OBJECT (src, "Retrieving attributes from IMAQ interface");
  rval = imgGetAttribute (src->iid, IMG_ATTR_BITSPERPIXEL, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  bpp = val;
  rval &= imgGetAttribute (src->iid, IMG_ATTR_BYTESPERPIXEL, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  depth = val * 8;
  rval &= imgGetAttribute (src->iid, IMG_ATTR_ROI_WIDTH, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  vinfo.width = val;
  rval &= imgGetAttribute (src->iid, IMG_ATTR_ROI_HEIGHT, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  vinfo.height = val;

  if (rval) {
    GST_ELEMENT_ERROR (src, STREAM, FAILED,
        ("attempt to read attributes failed"),
        ("attempt to read attributes failed"));
    goto error;
  }

  if (depth == 8)
    vinfo.finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_GRAY8);
  else if (depth == 16)
    vinfo.finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_GRAY16_LE);
  else if (depth == 32)
    vinfo.finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_BGRA);
  else {
    GST_ERROR_OBJECT (src, "Depth %d (%d-bit) not supported yet", depth, bpp);
    goto error;
  }

  vinfo.fps_n = 30;
  vinfo.fps_d = 1;
  /* hard code framerate and par as IMAQ doesn't tell us anything about it */
  gcaps = gst_video_info_to_caps (&vinfo);
  GST_LOG_OBJECT (src, "the camera caps are %" GST_PTR_FORMAT, gcaps);

  return gcaps;

error:

  if (gcaps) {
    gst_caps_unref (gcaps);
  }

  return NULL;
}

/**
* gst_niimaqsrc_start:
* src: #GstBaseSrc instance
*
* Open necessary resources
*
* Returns: TRUE on success
*/
static gboolean
gst_niimaqsrc_start (GstBaseSrc * bsrc)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (bsrc);
  Int32 rval;
  gint i;
  uInt32 timeout;

  GST_DEBUG_OBJECT (src, "start");

  gst_niimaqsrc_reset (src);

  GST_LOG_OBJECT (src, "Opening IMAQ interface: %s", src->interface_name);

  /* open IMAQ interface */
  rval = imgInterfaceOpen (src->interface_name, &(src->iid));
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to open IMAQ interface"),
        ("Failed to open camera interface %s", src->interface_name));
    goto error;
  }

  GST_LOG_OBJECT (src, "Opening IMAQ session: %s", src->interface_name);

  /* open IMAQ session */
  rval = imgSessionOpen (src->iid, &(src->sid));
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to open IMAQ session"), ("Failed to open IMAQ session %d",
            src->sid));
    goto error;
  }

  /* Allow use of 1428 and other 32-bit DMA cards on 64-bit systems with
     greater than 3GB of memory. */
  niimaquDisable32bitPhysMemLimitEnforcement (src->sid);

  GST_LOG_OBJECT (src, "Creating ring with %d buffers", src->bufsize);

  /* create array of pointers to give to IMAQ for creating internal buffers */
  src->buflist = g_new (guint32 *, src->bufsize);
  for (i = 0; i < src->bufsize; i++) {
    src->buflist[i] = 0;
  }
  /* CAUTION: if this is ever changed to manually allocate memory, we must
     be careful about allocating 64-bit addresses, as some IMAQ cards don't
     support this, and can give a runtime error. See above call to
     niimaquDisable32bitPhysMemLimitEnforcement */
  rval =
      imgRingSetup (src->sid, src->bufsize, (void **) (src->buflist), 0, FALSE);
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to create ring buffer"),
        ("Failed to create ring buffer with %d buffers", src->bufsize));
    goto error;
  }

  GST_LOG_OBJECT (src, "Registering callback functions");
  rval = imgSessionWaitSignalAsync2 (src->sid, IMG_SIGNAL_STATUS,
      IMG_FRAME_START, IMG_SIGNAL_STATE_RISING,
      gst_niimaqsrc_frame_start_callback, src);
  rval |= imgSessionWaitSignalAsync2 (src->sid, IMG_SIGNAL_STATUS,
      IMG_AQ_IN_PROGRESS, IMG_SIGNAL_STATE_RISING,
      gst_niimaqsrc_aq_in_progress_callback, src);
  rval |= imgSessionWaitSignalAsync2 (src->sid, IMG_SIGNAL_STATUS,
      IMG_AQ_DONE, IMG_SIGNAL_STATE_RISING,
      gst_niimaqsrc_aq_done_callback, src);
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to register callback(s)"), ("Failed to register callback(s)"));
    goto error;
  }

  imgGetAttribute (src->iid, IMG_ATTR_FRAMEWAIT_MSEC, &timeout);
  if (src->timeout != 0) {
    rval = imgSetAttribute2 (src->sid, IMG_ATTR_FRAMEWAIT_MSEC, src->timeout);
    if (rval) {
      gst_niimaqsrc_report_imaq_error (rval);
      GST_WARNING_OBJECT (src, "Failed to change timeout from %d to %d msecs",
          timeout, src->timeout);
    } else {
      GST_DEBUG_OBJECT (src, "Changed timeout from %d to %d msecs", timeout,
          src->timeout);
    }
  } else {
    src->timeout = timeout;
    GST_DEBUG_OBJECT (src, "Current timeout is %d msecs", timeout);
  }

  return TRUE;

error:
  gst_niimaqsrc_close_interface (src);

  return FALSE;;

}

/**
* gst_niimaqsrc_stop:
* src: #GstBaseSrc instance
*
* Close resources opened by gst_niimaqsrc_start
*
* Returns: TRUE on success
*/
static gboolean
gst_niimaqsrc_stop (GstBaseSrc * bsrc)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (bsrc);
  Int32 rval;
  gboolean result = TRUE;

  GST_DEBUG_OBJECT (src, "stop");

  /* stop IMAQ session */
  if (src->session_started) {
    uInt32 last_buf_num;
    rval = imgSessionAbort (src->sid, &last_buf_num);
    GST_DEBUG_OBJECT (src, "Last good buffer number is %d", last_buf_num);
    if (rval != IMG_ERR_GOOD) {
      gst_niimaqsrc_report_imaq_error (rval);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Unable to stop acquisition"), ("Unable to stop acquisition"));
      result = FALSE;
    }
    src->session_started = FALSE;
    GST_DEBUG_OBJECT (src, "Acquisition stopped");
  }

  result &= gst_niimaqsrc_close_interface (src);

  gst_niimaqsrc_reset (src);

  return result;
}

static gboolean
gst_niimaqsrc_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (bsrc);
  gboolean res;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      if (!src->session_started) {
        GST_WARNING_OBJECT (src, "Can't give latency since device isn't open!");
        res = FALSE;
      } else {
        GstClockTime min_latency, max_latency;
        /* TODO: this is a ballpark figure, estimate from FVAL times */
        min_latency = 33 * GST_MSECOND;
        max_latency = 33 * GST_MSECOND * src->bufsize;

        GST_LOG_OBJECT (src,
            "report latency min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
            GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

        gst_query_set_latency (query, TRUE, min_latency, max_latency);

        res = TRUE;
      }
    }
    default:
      res =
          GST_BASE_SRC_CLASS (gst_niimaqsrc_parent_class)->query (bsrc, query);
      break;
  }

  return res;
}

static GstCaps *
gst_niimaqsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstNiImaqSrc *src = GST_NIIMAQSRC (bsrc);
  GstCaps *caps;

  if (!src->sid)
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  else
    caps = gst_niimaqsrc_get_cam_caps (src);

  if (filter) {
    GstCaps *tmp = gst_caps_intersect (caps, filter);
    gst_caps_unref (caps);
    caps = tmp;
  }

  return caps;
}

/**
* gst_niimaqsrc_close_interface:
* src: #GstNiImaqSrc instance
*
* Close IMAQ session and interface
*
*/
static gboolean
gst_niimaqsrc_close_interface (GstNiImaqSrc * src)
{
  Int32 rval;
  gboolean result = TRUE;

  /* close IMAQ session and interface */
  if (src->sid) {
    rval = imgClose (src->sid, TRUE);
    if (rval != IMG_ERR_GOOD) {
      gst_niimaqsrc_report_imaq_error (rval);
      result = FALSE;
    } else
      GST_LOG_OBJECT (src, "IMAQ session closed");
    src->sid = 0;
  }
  if (src->iid) {
    rval = imgClose (src->iid, TRUE);
    if (rval != IMG_ERR_GOOD) {
      gst_niimaqsrc_report_imaq_error (rval);
      result = FALSE;
    } else {
      GST_LOG_OBJECT (src, "IMAQ interface closed");
    }
    src->iid = 0;
  }

  return result;
}

/**
* plugin_init:
* plugin: #GstPlugin
*
* Initialize plugin by registering elements
*
* Returns: TRUE on success
*/
static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (niimaqsrc_debug, "niimaqsrc", 0,
      "NI-IMAQ interface");

  /* we only have one element in this plugin */
  return gst_element_register (plugin, "niimaqsrc", GST_RANK_NONE,
      GST_TYPE_NIIMAQSRC);

}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, niimaq,
    "NI-IMAQ source element", plugin_init, PACKAGE_VERSION, GST_LICENSE, PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
