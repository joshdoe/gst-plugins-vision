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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gst/interfaces/propertyprobe.h"

#include "gstniimaq.h"

#include <time.h>
#include <string.h>

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
  PROP_TIMESTAMP_OFFSET,
  PROP_BUFSIZE
      /* FILL ME */
};

#define DEFAULT_PROP_INTERFACE "img0"
#define DEFAULT_PROP_TIMESTAMP_OFFSET  0
#define DEFAULT_PROP_BUFSIZE  10

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-gray, "
        "bpp = (int) 8, "
        "depth = (int) 8, "
        "width = (int) [ 1, max ], "
        "height = (int) [ 1, max ], "
        "framerate = (fraction) [ 0, max ]"
        ";"
        "video/x-raw-gray, "
        "bpp = (int) {10, 12, 14, 16}, "
        "depth = (int) 16, "
        "endianness = (int) LITTLE_ENDIAN, "
        "width = (int) [ 1, max ], "
        "height = (int) [ 1, max ], " "framerate = (fraction) [ 0, max ]")
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
static void gst_niimaqsrc_get_times (GstBaseSrc * basesrc,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static gboolean gst_niimaqsrc_start (GstBaseSrc * src);
static gboolean gst_niimaqsrc_stop (GstBaseSrc * src);

/* GstPushSrc virtual methods */
static GstFlowReturn gst_niimaqsrc_create (GstPushSrc * psrc,
    GstBuffer ** buffer);

/* GstNiImaq methods */
static gboolean gst_niimaqsrc_parse_caps (const GstCaps * caps,
    gint * width, gint * height, gint * depth, gint * bpp);

static gboolean gst_niimaqsrc_set_caps_color (GstStructure * gs, gint bpp,
    gint depth);
static gboolean gst_niimaqsrc_set_caps_framesize (GstStructure * gs, gint width,
    gint height);

static GstCaps *gst_niimaqsrc_get_cam_caps (GstNiImaqSrc * src);
static void gst_niimaqsrc_close_interface (GstNiImaqSrc * niimaqsrc);

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

typedef struct _GstNiImaqSrcFrameTime GstNiImaqSrcFrameTime;

struct _GstNiImaqSrcFrameTime
{
  guint32 number;
  GstClockTime time;
};

uInt32
gst_niimaqsrc_frame_start_callback (SESSION_ID sid, IMG_ERR err,
    IMG_SIGNAL_TYPE signal_type, uInt32 signal_identifier, void *userdata)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (userdata);
  GstClock *clock;
  GstNiImaqSrcFrameTime *frametime = g_new (GstNiImaqSrcFrameTime, 1);
  uInt32 val;

  /* get clock time and set to frametime struct */
  clock = gst_element_get_clock (GST_ELEMENT (niimaqsrc));
  frametime->time = gst_clock_get_time (clock);
  gst_object_unref (clock);

  /* get current frame number */
  imgGetAttribute (sid, IMG_ATTR_FRAME_COUNT, &val);
  frametime->number = val;

  /* append frame number and clock time to list */
  g_mutex_lock (niimaqsrc->frametime_mutex);
  niimaqsrc->timelist = g_slist_append (niimaqsrc->timelist, frametime);
  g_mutex_unlock (niimaqsrc->frametime_mutex);

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

    GST_DEBUG_OBJECT (klass, "About to probe for IMAQ interfaces");

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
          G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_TIMESTAMP_OFFSET, g_param_spec_int64 ("timestamp-offset",
          "Timestamp offset",
          "An offset added to timestamps set on buffers (in ns)", G_MININT64,
          G_MAXINT64, DEFAULT_PROP_TIMESTAMP_OFFSET, G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BUFSIZE,
      g_param_spec_int ("buffer-size",
          "Number of frames in the IMAQ ringbuffer",
          "The number of frames in the IMAQ ringbuffer", 1, G_MAXINT,
          DEFAULT_PROP_BUFSIZE, G_PARAM_READWRITE));

  /* install GstBaseSrc vmethod implementations */
  gstbasesrc_class->get_caps = gst_niimaqsrc_get_caps;
  gstbasesrc_class->set_caps = gst_niimaqsrc_set_caps;
  gstbasesrc_class->get_times = gst_niimaqsrc_get_times;
  gstbasesrc_class->start = gst_niimaqsrc_start;
  gstbasesrc_class->stop = gst_niimaqsrc_stop;

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

  /* initialize member variables */
  niimaqsrc->timestamp_offset = 0;
  niimaqsrc->caps = NULL;
  niimaqsrc->bufsize = 10;
  niimaqsrc->n_frames = 0;
  niimaqsrc->cumbufnum = 0;
  niimaqsrc->n_dropped_frames = 0;
  niimaqsrc->buflist = 0;
  niimaqsrc->sid = 0;
  niimaqsrc->iid = 0;
  niimaqsrc->camera_name = g_strdup (DEFAULT_PROP_INTERFACE);
  niimaqsrc->interface_name = g_strdup (DEFAULT_PROP_INTERFACE);
  niimaqsrc->session_started = FALSE;

  niimaqsrc->timelist = NULL;
  niimaqsrc->frametime_mutex = g_mutex_new ();
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
  g_free (niimaqsrc->camera_name);
  niimaqsrc->camera_name = NULL;
  g_free (niimaqsrc->interface_name);
  niimaqsrc->interface_name = NULL;

  if (niimaqsrc->caps)
    gst_caps_unref (niimaqsrc->caps);
  g_slist_free (niimaqsrc->timelist);
  g_mutex_free (niimaqsrc->frametime_mutex);


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

      if (niimaqsrc->camera_name)
        g_free (niimaqsrc->camera_name);
      niimaqsrc->camera_name = g_strdup (g_value_get_string (value));
      break;
    case PROP_TIMESTAMP_OFFSET:
      niimaqsrc->timestamp_offset = g_value_get_int64 (value);
      break;
    case PROP_BUFSIZE:
      niimaqsrc->bufsize = g_value_get_int (value);
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
    case PROP_TIMESTAMP_OFFSET:
      g_value_set_int64 (value, niimaqsrc->timestamp_offset);
      break;
    case PROP_BUFSIZE:
      g_value_set_int (value, niimaqsrc->bufsize);
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

  GST_DEBUG_OBJECT (bsrc, "Entering function get_caps");

  /* return template caps if we don't know the actual camera caps */
  if (!niimaqsrc->caps) {
    return
        gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD
            (niimaqsrc)));
  }

  return gst_caps_copy (niimaqsrc->caps);
}


static gboolean
gst_niimaqsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{

  gboolean res = TRUE;
  GstNiImaqSrc *niimaqsrc;
  gint width, height;
  gint bpp, depth;

  niimaqsrc = GST_NIIMAQSRC (bsrc);

  GST_DEBUG_OBJECT (bsrc, "Entering function set_caps");

  GST_DEBUG_OBJECT (caps, "are the caps being set");

  if (niimaqsrc->caps) {
    gst_caps_unref (niimaqsrc->caps);
    niimaqsrc->caps = gst_caps_copy (caps);
  }

  res =
      gst_niimaqsrc_parse_caps (niimaqsrc->caps, &width, &height, &depth, &bpp);

  if (res) {
    /* looks ok here */
    niimaqsrc->width = width;
    niimaqsrc->height = height;
    niimaqsrc->depth = depth;
    niimaqsrc->bpp = bpp;
    niimaqsrc->framesize = width * height * (depth / 8);
  }

  return res;
}

static void
gst_niimaqsrc_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
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
gst_niimaqsrc_create (GstPushSrc * psrc, GstBuffer ** buffer)
{
  GstNiImaqSrc *niimaqsrc = GST_NIIMAQSRC (psrc);
  gpointer data;
  GstFlowReturn res = GST_FLOW_OK;
  guint i;
  GstNiImaqSrcFrameTime *frametime;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstClockTime timestamp2 = GST_CLOCK_TIME_NONE;
  GstClockTime duration = GST_CLOCK_TIME_NONE;

  uInt32 copied_number;
  uInt32 copied_index;
  Int32 rval;
  uInt32 dropped;

  /* start the IMAQ acquisition session if we haven't done so yet */
  if (!niimaqsrc->session_started) {
    GST_DEBUG_OBJECT (niimaqsrc, "Starting acquisition");

    /* try to open the camera five times */
    for (i = 0; i < 5; i++) {
      rval = imgSessionStartAcquisition (niimaqsrc->sid);
      if (rval == 0) {
        break;
      } else {
        gst_niimaqsrc_report_imaq_error (rval);
        GST_LOG_OBJECT (niimaqsrc, "camera is still off , wait 50ms and retry");
        g_usleep (50000);
      }
    }

    /* we tried five times and failed, so we error */
    if (i >= 5) {
      GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
          ("Camera doesn't seem to want to turn on!"),
          ("Camera doesn't seem to want to turn on!"));

      gst_niimaqsrc_close_interface (niimaqsrc);

      return GST_FLOW_ERROR;
    }
    niimaqsrc->session_started = TRUE;
  }

  data = g_malloc (niimaqsrc->framesize);

  GST_DEBUG_OBJECT (niimaqsrc, "Copying IMAQ buffer %d", niimaqsrc->cumbufnum);

  rval =
      imgSessionCopyBufferByNumber (niimaqsrc->sid, niimaqsrc->cumbufnum, data,
      IMG_OVERWRITE_GET_OLDEST, &copied_number, &copied_index);
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("failed to copy buffer %d", niimaqsrc->cumbufnum),
        ("failed to copy buffer %d, IMAQ error: \"%s\"", niimaqsrc->cumbufnum));
    goto error;
  }

  /* TODO, DEBUG: get running time now to compare to what the callback gives us */
  /*clock = gst_element_get_clock (GST_ELEMENT (niimaqsrc));
     timestamp2 =
     GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (niimaqsrc)), gst_clock_get_time (clock));
     gst_object_unref (clock); */

  GST_DEBUG_OBJECT (niimaqsrc, "Creating buffer");

  *buffer = gst_buffer_new ();

  GST_BUFFER_DATA (*buffer) = data;
  GST_BUFFER_MALLOCDATA (*buffer) = data;
  GST_BUFFER_SIZE (*buffer) = niimaqsrc->framesize;
  GST_BUFFER_OFFSET (*buffer) = copied_number;
  GST_BUFFER_OFFSET_END (*buffer) = copied_number;

  GST_DEBUG_OBJECT (niimaqsrc, "Associating time with buffer");

  /* search linked list for frame time */
  g_mutex_lock (niimaqsrc->frametime_mutex);
  {
    /* remove all old frametimes from the list */
    frametime = niimaqsrc->timelist->data;
    while (frametime->number < copied_number) {
      niimaqsrc->timelist =
          g_slist_delete_link (niimaqsrc->timelist, niimaqsrc->timelist);
      frametime = niimaqsrc->timelist->data;
    }

    if (frametime->number == copied_number) {
      timestamp = frametime->time;

      /* remove frame time as we no longer need it */
      niimaqsrc->timelist =
          g_slist_delete_link (niimaqsrc->timelist, niimaqsrc->timelist);
    } else {
      timestamp = GST_CLOCK_TIME_NONE;
    }
  }
  g_mutex_unlock (niimaqsrc->frametime_mutex);

  /* set timestamp */
  if (timestamp == GST_CLOCK_TIME_NONE) {
    GST_WARNING_OBJECT (niimaqsrc, "No timestamp found; callback failed?");
    timestamp = GST_CLOCK_TIME_NONE;
    /*clock = gst_element_get_clock (GST_ELEMENT (niimaqsrc));
       GST_BUFFER_TIMESTAMP (*buffer) =
       GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (niimaqsrc)), gst_clock_get_time (clock));
       gst_object_unref (clock); */
  } else {
    timestamp =
        GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (niimaqsrc)),
        timestamp);
  }

  /* make guess of duration from timestamp and cumulative buffer number */
  if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
    duration = timestamp / (copied_number + 1);
  } else {
    duration = 33 * GST_MSECOND;
  }

  /* TODO, DEBUG: set duration to see what the difference is between callback and create */
  /*duration = GST_CLOCK_DIFF (timestamp, timestamp2); */


  GST_BUFFER_TIMESTAMP (*buffer) = timestamp;
  GST_BUFFER_DURATION (*buffer) = duration;

  /* the negotiate() method already set caps on the source pad */
  gst_buffer_set_caps (*buffer, GST_PAD_CAPS (GST_BASE_SRC_PAD (niimaqsrc)));

  /*GST_BUFFER_TIMESTAMP (outbuf) = src->timestamp_offset + src->running_time;
     if (src->rate_numerator != 0) {
     GST_BUFFER_DURATION (outbuf) = gst_util_uint64_scale_int (GST_SECOND,
     src->rate_denominator, src->rate_numerator);
     } */

  dropped = copied_number - niimaqsrc->cumbufnum;
  if (dropped > 0) {
    niimaqsrc->n_dropped_frames += dropped;
    GST_WARNING_OBJECT (niimaqsrc, "Asked to copy buffer %d but was given %d",
        niimaqsrc->cumbufnum, copied_number);
    GST_WARNING_OBJECT (niimaqsrc, "Dropped %d frames (%d total)", dropped,
        niimaqsrc->n_dropped_frames);
  }

  /* set cumulative buffer number to get next frame */
  niimaqsrc->cumbufnum = copied_number + 1;
  niimaqsrc->n_frames++;

  /*if (src->rate_numerator != 0) {
     src->running_time = gst_util_uint64_scale_int (src->n_frames * GST_SECOND,
     src->rate_denominator, src->rate_numerator);
     } */

  return res;

error:
  {
    return GST_FLOW_ERROR;
  }
}

/**
* gst_niimaqsrc_parse_caps:
* caps: a #GstCaps to parse
* width:
* height:
* depth:
* bpp:
*
* Parses a given caps and sets critical values.
*
* Returns: TRUE on success
*/
static gboolean
gst_niimaqsrc_parse_caps (const GstCaps * caps, gint * width, gint * height,
    gint * depth, gint * bpp)
{
  GstStructure *structure;
  gboolean ret;

  if (gst_caps_get_size (caps) < 1)
    return FALSE;

  structure = gst_caps_get_structure (caps, 0);

  ret = gst_structure_get (structure,
      "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height,
      "depth", G_TYPE_INT, depth, "bpp", G_TYPE_INT, bpp, NULL);

  if (!ret) {
    GST_DEBUG ("Failed to retrieve width, height, depth, or bpp");
    return FALSE;
  }

  return TRUE;
}

/**
* gst_niimaqsrc_set_caps_color:
* gs: a #GstStructure to set the color of.
* bpp: the bits per pixel to set.
* depth: the depth to set.
*
* Sets the given bpp and depth to the given #GstStructure.
*
* Returns: TRUE on success
*/
static gboolean
gst_niimaqsrc_set_caps_color (GstStructure * gs, gint bpp, gint depth)
{
  gboolean ret = TRUE;

  gst_structure_set_name (gs, "video/x-raw-gray");
  gst_structure_set (gs,
      "bpp", G_TYPE_INT, bpp, "depth", G_TYPE_INT, depth, NULL);
  if (depth > 8) {
    gst_structure_set (gs, "endianness", G_TYPE_INT, G_LITTLE_ENDIAN, NULL);
  }

  return ret;
}

/**
* gst_niimaqsrc_set_caps_framesize:
* gs: #GstStructure
* width: width to set
* height: height to set
*
* Sets the given width and height to the given #GstStructure
*
* Returns: TRUE on success
*/
static gboolean
gst_niimaqsrc_set_caps_framesize (GstStructure * gs, gint width, gint height)
{
  gst_structure_set (gs,
      "width", G_TYPE_INT, width, "height", G_TYPE_INT, height, NULL);

  return TRUE;
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
  GstStructure *gs;

  gcaps = gst_caps_new_empty ();

  if (!niimaqsrc->iid) {
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("Camera interface not open"), ("Camera interface not open"));
    goto error;
  }

  GST_DEBUG_OBJECT (niimaqsrc, "Retrieving attributes from IMAQ interface");
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

  /* create new structure and set caps we got from IMAQ */
  gs = gst_structure_empty_new ("video");
  if (!gst_niimaqsrc_set_caps_color (gs, bpp, depth) ||
      !gst_niimaqsrc_set_caps_framesize (gs, width, height)) {
    GST_ELEMENT_ERROR (niimaqsrc, STREAM, FAILED,
        ("attempt to set caps %dx%dx%d (%d) failed", width, height, depth, bpp),
        ("attempt to set caps %dx%dx%d (%d) failed", width, height, depth,
            bpp));
    goto error;
  }

  /* hard code framerate to 30Hz as IMAQ doesn't tell us anything about it */
  GST_DEBUG_OBJECT (niimaqsrc, "Setting framerate to 30 fps");
  gst_structure_set (gs, "framerate", GST_TYPE_FRACTION, 30, 1, NULL);

  GST_DEBUG_OBJECT (gs, "is the basic structure");

  gst_caps_append_structure (gcaps, gst_structure_copy (gs));

  /* if (8 < bpp < 16), then append structure with bpp=16 so ffmpegcolorspace
   * and other elements can work directly with this src */
  if (bpp > 8 && bpp < 16) {
    GST_DEBUG_OBJECT (niimaqsrc, "Adding 16bpp caps for compatibility");
    gst_niimaqsrc_set_caps_color (gs, 16, 16);
    gst_caps_append_structure (gcaps, gst_structure_copy (gs));
  }
  gst_structure_free (gs);

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

  niimaqsrc->iid = 0;
  niimaqsrc->sid = 0;

  GST_DEBUG_OBJECT (niimaqsrc, "Opening IMAQ interface: %s",
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

  GST_DEBUG_OBJECT (niimaqsrc, "Opening IMAQ session: %s",
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

  if (niimaqsrc->caps) {
    gst_caps_unref (niimaqsrc->caps);
    niimaqsrc->caps = NULL;
  }

  GST_LOG_OBJECT (niimaqsrc, "Getting caps from camera");

  /* get caps from camera and set to src pad */
  niimaqsrc->caps = gst_niimaqsrc_get_cam_caps (niimaqsrc);
  if (niimaqsrc->caps == NULL) {
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("Failed to get caps from IMAQ"), ("Failed to get caps from IMAQ"));
    goto error;
  }

  GST_LOG_OBJECT (niimaqsrc, "Creating ring with %d buffers",
      niimaqsrc->bufsize);

  /* create array of pointers to give to IMAQ for creating internal buffers */
  niimaqsrc->buflist = g_new (guint32 *, niimaqsrc->bufsize);
  for (i = 0; i < niimaqsrc->bufsize; i++) {
    niimaqsrc->buflist[i] = 0;
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
  if (rval) {
    gst_niimaqsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
        ("Failed to register BUF_COMPLETE callback"),
        ("Failed to register BUF_COMPLETE callback"));
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

  /* stop IMAQ session */
  if (niimaqsrc->session_started) {
    rval = imgSessionStopAcquisition (niimaqsrc->sid);
    if (rval) {
      gst_niimaqsrc_report_imaq_error (rval);
      GST_ELEMENT_ERROR (niimaqsrc, RESOURCE, FAILED,
          ("Unable to stop acquisition"), ("Unable to stop acquisition"));
    }
    niimaqsrc->session_started = FALSE;
    GST_DEBUG_OBJECT (niimaqsrc, "Acquisition stopped");
  }

  gst_niimaqsrc_close_interface (niimaqsrc);

  if (niimaqsrc->caps) {
    gst_caps_unref (niimaqsrc->caps);
    niimaqsrc->caps = NULL;
  }
}

/**
* gst_niimaqsrc_close_interface:
* niimaqsrc: #GstNiImaqSrc instance
*
* Close IMAQ session and interface
*
*/
static void
gst_niimaqsrc_close_interface (GstNiImaqSrc * niimaqsrc)
{
  Int32 rval;

  /* close IMAQ session and interface */
  if (niimaqsrc->sid) {
    rval = imgClose (niimaqsrc->sid, TRUE);
    gst_niimaqsrc_report_imaq_error (rval);
    niimaqsrc->sid = 0;
    GST_DEBUG_OBJECT (niimaqsrc, "IMAQ session closed");
  }
  if (niimaqsrc->iid) {
    rval = imgClose (niimaqsrc->iid, TRUE);
    gst_niimaqsrc_report_imaq_error (rval);
    niimaqsrc->iid = 0;
    GST_DEBUG_OBJECT (niimaqsrc, "IMAQ interface closed");
  }
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
