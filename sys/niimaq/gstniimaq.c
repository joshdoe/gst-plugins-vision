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
  PROP_AVOID_COPY
};

#define DEFAULT_PROP_DEVICE "img0"
#define DEFAULT_PROP_RING_BUFFER_COUNT  2
#define DEFAULT_PROP_AVOID_COPY FALSE

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
static gboolean gst_niimaqsrc_start (GstBaseSrc * src);
static gboolean gst_niimaqsrc_stop (GstBaseSrc * src);
static gboolean gst_niimaqsrc_query (GstBaseSrc * src, GstQuery * query);
static GstCaps *gst_niimaqsrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_niimaqsrc_set_caps (GstBaseSrc * src, GstCaps * caps);

/* GstPushSrc virtual methods */
static GstFlowReturn gst_niimaqsrc_create (GstPushSrc * psrc,
    GstBuffer ** buffer);

/* GstNiImaq methods */
static GstCaps *gst_niimaqsrc_get_cam_caps (GstNiImaqSrc * src);
static gboolean gst_niimaqsrc_close_interface (GstNiImaqSrc * niimaqsrc);

uInt32
gst_niimaqsrc_report_imaq_error (uInt32 code)
{
  static char imaq_error_string[256];
  if (code) {
    imgShowError (code, imaq_error_string);
    GST_ERROR ("IMAQ error: %s", imaq_error_string);
  }
  return code;
}

uInt32
gst_niimaqsrc_aq_in_progress_callback (SESSION_ID sid, IMG_ERR err,
    IMG_SIGNAL_TYPE signal_type, uInt32 signal_identifier, void *userdata)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (userdata);
  if (niimaqsrc->session_started)
    GST_ERROR_OBJECT (niimaqsrc, "Session already started");
  niimaqsrc->session_started = TRUE;
  return 0;                     /* don't re-arm */
}

uInt32
gst_niimaqsrc_aq_done_callback (SESSION_ID sid, IMG_ERR err,
    IMG_SIGNAL_TYPE signal_type, uInt32 signal_identifier, void *userdata)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (userdata);
  if (!niimaqsrc->session_started)
    GST_ERROR_OBJECT (niimaqsrc, "Session not started");
  niimaqsrc->session_started = FALSE;
  return 0;                     /* don't re-arm */
}

/* This will be called "at the start of acquisition into each image buffer."
 * If acquisition blocks because we don't copy buffers fast enough, the number
 * of times this function is called will be less than the IMAQ cumulative
 * buffer count. */
uInt32
gst_niimaqsrc_frame_start_callback (SESSION_ID sid, IMG_ERR err,
    IMG_SIGNAL_TYPE signal_type, uInt32 signal_identifier, void *userdata)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (userdata);
  GstClockTime abstime;
  static guint32 index = 0;

  g_mutex_lock (&niimaqsrc->mutex);

  /* time hasn't been read yet, this frame will be dropped */
  if (niimaqsrc->times[index] != GST_CLOCK_TIME_NONE) {
    g_mutex_unlock (&niimaqsrc->mutex);
    return 1;
  }

  /* get clock time */
  abstime = gst_clock_get_time (GST_ELEMENT_CLOCK (niimaqsrc));
  niimaqsrc->times[index] = abstime;

  if (G_UNLIKELY (niimaqsrc->start_time == NULL))
    niimaqsrc->start_time = gst_date_time_new_now_utc ();

  /* first frame, use as element base time */
  if (niimaqsrc->base_time == GST_CLOCK_TIME_NONE)
    niimaqsrc->base_time = abstime;

  index = (index + 1) % niimaqsrc->bufsize;

  g_mutex_unlock (&niimaqsrc->mutex);

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
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_AVOID_COPY,
      g_param_spec_boolean ("avoid-copy", "Avoid copying",
          "Whether to avoid copying (do not use with queues)",
          DEFAULT_PROP_AVOID_COPY, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

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
gst_niimaqsrc_init (GstNiImaqSrc * niimaqsrc)
{
  GstPad *srcpad = GST_BASE_SRC_PAD (niimaqsrc);

  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (niimaqsrc), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (niimaqsrc), GST_FORMAT_TIME);

  g_mutex_init (&niimaqsrc->mutex);

  /* initialize properties */
  niimaqsrc->bufsize = DEFAULT_PROP_RING_BUFFER_COUNT;
  niimaqsrc->interface_name = g_strdup (DEFAULT_PROP_DEVICE);
  niimaqsrc->avoid_copy = DEFAULT_PROP_AVOID_COPY;
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
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (object);

  gst_niimaqsrc_close_interface (niimaqsrc);

  /* free memory allocated */
  g_free (niimaqsrc->interface_name);
  niimaqsrc->interface_name = NULL;

  /* unref objects */
  if (niimaqsrc->start_time) {
    gst_date_time_unref (niimaqsrc->start_time);
    niimaqsrc->start_time = NULL;
  }

  /* chain dispose fuction of parent class */
  G_OBJECT_CLASS (gst_niimaqsrc_parent_class)->dispose (object);
}

static void
gst_niimaqsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      if (niimaqsrc->interface_name)
        g_free (niimaqsrc->interface_name);
      niimaqsrc->interface_name = g_strdup (g_value_get_string (value));
      break;
    case PROP_RING_BUFFER_COUNT:
      niimaqsrc->bufsize = g_value_get_int (value);
      break;
    case PROP_AVOID_COPY:
      niimaqsrc->avoid_copy = g_value_get_boolean (value);
      break;
    default:
      break;
  }
}

static void
gst_niimaqsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, niimaqsrc->interface_name);
      break;
    case PROP_RING_BUFFER_COUNT:
      g_value_set_int (value, niimaqsrc->bufsize);
      break;
    case PROP_AVOID_COPY:
      g_value_set_boolean (value, niimaqsrc->avoid_copy);
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
  int depth;
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

  depth = GST_VIDEO_INFO_COMP_DEPTH (&vinfo, 0);

  /* use this so NI can give us proper byte alignment */
  src->rowpixels = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0) / (depth / 8);

  GST_LOG_OBJECT (src, "Caps set, framesize=%d, rowpixels=%d",
      src->framesize, src->rowpixels);

  return res;
}

static void
gst_niimaqsrc_reset (GstNiImaqSrc * niimaqsrc)
{
  GST_LOG_OBJECT (niimaqsrc, "Resetting instance");

  /* initialize member variables */
  niimaqsrc->n_frames = 0;
  niimaqsrc->cumbufnum = 0;
  niimaqsrc->n_dropped_frames = 0;
  niimaqsrc->sid = 0;
  niimaqsrc->iid = 0;
  niimaqsrc->session_started = FALSE;
  niimaqsrc->format = GST_VIDEO_FORMAT_UNKNOWN;
  niimaqsrc->width = 0;
  niimaqsrc->height = 0;
  niimaqsrc->rowpixels = 0;
  niimaqsrc->start_time = NULL;
  niimaqsrc->start_time_sent = FALSE;
  niimaqsrc->base_time = GST_CLOCK_TIME_NONE;

  g_free (niimaqsrc->buflist);
  niimaqsrc->buflist = NULL;

  g_free (niimaqsrc->times);
  niimaqsrc->times = NULL;
}

static gboolean
gst_niimaqsrc_start_acquisition (GstNiImaqSrc * niimaqsrc)
{
  int i;
  gint32 rval;

  g_assert (!niimaqsrc->session_started);

  GST_DEBUG_OBJECT (niimaqsrc, "Starting acquisition");

  /* try to open the camera five times */
  for (i = 0; i < 5; i++) {
    rval = imgSessionStartAcquisition (niimaqsrc->sid);
    if (rval == IMG_ERR_GOOD) {
      return TRUE;
    } else {
      gst_niimaqsrc_report_imaq_error (rval);
      GST_LOG_OBJECT (niimaqsrc, "camera is still off , wait 50ms and retry");
      g_usleep (50000);
    }
  }

  /* we tried five times and failed, so we error */
  gst_niimaqsrc_close_interface (niimaqsrc);

  return FALSE;
}

static GstClockTime
gst_niimaqsrc_get_timestamp_from_buffer_number (GstNiImaqSrc * niimaqsrc,
    guint32 buffer_number)
{
  GstClockTime abstime;

  abstime = niimaqsrc->times[(buffer_number) % niimaqsrc->bufsize];
  niimaqsrc->times[(buffer_number) % niimaqsrc->bufsize] = GST_CLOCK_TIME_NONE;

  if (abstime == GST_CLOCK_TIME_NONE)
    GST_WARNING_OBJECT (niimaqsrc,
        "No valid time found for buffer %d, callback failed?", buffer_number);

  return abstime;
}

static void
gst_niimaqsrc_release_buffer (gpointer data)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (data);
  imgSessionReleaseBuffer (niimaqsrc->sid);
}

static GstFlowReturn
gst_niimaqsrc_create (GstPushSrc * psrc, GstBuffer ** buffer)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (psrc);
  GstFlowReturn ret = GST_FLOW_OK;
  GstClockTime timestamp;
  GstClockTime duration;
  uInt32 copied_number;
  uInt32 copied_index;
  Int32 rval;
  uInt32 dropped;
  gboolean no_copy;
  GstMapInfo minfo;

  /* we can only do a no-copy if strides are property byte aligned */
  no_copy = niimaqsrc->avoid_copy && niimaqsrc->width == niimaqsrc->rowpixels;

  /* start the IMAQ acquisition session if we haven't done so yet */
  if (!niimaqsrc->session_started) {
    if (!gst_niimaqsrc_start_acquisition (niimaqsrc)) {
      GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
          ("Unable to start acquisition."), (NULL));
      return GST_FLOW_ERROR;
    }
  }

  if (no_copy) {
    GST_LOG_OBJECT (niimaqsrc,
        "Sending IMAQ buffer #%d along without copying", niimaqsrc->cumbufnum);
    *buffer = gst_buffer_new ();
    if (G_UNLIKELY (*buffer == NULL))
      goto error;
  } else {
    GST_LOG_OBJECT (niimaqsrc, "Copying IMAQ buffer #%d, size %d",
        niimaqsrc->cumbufnum, niimaqsrc->framesize);
    ret =
        GST_BASE_SRC_CLASS (gst_niimaqsrc_parent_class)->alloc (GST_BASE_SRC
        (niimaqsrc), 0, niimaqsrc->framesize, buffer);
    if (ret != GST_FLOW_OK) {
      GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
          ("Failed to allocate buffer"),
          ("Failed to get downstream pad to allocate buffer"));
      goto error;
    }
  }

  //{
  // guint32 *data;
  // int i;
  //    rval = imgSessionExamineBuffer2 (niimaqsrc->sid, niimaqsrc->cumbufnum, &copied_number, &data);
  // for (i=0; i<niimaqsrc->bufsize;i++)
  //  if (data == niimaqsrc->buflist[i])
  //        break;
  // timestamp = niimaqsrc->times[i];
  // memcpy (GST_BUFFER_DATA (*buffer), data, niimaqsrc->framesize);
  // niimaqsrc->times[i] = GST_CLOCK_TIME_NONE;
  // imgSessionReleaseBuffer (niimaqsrc->sid); //TODO: mutex here?
  //}
  if (no_copy) {
    /* FIXME: with callback change, is this broken now? mutex... */
    gpointer data;
    rval =
        imgSessionExamineBuffer2 (niimaqsrc->sid, niimaqsrc->cumbufnum,
        &copied_number, &data);
    gst_buffer_append_memory (*buffer,
        gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY, data,
            niimaqsrc->framesize, 0, niimaqsrc->framesize, niimaqsrc,
            gst_niimaqsrc_release_buffer));
  } else if (niimaqsrc->width == niimaqsrc->rowpixels) {
    /* TODO: optionally use ExamineBuffer and byteswap in transfer (to offer BIG_ENDIAN) */
    gst_buffer_map (*buffer, &minfo, GST_MAP_WRITE);
    g_mutex_lock (&niimaqsrc->mutex);
    rval =
        imgSessionCopyBufferByNumber (niimaqsrc->sid, niimaqsrc->cumbufnum,
        minfo.data, IMG_OVERWRITE_GET_OLDEST, &copied_number, &copied_index);
    timestamp = niimaqsrc->times[copied_index];
    niimaqsrc->times[copied_index] = GST_CLOCK_TIME_NONE;
    g_mutex_unlock (&niimaqsrc->mutex);
    gst_buffer_unmap (*buffer, &minfo);
  } else {
    gst_buffer_map (*buffer, &minfo, GST_MAP_WRITE);
    g_mutex_lock (&niimaqsrc->mutex);
    rval =
        imgSessionCopyAreaByNumber (niimaqsrc->sid, niimaqsrc->cumbufnum, 0, 0,
        niimaqsrc->height, niimaqsrc->width, minfo.data, niimaqsrc->rowpixels,
        IMG_OVERWRITE_GET_OLDEST, &copied_number, &copied_index);
    timestamp = niimaqsrc->times[copied_index];
    niimaqsrc->times[copied_index] = GST_CLOCK_TIME_NONE;
    g_mutex_unlock (&niimaqsrc->mutex);
    gst_buffer_unmap (*buffer, &minfo);
  }

  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("failed to copy buffer %d", niimaqsrc->cumbufnum),
        ("failed to copy buffer %d", niimaqsrc->cumbufnum));
    goto error;
  }

  /* make guess of duration from timestamp and cumulative buffer number */
  if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
    duration = timestamp / (copied_number + 1);
  } else {
    duration = 33 * GST_MSECOND;
  }

  GST_BUFFER_OFFSET (*buffer) = copied_number;
  GST_BUFFER_OFFSET_END (*buffer) = copied_number + 1;
  GST_BUFFER_TIMESTAMP (*buffer) =
      timestamp - gst_element_get_base_time (GST_ELEMENT (niimaqsrc));
  GST_BUFFER_DURATION (*buffer) = duration;

  dropped = copied_number - niimaqsrc->cumbufnum;
  if (dropped > 0) {
    niimaqsrc->n_dropped_frames += dropped;
    GST_WARNING_OBJECT (niimaqsrc,
        "Asked to copy buffer #%d but was given #%d; just dropped %d frames (%d total)",
        niimaqsrc->cumbufnum, copied_number, dropped,
        niimaqsrc->n_dropped_frames);
  }

  /* set cumulative buffer number to get next frame */
  niimaqsrc->cumbufnum = copied_number + 1;
  niimaqsrc->n_frames++;

  if (G_UNLIKELY (niimaqsrc->start_time && !niimaqsrc->start_time_sent)) {
    GstTagList *tl =
        gst_tag_list_new (GST_TAG_DATE_TIME, niimaqsrc->start_time, NULL);
    GstEvent *e = gst_event_new_tag (tl);
    GST_DEBUG_OBJECT (niimaqsrc, "Sending start time event: %" GST_PTR_FORMAT,
        e);
    gst_pad_push_event (GST_BASE_SRC_PAD (niimaqsrc), e);
    niimaqsrc->start_time_sent = TRUE;
  }
  return ret;

error:
  {
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
gst_niimaqsrc_get_cam_caps (GstNiImaqSrc * niimaqsrc)
{
  GstCaps *gcaps = NULL;
  Int32 rval;
  uInt32 val;
  gint depth, bpp;
  GstVideoInfo vinfo;

  if (!niimaqsrc->iid) {
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("Camera interface not open"), ("Camera interface not open"));
    goto error;
  }

  gst_video_info_init (&vinfo);

  GST_LOG_OBJECT (niimaqsrc, "Retrieving attributes from IMAQ interface");
  rval = imgGetAttribute (niimaqsrc->iid, IMG_ATTR_BITSPERPIXEL, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  bpp = val;
  rval &= imgGetAttribute (niimaqsrc->iid, IMG_ATTR_BYTESPERPIXEL, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  depth = val * 8;
  rval &= imgGetAttribute (niimaqsrc->iid, IMG_ATTR_ROI_WIDTH, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  vinfo.width = val;
  rval &= imgGetAttribute (niimaqsrc->iid, IMG_ATTR_ROI_HEIGHT, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  vinfo.height = val;

  if (rval) {
    GST_ELEMENT_ERROR (niimaqsrc, STREAM, FAILED,
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
    GST_ERROR_OBJECT (niimaqsrc, "Depth %d (%d-bit) not supported yet", depth,
        bpp);
    goto error;
  }

  vinfo.fps_n = 30;
  vinfo.fps_d = 1;
  /* hard code framerate and par as IMAQ doesn't tell us anything about it */
  gcaps = gst_video_info_to_caps (&vinfo);
  GST_LOG_OBJECT (niimaqsrc, "the camera caps are %" GST_PTR_FORMAT, gcaps);

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
gst_niimaqsrc_start (GstBaseSrc * src)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (src);
  Int32 rval;
  gint i;

  gst_niimaqsrc_reset (niimaqsrc);

  GST_LOG_OBJECT (niimaqsrc, "Opening IMAQ interface: %s",
      niimaqsrc->interface_name);

  /* open IMAQ interface */
  rval = imgInterfaceOpen (niimaqsrc->interface_name, &(niimaqsrc->iid));
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("Failed to open IMAQ interface"),
        ("Failed to open camera interface %s", niimaqsrc->interface_name));
    goto error;
  }

  GST_LOG_OBJECT (niimaqsrc, "Opening IMAQ session: %s",
      niimaqsrc->interface_name);

  /* open IMAQ session */
  rval = imgSessionOpen (niimaqsrc->iid, &(niimaqsrc->sid));
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("Failed to open IMAQ session"), ("Failed to open IMAQ session %d",
            niimaqsrc->sid));
    goto error;
  }

  /* Allow use of 1428 and other 32-bit DMA cards on 64-bit systems with
     greater than 3GB of memory. */
  niimaquDisable32bitPhysMemLimitEnforcement (niimaqsrc->sid);

  GST_LOG_OBJECT (niimaqsrc, "Creating ring with %d buffers",
      niimaqsrc->bufsize);

  /* create array of pointers to give to IMAQ for creating internal buffers */
  niimaqsrc->buflist = g_new (guint32 *, niimaqsrc->bufsize);
  niimaqsrc->times = g_new (GstClockTime, niimaqsrc->bufsize);
  for (i = 0; i < niimaqsrc->bufsize; i++) {
    niimaqsrc->buflist[i] = 0;
    niimaqsrc->times[i] = GST_CLOCK_TIME_NONE;
  }
  /* CAUTION: if this is ever changed to manually allocate memory, we must
     be careful about allocating 64-bit addresses, as some IMAQ cards don't
     support this, and can give a runtime error. See above call to
     niimaquDisable32bitPhysMemLimitEnforcement */
  rval =
      imgRingSetup (niimaqsrc->sid, niimaqsrc->bufsize,
      (void **) (niimaqsrc->buflist), 0, FALSE);
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("Failed to create ring buffer"),
        ("Failed to create ring buffer with %d buffers", niimaqsrc->bufsize));
    goto error;
  }

  GST_LOG_OBJECT (niimaqsrc, "Registering callback functions");
  rval = imgSessionWaitSignalAsync2 (niimaqsrc->sid, IMG_SIGNAL_STATUS,
      IMG_FRAME_START, IMG_SIGNAL_STATE_RISING,
      gst_niimaqsrc_frame_start_callback, niimaqsrc);
  rval |= imgSessionWaitSignalAsync2 (niimaqsrc->sid, IMG_SIGNAL_STATUS,
      IMG_AQ_IN_PROGRESS, IMG_SIGNAL_STATE_RISING,
      gst_niimaqsrc_aq_in_progress_callback, niimaqsrc);
  rval |= imgSessionWaitSignalAsync2 (niimaqsrc->sid, IMG_SIGNAL_STATUS,
      IMG_AQ_DONE, IMG_SIGNAL_STATE_RISING,
      gst_niimaqsrc_aq_done_callback, niimaqsrc);
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("Failed to register callback(s)"), ("Failed to register callback(s)"));
    goto error;
  }

  return TRUE;

error:
  gst_niimaqsrc_close_interface (niimaqsrc);

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
gst_niimaqsrc_stop (GstBaseSrc * src)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (src);
  Int32 rval;
  gboolean result = TRUE;

  /* stop IMAQ session */
  if (niimaqsrc->session_started) {
    rval = imgSessionStopAcquisition (niimaqsrc->sid);
    if (rval != IMG_ERR_GOOD) {
      gst_niimaqsrc_report_imaq_error (rval);
      GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
          ("Unable to stop acquisition"), ("Unable to stop acquisition"));
      result = FALSE;
    }
    niimaqsrc->session_started = FALSE;
    GST_DEBUG_OBJECT (niimaqsrc, "Acquisition stopped");
  }

  result &= gst_niimaqsrc_close_interface (niimaqsrc);

  gst_niimaqsrc_reset (niimaqsrc);

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
* niimaqsrc: #GstNiImaqSrc instance
*
* Close IMAQ session and interface
*
*/
static gboolean
gst_niimaqsrc_close_interface (GstNiImaqSrc * niimaqsrc)
{
  Int32 rval;
  gboolean result = TRUE;

  /* close IMAQ session and interface */
  if (niimaqsrc->sid) {
    rval = imgClose (niimaqsrc->sid, TRUE);
    if (rval != IMG_ERR_GOOD) {
      gst_niimaqsrc_report_imaq_error (rval);
      result = FALSE;
    } else
      GST_LOG_OBJECT (niimaqsrc, "IMAQ session closed");
    niimaqsrc->sid = 0;
  }
  if (niimaqsrc->iid) {
    rval = imgClose (niimaqsrc->iid, TRUE);
    if (rval != IMG_ERR_GOOD) {
      gst_niimaqsrc_report_imaq_error (rval);
      result = FALSE;
    } else {
      GST_LOG_OBJECT (niimaqsrc, "IMAQ interface closed");
    }
    niimaqsrc->iid = 0;
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
    "NI-IMAQ source element", plugin_init, VERSION, GST_LICENSE, PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
