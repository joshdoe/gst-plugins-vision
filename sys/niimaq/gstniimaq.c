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

#include <gst/interfaces/propertyprobe.h>
#include <gst/video/video.h>

GST_DEBUG_CATEGORY (niimaqsrc_debug);
#define GST_CAT_DEFAULT niimaqsrc_debug

static GstElementDetails niimaqsrc_details =
GST_ELEMENT_DETAILS ("NI-IMAQ Video Source",
    "Source/Video",
    "National Instruments based source, supports Camera Link and analog cameras",
    "Joshua Doe <oss@nvl.army.mil>");

enum
{
  PROP_0,
  PROP_INTERFACE,
  PROP_BUFSIZE,
  PROP_AVOID_COPY
};

#define DEFAULT_PROP_INTERFACE "img0"
#define DEFAULT_PROP_BUFSIZE  10
#define DEFAULT_PROP_AVOID_COPY FALSE

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_GRAY8 ";"
        GST_VIDEO_CAPS_GRAY16 ("LITTLE_ENDIAN"))
    );

static void gst_niimaqsrc_init_interfaces (GType type);

GST_BOILERPLATE_FULL (GstNiImaqSrc, gst_niimaqsrc, GstPushSrc,
    GST_TYPE_PUSH_SRC, gst_niimaqsrc_init_interfaces);

/* GObject virtual methods */
static void gst_niimaqsrc_dispose (GObject * object);
static void gst_niimaqsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_niimaqsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* GstBaseSrc virtual methods */
static GstCaps *gst_niimaqsrc_get_caps (GstBaseSrc * bsrc);
static gboolean gst_niimaqsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_niimaqsrc_start (GstBaseSrc * src);
static gboolean gst_niimaqsrc_stop (GstBaseSrc * src);
static gboolean gst_niimaqsrc_query (GstBaseSrc * src, GstQuery * query);

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

  g_mutex_lock (niimaqsrc->mutex);

  /* time hasn't been read yet, this frame will be dropped */
  if (niimaqsrc->times[index] != GST_CLOCK_TIME_NONE) {
    g_mutex_unlock (niimaqsrc->mutex);
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

  g_mutex_unlock (niimaqsrc->mutex);

  /* return 1 to rearm the callback */
  return 1;
}

static void _____BEGIN_FUNCTIONS_____ ();

/**
* gst_niimaqsrc_probe_get_properties:
* @probe: #GstPropertyProbe
*
* Gets list of properties that can be probed
*
* Returns: #GList of properties that can be probed
*/
static const GList *
gst_niimaqsrc_probe_get_properties (GstPropertyProbe * probe)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (probe);
  static GList *list = NULL;

  if (!list) {
    list =
        g_list_append (NULL, g_object_class_find_property (klass, "interface"));
  }

  return list;
}

static gboolean init = FALSE;
static GList *interfaces = NULL;

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

/**
* gst_niimaqsrc_probe_probe_property:
* @probe: #GstPropertyProbe
* @prop_id: Property id
* @pspec: #GParamSpec
*
* GstPropertyProbe _probe_proprty vmethod implementation that probes a
*   property for possible values
*/
static void
gst_niimaqsrc_probe_probe_property (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstNiImaqSrcClass *klass = GST_NIIMAQSRC_GET_CLASS (probe);

  switch (prop_id) {
    case PROP_INTERFACE:
      gst_niimaqsrc_class_probe_interfaces (klass, FALSE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }
}

/**
* gst_niimaqsrc_probe_needs_probe:
* @probe: #GstPropertyProbe
* @prop_id: Property id
* @pspec: #GParamSpec
*
* GstPropertyProbe _needs_probe vmethod implementation that indicates if
*   a property needs to be updated
*
* Returns: TRUE if a property needs to be updated
*/
static gboolean
gst_niimaqsrc_probe_needs_probe (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstNiImaqSrcClass *klass = GST_NIIMAQSRC_GET_CLASS (probe);
  gboolean ret = FALSE;

  switch (prop_id) {
    case PROP_INTERFACE:
      ret = !gst_niimaqsrc_class_probe_interfaces (klass, TRUE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

  return ret;
}

/**
* gst_niimaqsrc_class_list_interfaces:
* @klass: #GstNiImaqClass
*
* Returns: #GValueArray of interface names
*/
static GValueArray *
gst_niimaqsrc_class_list_interfaces (GstNiImaqSrcClass * klass)
{
  GValueArray *array;
  GValue value = { 0 };
  GList *item;

  if (!klass->interfaces)
    return NULL;

  array = g_value_array_new (g_list_length (klass->interfaces));
  item = klass->interfaces;
  g_value_init (&value, G_TYPE_STRING);
  while (item) {
    gchar *iface = item->data;

    g_value_set_string (&value, iface);
    g_value_array_append (array, &value);

    item = item->next;
  }
  g_value_unset (&value);

  return array;
}

/**
* gst_niimaqsrc_probe_get_values:
* @probe: #GstPropertyProbe
* @prop_id: Property id
* @pspec: #GParamSpec
*
* GstPropertyProbe _get_values vmethod implementation that gets possible
*   values for a property
*
* Returns: #GValueArray containing possible values for requested property
*/
static GValueArray *
gst_niimaqsrc_probe_get_values (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstNiImaqSrcClass *klass = GST_NIIMAQSRC_GET_CLASS (probe);
  GValueArray *array = NULL;

  switch (prop_id) {
    case PROP_INTERFACE:
      array = gst_niimaqsrc_class_list_interfaces (klass);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

  return array;
}

/**
* gst_v4l_property_probe_interface_init:
* @iface: #GstPropertyProbeInterface
*
* Install property probe interfaces functions
*/
static void
gst_niimaqsrc_property_probe_interface_init (GstPropertyProbeInterface * iface)
{
  iface->get_properties = gst_niimaqsrc_probe_get_properties;
  iface->probe_property = gst_niimaqsrc_probe_probe_property;
  iface->needs_probe = gst_niimaqsrc_probe_needs_probe;
  iface->get_values = gst_niimaqsrc_probe_get_values;
}

/**
* gst_niimaqsrc_init_interfaces:
* @type: #GType
*
* Initialize all GStreamer interfaces
*/
static void
gst_niimaqsrc_init_interfaces (GType type)
{
  static const GInterfaceInfo niimaq_propertyprobe_info = {
    (GInterfaceInitFunc) gst_niimaqsrc_property_probe_interface_init,
    NULL,
    NULL,
  };

  g_type_add_interface_static (type,
      GST_TYPE_PROPERTY_PROBE, &niimaq_propertyprobe_info);
}

/**
* gst_niimaqsrc_base_init:
* g_class:
*
* Base GObject initialization
*/
static void
gst_niimaqsrc_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (element_class, &niimaqsrc_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
}

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
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;

  /* install GObject vmethod implementations */
  gobject_class->dispose = gst_niimaqsrc_dispose;
  gobject_class->set_property = gst_niimaqsrc_set_property;
  gobject_class->get_property = gst_niimaqsrc_get_property;

  /* install GObject properties */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_INTERFACE, g_param_spec_string ("interface",
          "Interface",
          "NI-IMAQ interface to open", DEFAULT_PROP_INTERFACE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BUFSIZE,
      g_param_spec_int ("buffer-size",
          "Number of frames in the IMAQ ringbuffer",
          "The number of frames in the IMAQ ringbuffer", 1, G_MAXINT,
          DEFAULT_PROP_BUFSIZE, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_AVOID_COPY,
      g_param_spec_boolean ("avoid-copy",
          "Avoid copying",
          "Whether to avoid copying (do not use with queues)",
          DEFAULT_PROP_AVOID_COPY, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  /* install GstBaseSrc vmethod implementations */
  gstbasesrc_class->get_caps = gst_niimaqsrc_get_caps;
  gstbasesrc_class->set_caps = gst_niimaqsrc_set_caps;
  gstbasesrc_class->start = gst_niimaqsrc_start;
  gstbasesrc_class->stop = gst_niimaqsrc_stop;
  gstbasesrc_class->query = gst_niimaqsrc_query;

  /* install GstPushSrc vmethod implementations */
  gstpushsrc_class->create = gst_niimaqsrc_create;
}

/**
* gst_niimaqsrc_init:
* src: the #GstNiImaq instance to initialize
* g_class: #GstNiImaqClass
*
* Initialize this instance of #GstNiImaq
*/
static void
gst_niimaqsrc_init (GstNiImaqSrc * niimaqsrc, GstNiImaqSrcClass * g_class)
{
  GstPad *srcpad = GST_BASE_SRC_PAD (niimaqsrc);

  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (niimaqsrc), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (niimaqsrc), GST_FORMAT_TIME);

  niimaqsrc->mutex = g_mutex_new ();

  /* initialize properties */
  niimaqsrc->bufsize = DEFAULT_PROP_BUFSIZE;
  niimaqsrc->interface_name = g_strdup (DEFAULT_PROP_INTERFACE);
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
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_niimaqsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (object);

  switch (prop_id) {
    case PROP_INTERFACE:
      if (niimaqsrc->interface_name)
        g_free (niimaqsrc->interface_name);
      niimaqsrc->interface_name = g_strdup (g_value_get_string (value));
      break;
    case PROP_BUFSIZE:
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
    case PROP_INTERFACE:
      g_value_set_string (value, niimaqsrc->interface_name);
      break;
    case PROP_BUFSIZE:
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

static GstCaps *
gst_niimaqsrc_get_caps (GstBaseSrc * bsrc)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (bsrc);

  GST_LOG_OBJECT (bsrc, "Entering function get_caps");

  /* return template caps if the session hasn't started yet */
  if (!niimaqsrc->sid) {
    return
        gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD
            (niimaqsrc)));
  }

  return gst_niimaqsrc_get_cam_caps (niimaqsrc);
}


static gboolean
gst_niimaqsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (bsrc);
  gboolean res = TRUE;
  int depth;

  gst_video_format_parse_caps (caps, &niimaqsrc->format, &niimaqsrc->width,
      &niimaqsrc->height);

  /* this will handle byte alignment (i.e. row multiple of 4 bytes) */
  niimaqsrc->framesize =
      gst_video_format_get_size (niimaqsrc->format, niimaqsrc->width,
      niimaqsrc->height);

  /* TODO: use gst_video_format_get_component once 0.10.37 is out */
  if (niimaqsrc->format == GST_VIDEO_FORMAT_GRAY8)
    depth = 8;
  else if (niimaqsrc->format == GST_VIDEO_FORMAT_GRAY16_LE)
    depth = 16;
  else
    g_assert_not_reached ();    /* negotiation failed? */

  /* use this so NI can give us proper byte alignment */
  niimaqsrc->rowpixels =
      gst_video_format_get_row_stride (niimaqsrc->format, 0,
      niimaqsrc->width) / (depth / 8);

  GST_DEBUG_OBJECT (niimaqsrc, "Caps set, framesize=%d, rowpixels=%d",
      niimaqsrc->framesize, niimaqsrc->rowpixels);

  return res;
}

static void
gst_niimaqsrc_reset (GstNiImaqSrc * niimaqsrc)
{
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
  GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
      ("Camera doesn't seem to want to turn on!"),
      ("Camera doesn't seem to want to turn on!"));

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

  /* we can only do a no-copy if strides are property byte aligned */
  no_copy = niimaqsrc->avoid_copy && niimaqsrc->width == niimaqsrc->rowpixels;

  /* start the IMAQ acquisition session if we haven't done so yet */
  if (!niimaqsrc->session_started) {
    gst_niimaqsrc_start_acquisition (niimaqsrc);
  }

  if (no_copy) {
    GST_LOG_OBJECT (niimaqsrc,
        "Sending IMAQ buffer #%d along without copying", niimaqsrc->cumbufnum);
    *buffer = gst_buffer_new ();
    if (G_UNLIKELY (*buffer == NULL))
      goto error;
    GST_BUFFER_SIZE (*buffer) = niimaqsrc->framesize;
  } else {
    GST_LOG_OBJECT (niimaqsrc, "Copying IMAQ buffer #%d", niimaqsrc->cumbufnum);
    ret =
        gst_pad_alloc_buffer (GST_BASE_SRC_PAD (niimaqsrc), 0,
        niimaqsrc->framesize, GST_PAD_CAPS (GST_BASE_SRC_PAD (niimaqsrc)),
        buffer);
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
    rval =
        imgSessionExamineBuffer2 (niimaqsrc->sid, niimaqsrc->cumbufnum,
        &copied_number, &GST_BUFFER_DATA (*buffer));
    GST_BUFFER_FREE_FUNC (*buffer) = gst_niimaqsrc_release_buffer;
    GST_BUFFER_MALLOCDATA (*buffer) = (guint8 *) niimaqsrc;
  } else if (niimaqsrc->width == niimaqsrc->rowpixels) {
    /* TODO: optionally use ExamineBuffer and byteswap in transfer (to offer BIG_ENDIAN) */
    guint8 *data = GST_BUFFER_DATA (*buffer);
    g_mutex_lock (niimaqsrc->mutex);
    rval =
        imgSessionCopyBufferByNumber (niimaqsrc->sid, niimaqsrc->cumbufnum,
        data, IMG_OVERWRITE_GET_OLDEST, &copied_number, &copied_index);
    timestamp = niimaqsrc->times[copied_index];
    niimaqsrc->times[copied_index] = GST_CLOCK_TIME_NONE;
    g_mutex_unlock (niimaqsrc->mutex);
  } else {
    guint8 *data = GST_BUFFER_DATA (*buffer);
    g_mutex_lock (niimaqsrc->mutex);
    rval =
        imgSessionCopyAreaByNumber (niimaqsrc->sid, niimaqsrc->cumbufnum, 0, 0,
        niimaqsrc->height, niimaqsrc->width, data, niimaqsrc->rowpixels,
        IMG_OVERWRITE_GET_OLDEST, &copied_number, &copied_index);
    timestamp = niimaqsrc->times[copied_index];
    niimaqsrc->times[copied_index] = GST_CLOCK_TIME_NONE;
    g_mutex_unlock (niimaqsrc->mutex);
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
  GST_BUFFER_TIMESTAMP (*buffer) = timestamp;
  GST_BUFFER_DURATION (*buffer) = duration;

  /* the negotiate() method already set caps on the source pad */
  gst_buffer_set_caps (*buffer, GST_PAD_CAPS (GST_BASE_SRC_PAD (niimaqsrc)));

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
        gst_tag_list_new_full (GST_TAG_DATE_TIME, niimaqsrc->start_time, NULL);
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
  gint width, height, depth, bpp;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;

  if (!niimaqsrc->iid) {
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("Camera interface not open"), ("Camera interface not open"));
    goto error;
  }

  GST_LOG_OBJECT (niimaqsrc, "Retrieving attributes from IMAQ interface");
  rval = imgGetAttribute (niimaqsrc->iid, IMG_ATTR_BITSPERPIXEL, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  bpp = val;
  rval &= imgGetAttribute (niimaqsrc->iid, IMG_ATTR_BYTESPERPIXEL, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  depth = val * 8;
  rval &= imgGetAttribute (niimaqsrc->iid, IMG_ATTR_ROI_WIDTH, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  width = val;
  rval &= imgGetAttribute (niimaqsrc->iid, IMG_ATTR_ROI_HEIGHT, &val);
  gst_niimaqsrc_report_imaq_error (rval);
  height = val;

  if (rval) {
    GST_ELEMENT_ERROR (niimaqsrc, STREAM, FAILED,
        ("attempt to read attributes failed"),
        ("attempt to read attributes failed"));
    goto error;
  }

  if (depth == 8)
    format = GST_VIDEO_FORMAT_GRAY8;
  else if (depth == 16)
    format = GST_VIDEO_FORMAT_GRAY16_LE;
  else {
    GST_ERROR_OBJECT (niimaqsrc, "Depth %d (%d-bit) not supported yet", depth,
        bpp);
    goto error;
  }

  /* hard code framerate and par as IMAQ doesn't tell us anything about it */
  gcaps = gst_video_format_new_caps (format, width, height, 30, 1, 1, 1);

  GST_DEBUG_OBJECT (gcaps, "are the camera caps");

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

  GST_LOG_OBJECT (niimaqsrc, "Creating ring with %d buffers",
      niimaqsrc->bufsize);

  /* create array of pointers to give to IMAQ for creating internal buffers */
  niimaqsrc->buflist = g_new (guint32 *, niimaqsrc->bufsize);
  niimaqsrc->times = g_new (GstClockTime, niimaqsrc->bufsize);
  for (i = 0; i < niimaqsrc->bufsize; i++) {
    niimaqsrc->buflist[i] = 0;
    niimaqsrc->times[i] = GST_CLOCK_TIME_NONE;
  }
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
gst_niimaqsrc_query (GstBaseSrc * src, GstQuery * query)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (src);
  gboolean res;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      if (!niimaqsrc->session_started) {
        GST_WARNING_OBJECT (niimaqsrc,
            "Can't give latency since device isn't open!");
        res = FALSE;
      } else {
        GstClockTime min_latency, max_latency;
        /* TODO: this is a ballpark figure, estimate from FVAL times */
        min_latency = 33 * GST_MSECOND;
        max_latency = 33 * GST_MSECOND * niimaqsrc->bufsize;

        GST_DEBUG_OBJECT (niimaqsrc,
            "report latency min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
            GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

        gst_query_set_latency (query, TRUE, min_latency, max_latency);

        res = TRUE;
      }
    }
    default:
      res = FALSE;
      break;
  }

  return res;
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

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, "niimaq",
    "NI-IMAQ source element", plugin_init, VERSION, GST_LICENSE, PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
