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

/**
 * SECTION:element-niimaqdxdxsrc
 *
 * Source for National Instruments IMAQdx (FireWire, USB, GigE Vision)
 * 
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v niimaqdxdxsrc ! ffmpegcolorspace ! autovideosink
 * ]|
 * </refsect2>
 */

/* FIXME: timestamps sent in GST_TAG_DATE_TIME are off, need to adjust for time of first buffer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstniimaqdx.h"

#include <time.h>
#include <string.h>

#include <gst/interfaces/propertyprobe.h>
#include <gst/video/video.h>

GST_DEBUG_CATEGORY (niimaqdxsrc_debug);
#define GST_CAT_DEFAULT niimaqdxsrc_debug

static GstElementDetails niimaqdxsrc_details =
GST_ELEMENT_DETAILS ("NI-IMAQdx Video Source",
    "Source/Video",
    "National Instruments IMAQdx source, supports FireWire, USB, and GigE Vision cameras",
    "Joshua M. Doe <oss@nvl.army.mil>");

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_RING_BUFFER_COUNT,
  PROP_ATTRIBUTES
};

#define DEFAULT_PROP_DEVICE "cam0"
#define DEFAULT_PROP_RING_BUFFER_COUNT 3
#define DEFAULT_PROP_ATTRIBUTES ""

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static void gst_niimaqdxsrc_init_interfaces (GType type);

GST_BOILERPLATE_FULL (GstNiImaqDxSrc, gst_niimaqdxsrc, GstPushSrc,
    GST_TYPE_PUSH_SRC, gst_niimaqdxsrc_init_interfaces);

/* GObject virtual methods */
static void gst_niimaqdxsrc_dispose (GObject * object);
static void gst_niimaqdxsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_niimaqdxsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* GstBaseSrc virtual methods */
static GstCaps *gst_niimaqdxsrc_get_caps (GstBaseSrc * bsrc);
static gboolean gst_niimaqdxsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps);
static gboolean gst_niimaqdxsrc_start (GstBaseSrc * src);
static gboolean gst_niimaqdxsrc_stop (GstBaseSrc * src);
static gboolean gst_niimaqdxsrc_query (GstBaseSrc * src, GstQuery * query);

/* GstPushSrc virtual methods */
static GstFlowReturn gst_niimaqdxsrc_create (GstPushSrc * psrc,
    GstBuffer ** buffer);

/* GstNiImaqDx methods */
static GstCaps *gst_niimaqdxsrc_get_cam_caps (GstNiImaqDxSrc * src);
static gboolean gst_niimaqdxsrc_close_interface (GstNiImaqDxSrc * niimaqdxsrc);
static void gst_niimaqdxsrc_reset (GstNiImaqDxSrc * niimaqdxsrc);

IMAQdxError
gst_niimaqdxsrc_report_imaq_error (IMAQdxError code)
{
  static char imaqdx_error_string[IMAQDX_MAX_API_STRING_LENGTH];
  if (code) {
    IMAQdxGetErrorString (code, imaqdx_error_string,
        IMAQDX_MAX_API_STRING_LENGTH);
    GST_ERROR ("IMAQdx error: %s", imaqdx_error_string);
  }
  return code;
}

/* This will be called "when a frame done event occurs."
 * FIXME true?: If acquisition blocks because we don't copy buffers fast enough, the number
 * of times this function is called will be less than the IMAQ cumulative
 * buffer count. */
uInt32
gst_niimaqdxsrc_frame_done_callback (IMAQdxSession session, uInt32 bufferNumber,
    void *userdata)
{
  GstNiImaqDxSrc *niimaqdxsrc = GST_NIIMAQDXSRC (userdata);
  GstClockTime abstime;
  static guint32 index = 0;

  g_mutex_lock (niimaqdxsrc->mutex);

  /* time hasn't been read yet, this frame will be dropped */
  if (niimaqdxsrc->times[index] != GST_CLOCK_TIME_NONE) {
    g_mutex_unlock (niimaqdxsrc->mutex);
    return 1;
  }

  /* get clock time */
  abstime = gst_clock_get_time (GST_ELEMENT_CLOCK (niimaqdxsrc));
  niimaqdxsrc->times[index] = abstime;

  if (G_UNLIKELY (niimaqdxsrc->start_time == NULL))
    niimaqdxsrc->start_time = gst_date_time_new_now_utc ();

  /* first frame, use as element base time */
  if (niimaqdxsrc->base_time == GST_CLOCK_TIME_NONE)
    niimaqdxsrc->base_time = abstime;

  index = (index + 1) % niimaqdxsrc->ringbuffer_count;

  g_mutex_unlock (niimaqdxsrc->mutex);

  /* return 1 to rearm the callback */
  return 1;
}

#define GST_VIDEO_CAPS_BAYER(format)                                \
    "video/x-raw-bayer, "                                           \
    "format = " format ","                                          \
    "width = " GST_VIDEO_SIZE_RANGE ", "                            \
    "height = " GST_VIDEO_SIZE_RANGE ", "                           \
    "framerate = " GST_VIDEO_FPS_RANGE

ImaqDxCapsInfo imaq_dx_caps_infos[] = {
  {"Mono 8", GST_VIDEO_CAPS_GRAY8, 8, 8, 4}
  ,
  //TODO: for packed formats, should we unpack?
  //{"Mono 12 Packed", GST_VIDEO_CAPS_GRAY16 ("BIG_ENDIAN"), 16, 16},
  {"Mono 16", GST_VIDEO_CAPS_GRAY16 ("BIG_ENDIAN"), 16, 16, 4}
  ,
  {"YUV 422 Packed", GST_VIDEO_CAPS_YUV ("UYVY"), 16, 16, 4}
  ,
  {"Bayer BG 8", GST_VIDEO_CAPS_BAYER ("bggr"), 8, 8, 1}
  ,
  //TODO: use a caps string that agrees with Aravis
  {"Bayer BG 16", GST_VIDEO_CAPS_BAYER ("bggr16"), 16, 16, 1}
};

static ImaqDxCapsInfo *
gst_niimaqdxsrc_get_caps_info (const char *pixel_format)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (imaq_dx_caps_infos); i++) {
    if (g_strcmp0 (pixel_format, imaq_dx_caps_infos[i].pixel_format) == 0)
      return &imaq_dx_caps_infos[i];
  }
  return NULL;
}

static const char *
gst_niimaqdxsrc_pixel_format_to_caps_string (const char *pixel_format)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (imaq_dx_caps_infos); i++) {
    if (g_strcmp0 (pixel_format, imaq_dx_caps_infos[i].pixel_format) == 0)
      break;
  }

  if (i == G_N_ELEMENTS (imaq_dx_caps_infos)) {
    GST_WARNING ("PixelFormat '%s' is not supported",
        imaq_dx_caps_infos[i].pixel_format);
    return NULL;
  }

  return imaq_dx_caps_infos[i].gst_caps_string;
}

static const char *
gst_niimaqdxsrc_pixel_format_from_caps (const GstCaps * caps)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (imaq_dx_caps_infos); i++) {
    GstCaps *super_caps;
    super_caps = gst_caps_from_string (imaq_dx_caps_infos[i].gst_caps_string);
    if (gst_caps_is_subset (caps, super_caps))
      return imaq_dx_caps_infos[i].pixel_format;
  }

  return NULL;
}

static int
gst_niimaqdxsrc_pixel_format_get_bpp (const char *pixel_format)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (imaq_dx_caps_infos); i++) {
    if (g_strcmp0 (pixel_format, imaq_dx_caps_infos[i].pixel_format) == 0) {
      return imaq_dx_caps_infos[i].bpp;
    }
  }
  return 0;
}

static int
gst_niimaqdxsrc_pixel_format_get_stride (const char *pixel_format, int width)
{
  return width * gst_niimaqdxsrc_pixel_format_get_bpp (pixel_format) / 8;
}

static GstCaps *
gst_niimaqdxsrc_new_caps_from_pixel_format (const char *pixel_format,
    int width, int height,
    int framerate_n, int framerate_d, int par_n, int par_d)
{
  const char *caps_string;
  GstCaps *caps;
  GstStructure *structure;

  caps_string = gst_niimaqdxsrc_pixel_format_to_caps_string (pixel_format);
  if (caps_string == NULL)
    return NULL;

  structure = gst_structure_from_string (caps_string, NULL);
  gst_structure_set (structure,
      "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height,
      "framerate", GST_TYPE_FRACTION, framerate_n, framerate_d,
      "par", GST_TYPE_FRACTION, par_n, par_d, NULL);

  caps = gst_caps_new_empty ();
  gst_caps_append_structure (caps, structure);

  return caps;
}


static void _____BEGIN_FUNCTIONS_____ ();

/**
* gst_niimaqdxsrc_probe_get_properties:
* @probe: #GstPropertyProbe
*
* Gets list of properties that can be probed
*
* Returns: #GList of properties that can be probed
*/
static const GList *
gst_niimaqdxsrc_probe_get_properties (GstPropertyProbe * probe)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (probe);
  static GList *list = NULL;

  if (!list) {
    list = g_list_append (NULL, g_object_class_find_property (klass, "device"));
  }

  return list;
}

static gboolean _imaqdx_init = FALSE;
static GList *_imaqdx_devices = NULL;

/**
* gst_niimaqdxsrc_class_probe_devices:
* @klass: #GstNiImaqDxClass
* @check: whether to enumerate devices
*
* Probes NI-IMAQdx driver for available interfaces
*
* Returns: TRUE always
*/
static gboolean
gst_niimaqdxsrc_class_probe_devices (GstNiImaqDxSrcClass * klass,
    gboolean check)
{
  if (!check) {
    guint32 i;
    uInt32 count;
    IMAQdxError rval = IMAQdxErrorSuccess;
    IMAQdxCameraInformation *cameraInformationArray = NULL;

    /* clear device list */
    while (_imaqdx_devices) {
      gchar *iface = _imaqdx_devices->data;
      _imaqdx_devices = g_list_remove (_imaqdx_devices, iface);
      g_free (iface);
    }

    GST_LOG_OBJECT (klass, "About to probe for IMAQdx interfaces");

    // get count of connected cameras
    rval = IMAQdxEnumerateCameras (NULL, &count, TRUE);
    if (rval != IMAQdxErrorSuccess) {
      gst_niimaqdxsrc_report_imaq_error (rval);
      return FALSE;
    }

    cameraInformationArray = g_new (IMAQdxCameraInformation, count);

    rval = IMAQdxEnumerateCameras (cameraInformationArray, &count, TRUE);
    if (rval != IMAQdxErrorSuccess) {
      gst_niimaqdxsrc_report_imaq_error (rval);
      return FALSE;
    }

    /* enumerate devices */
    for (i = 0; i < count; i++) {
      gchar *iname;
      IMAQdxCameraInformation *info = &cameraInformationArray[i];

      GST_DEBUG_OBJECT (klass, "Found camera %s: %s, %s, %s, %s",
          info->InterfaceName, info->VendorName, info->ModelName,
          info->CameraFileName, info->CameraAttributeURL);

      iname = g_strdup (info->InterfaceName);
      _imaqdx_devices = g_list_append (_imaqdx_devices, iname);
    }
    g_free (cameraInformationArray);

    _imaqdx_init = TRUE;
  }

  klass->devices = _imaqdx_devices;

  return _imaqdx_init;
}

/**
* gst_niimaqdxsrc_probe_probe_property:
* @probe: #GstPropertyProbe
* @prop_id: Property id
* @pspec: #GParamSpec
*
* GstPropertyProbe _probe_proprty vmethod implementation that probes a
*   property for possible values
*/
static void
gst_niimaqdxsrc_probe_probe_property (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstNiImaqDxSrcClass *klass = GST_NIIMAQDXSRC_GET_CLASS (probe);

  switch (prop_id) {
    case PROP_DEVICE:
      gst_niimaqdxsrc_class_probe_devices (klass, FALSE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }
}

/**
* gst_niimaqdxsrc_probe_needs_probe:
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
gst_niimaqdxsrc_probe_needs_probe (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstNiImaqDxSrcClass *klass = GST_NIIMAQDXSRC_GET_CLASS (probe);
  gboolean ret = FALSE;

  switch (prop_id) {
    case PROP_DEVICE:
      ret = !gst_niimaqdxsrc_class_probe_devices (klass, TRUE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (probe, prop_id, pspec);
      break;
  }

  return ret;
}

/**
* gst_niimaqdxsrc_class_list_interfaces:
* @klass: #GstNiImaqDxClass
*
* Returns: #GValueArray of interface names
*/
static GValueArray *
gst_niimaqdxsrc_class_list_devices (GstNiImaqDxSrcClass * klass)
{
  GValueArray *array;
  GValue value = { 0 };
  GList *item;

  if (!klass->devices)
    return NULL;

  array = g_value_array_new (g_list_length (klass->devices));
  item = klass->devices;
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
* gst_niimaqdxsrc_probe_get_values:
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
gst_niimaqdxsrc_probe_get_values (GstPropertyProbe * probe,
    guint prop_id, const GParamSpec * pspec)
{
  GstNiImaqDxSrcClass *klass = GST_NIIMAQDXSRC_GET_CLASS (probe);
  GValueArray *array = NULL;

  switch (prop_id) {
    case PROP_DEVICE:
      array = gst_niimaqdxsrc_class_list_devices (klass);
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
gst_niimaqdxsrc_property_probe_interface_init (GstPropertyProbeInterface *
    iface)
{
  iface->get_properties = gst_niimaqdxsrc_probe_get_properties;
  iface->probe_property = gst_niimaqdxsrc_probe_probe_property;
  iface->needs_probe = gst_niimaqdxsrc_probe_needs_probe;
  iface->get_values = gst_niimaqdxsrc_probe_get_values;
}

/**
* gst_niimaqdxsrc_init_interfaces:
* @type: #GType
*
* Initialize all GStreamer interfaces
*/
static void
gst_niimaqdxsrc_init_interfaces (GType type)
{
  static const GInterfaceInfo niimaqdx_propertyprobe_info = {
    (GInterfaceInitFunc) gst_niimaqdxsrc_property_probe_interface_init,
    NULL,
    NULL,
  };

  g_type_add_interface_static (type,
      GST_TYPE_PROPERTY_PROBE, &niimaqdx_propertyprobe_info);
}

/**
* gst_niimaqdxsrc_base_init:
* g_class:
*
* Base GObject initialization
*/
static void
gst_niimaqdxsrc_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (element_class, &niimaqdxsrc_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
}

/**
* gst_niimaqdxsrc_class_init:
* klass: #GstNiImaqDxClass to initialize
*
* Initialize #GstNiImaqDxClass, which occurs only once no matter how many
* instances of the class there are
*/
static void
gst_niimaqdxsrc_class_init (GstNiImaqDxSrcClass * klass)
{
  /* get pointers to base classes */
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;

  /* install GObject vmethod implementations */
  gobject_class->dispose = gst_niimaqdxsrc_dispose;
  gobject_class->set_property = gst_niimaqdxsrc_set_property;
  gobject_class->get_property = gst_niimaqdxsrc_get_property;

  /* install GObject properties */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_DEVICE, g_param_spec_string ("device",
          "Device", "NI-IMAQdx camera to open", DEFAULT_PROP_DEVICE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_RING_BUFFER_COUNT, g_param_spec_int ("ring-buffer-count",
          "Ring Buffer Count",
          "The number of buffers in the internal IMAQdx ringbuffer", 1,
          G_MAXINT, DEFAULT_PROP_RING_BUFFER_COUNT,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_ATTRIBUTES, g_param_spec_string ("attributes",
          "Attributes", "Initial attributes to set", DEFAULT_PROP_ATTRIBUTES,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  /* install GstBaseSrc vmethod implementations */
  gstbasesrc_class->get_caps = gst_niimaqdxsrc_get_caps;
  gstbasesrc_class->set_caps = gst_niimaqdxsrc_set_caps;
  gstbasesrc_class->start = gst_niimaqdxsrc_start;
  gstbasesrc_class->stop = gst_niimaqdxsrc_stop;
  gstbasesrc_class->query = gst_niimaqdxsrc_query;

  /* install GstPushSrc vmethod implementations */
  gstpushsrc_class->create = gst_niimaqdxsrc_create;
}

/**
* gst_niimaqdxsrc_init:
* src: the #GstNiImaqDx instance to initialize
* g_class: #GstNiImaqDxClass
*
* Initialize this instance of #GstNiImaqDx
*/
static void
gst_niimaqdxsrc_init (GstNiImaqDxSrc * niimaqdxsrc,
    GstNiImaqDxSrcClass * g_class)
{
  GstPad *srcpad = GST_BASE_SRC_PAD (niimaqdxsrc);

  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (niimaqdxsrc), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (niimaqdxsrc), GST_FORMAT_TIME);

  niimaqdxsrc->mutex = g_mutex_new ();

  /* initialize properties */
  niimaqdxsrc->ringbuffer_count = DEFAULT_PROP_RING_BUFFER_COUNT;
  niimaqdxsrc->device_name = g_strdup (DEFAULT_PROP_DEVICE);
  niimaqdxsrc->attributes = g_strdup (DEFAULT_PROP_ATTRIBUTES);

  /* initialize pointers, then call reset to initialize the rest */
  niimaqdxsrc->times = NULL;
  niimaqdxsrc->temp_buffer = NULL;
  gst_niimaqdxsrc_reset (niimaqdxsrc);
}

/**
* gst_niimaqdxsrc_dispose:
* object: #GObject to dispose
*
* Disposes of the #GObject as part of object destruction
*/
static void
gst_niimaqdxsrc_dispose (GObject * object)
{
  GstNiImaqDxSrc *niimaqdxsrc = GST_NIIMAQDXSRC (object);

  gst_niimaqdxsrc_close_interface (niimaqdxsrc);

  /* free memory allocated */
  g_free (niimaqdxsrc->device_name);
  niimaqdxsrc->device_name = NULL;

  /* unref objects */
  if (niimaqdxsrc->start_time) {
    gst_date_time_unref (niimaqdxsrc->start_time);
    niimaqdxsrc->start_time = NULL;
  }

  /* chain dispose fuction of parent class */
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_niimaqdxsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNiImaqDxSrc *niimaqdxsrc = GST_NIIMAQDXSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      if (niimaqdxsrc->device_name)
        g_free (niimaqdxsrc->device_name);
      niimaqdxsrc->device_name = g_strdup (g_value_get_string (value));
      break;
    case PROP_RING_BUFFER_COUNT:
      niimaqdxsrc->ringbuffer_count = g_value_get_int (value);
      break;
    case PROP_ATTRIBUTES:
      if (niimaqdxsrc->attributes)
        g_free (niimaqdxsrc->attributes);
      niimaqdxsrc->attributes = g_strdup (g_value_get_string (value));
      break;
    default:
      break;
  }
}

static void
gst_niimaqdxsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNiImaqDxSrc *niimaqdxsrc = GST_NIIMAQDXSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, niimaqdxsrc->device_name);
      break;
    case PROP_RING_BUFFER_COUNT:
      g_value_set_int (value, niimaqdxsrc->ringbuffer_count);
      break;
    case PROP_ATTRIBUTES:
      g_value_set_string (value, niimaqdxsrc->attributes);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_niimaqdxsrc_get_caps (GstBaseSrc * bsrc)
{
  GstNiImaqDxSrc *niimaqdxsrc = GST_NIIMAQDXSRC (bsrc);

  GST_LOG_OBJECT (bsrc, "Entering function get_caps");

  /* return template caps if the session hasn't started yet */
  if (!niimaqdxsrc->session) {
    return
        gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD
            (niimaqdxsrc)));
  }
  //TODO: should also call this when first opening the camera, in case format isn't supported
  return gst_niimaqdxsrc_get_cam_caps (niimaqdxsrc);
}


static gboolean
gst_niimaqdxsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstNiImaqDxSrc *niimaqdxsrc = GST_NIIMAQDXSRC (bsrc);
  gboolean res = TRUE;
  GstStructure *structure;
  const char *pixel_format;

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_get_int (structure, "width", &niimaqdxsrc->width);
  gst_structure_get_int (structure, "height", &niimaqdxsrc->height);

  pixel_format = gst_niimaqdxsrc_pixel_format_from_caps (caps);
  g_assert (pixel_format);

  niimaqdxsrc->caps_info = gst_niimaqdxsrc_get_caps_info (pixel_format);

  niimaqdxsrc->dx_row_stride =
      gst_niimaqdxsrc_pixel_format_get_stride (pixel_format,
      niimaqdxsrc->width);

  niimaqdxsrc->framesize =
      GST_ROUND_UP_4 (niimaqdxsrc->dx_row_stride) * niimaqdxsrc->height;

  if (niimaqdxsrc->temp_buffer)
    g_free (niimaqdxsrc->temp_buffer);

  niimaqdxsrc->temp_buffer = g_malloc (niimaqdxsrc->framesize);

  GST_DEBUG ("Size %dx%d", niimaqdxsrc->width, niimaqdxsrc->height);

  GST_LOG_OBJECT (niimaqdxsrc, "Caps set, framesize=%d",
      niimaqdxsrc->framesize);

  return res;
}

static void
gst_niimaqdxsrc_reset (GstNiImaqDxSrc * niimaqdxsrc)
{
  GST_LOG_OBJECT (niimaqdxsrc, "Resetting instance");

  /* initialize member variables */
  niimaqdxsrc->n_frames = 0;
  niimaqdxsrc->cumbufnum = 0;
  niimaqdxsrc->n_dropped_frames = 0;
  niimaqdxsrc->session = 0;
  niimaqdxsrc->session_started = FALSE;
  niimaqdxsrc->width = 0;
  niimaqdxsrc->height = 0;
  niimaqdxsrc->dx_row_stride = 0;
  niimaqdxsrc->start_time = NULL;
  niimaqdxsrc->start_time_sent = FALSE;
  niimaqdxsrc->base_time = GST_CLOCK_TIME_NONE;

  g_free (niimaqdxsrc->temp_buffer);
  niimaqdxsrc->temp_buffer = NULL;

  g_free (niimaqdxsrc->times);
  niimaqdxsrc->times = NULL;
}

static gboolean
gst_niimaqdxsrc_start_acquisition (GstNiImaqDxSrc * niimaqdxsrc)
{
  int i;
  IMAQdxError rval;

  g_assert (!niimaqdxsrc->session_started);

  GST_DEBUG_OBJECT (niimaqdxsrc, "Starting acquisition");

  /* try to open the camera five times */
  for (i = 0; i < 5; i++) {
    rval = IMAQdxStartAcquisition (niimaqdxsrc->session);
    if (rval == IMAQdxErrorSuccess) {
      niimaqdxsrc->session_started = TRUE;
      return TRUE;
    } else {
      gst_niimaqdxsrc_report_imaq_error (rval);
      GST_LOG_OBJECT (niimaqdxsrc, "camera is still off , wait 50ms and retry");
      g_usleep (50000);
    }
  }

  /* we tried five times and failed, so we error */
  gst_niimaqdxsrc_close_interface (niimaqdxsrc);

  return FALSE;
}

static GstClockTime
gst_niimaqdxsrc_get_timestamp_from_buffer_number (GstNiImaqDxSrc * niimaqdxsrc,
    guint32 buffer_number)
{
  GstClockTime abstime;

  abstime = niimaqdxsrc->times[(buffer_number) % niimaqdxsrc->ringbuffer_count];
  niimaqdxsrc->times[(buffer_number) % niimaqdxsrc->ringbuffer_count] =
      GST_CLOCK_TIME_NONE;

  if (abstime == GST_CLOCK_TIME_NONE)
    GST_WARNING_OBJECT (niimaqdxsrc,
        "No valid time found for buffer %d, callback failed?", buffer_number);

  return abstime;
}

#define ROUND_UP_N(num, n)  (((num)+((n)-1))&~((n)-1))

static GstFlowReturn
gst_niimaqdxsrc_create (GstPushSrc * psrc, GstBuffer ** buffer)
{
  GstNiImaqDxSrc *niimaqdxsrc = GST_NIIMAQDXSRC (psrc);
  GstFlowReturn ret = GST_FLOW_OK;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstClockTime duration;
  uInt32 copied_number;
  IMAQdxError rval;
  uInt32 dropped;
  gboolean do_align_stride;

  /* start the IMAQ acquisition session if we haven't done so yet */
  if (!niimaqdxsrc->session_started) {
    if (!gst_niimaqdxsrc_start_acquisition (niimaqdxsrc)) {
      GST_ELEMENT_ERROR (niimaqdxsrc, RESOURCE, FAILED,
          ("Unable to start acquisition."), (NULL));
      return GST_FLOW_ERROR;
    }
  }

  GST_LOG_OBJECT (niimaqdxsrc, "Copying IMAQ buffer #%d",
      niimaqdxsrc->cumbufnum);
  ret =
      gst_pad_alloc_buffer (GST_BASE_SRC_PAD (niimaqdxsrc), 0,
      niimaqdxsrc->framesize, GST_PAD_CAPS (GST_BASE_SRC_PAD (niimaqdxsrc)),
      buffer);
  if (ret != GST_FLOW_OK) {
    GST_ELEMENT_ERROR (niimaqdxsrc, RESOURCE, FAILED,
        ("Failed to get downstream pad to allocate buffer"), (NULL));
    goto error;
  }

  do_align_stride =
      (niimaqdxsrc->dx_row_stride % niimaqdxsrc->caps_info->row_multiple) != 0;

  g_mutex_lock (niimaqdxsrc->mutex);
  if (!do_align_stride) {
    // we have properly aligned strides, copy directly to buffer
    rval = IMAQdxGetImageData (niimaqdxsrc->session, GST_BUFFER_DATA (*buffer),
        GST_BUFFER_SIZE (*buffer), IMAQdxBufferNumberModeBufferNumber,
        niimaqdxsrc->cumbufnum, &copied_number);
  } else {
    // we don't have aligned strides, copy to temp buffer
    rval = IMAQdxGetImageData (niimaqdxsrc->session, niimaqdxsrc->temp_buffer,
        GST_BUFFER_SIZE (*buffer), IMAQdxBufferNumberModeBufferNumber,
        niimaqdxsrc->cumbufnum, &copied_number);
  }

  //FIXME: handle timestamps
  //timestamp = niimaqdxsrc->times[copied_index];
  //niimaqdxsrc->times[copied_index] = GST_CLOCK_TIME_NONE;
  g_mutex_unlock (niimaqdxsrc->mutex);

  // adjust for row stride if needed (must be multiple of 4)
  if (do_align_stride) {
    int i;
    int dx_row_stride = niimaqdxsrc->dx_row_stride;
    int gst_row_stride =
        ROUND_UP_N (dx_row_stride, niimaqdxsrc->caps_info->row_multiple);
    guint8 *src = niimaqdxsrc->temp_buffer;
    guint8 *dst = GST_BUFFER_DATA (*buffer);

    GST_LOG_OBJECT (niimaqdxsrc,
        "Row stride not aligned, copying %d -> %d",
        dx_row_stride, gst_row_stride);
    for (i = 0; i < niimaqdxsrc->height; i++)
      memcpy (dst + i * gst_row_stride, src + i * dx_row_stride, dx_row_stride);
  }

  if (rval) {
    gst_niimaqdxsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqdxsrc, RESOURCE, FAILED,
        ("failed to copy buffer %d", niimaqdxsrc->cumbufnum), (NULL));
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
  //TODO: handle timestamps
  //GST_BUFFER_TIMESTAMP (*buffer) =
  //    timestamp - gst_element_get_base_time (GST_ELEMENT (niimaqdxsrc));
  GST_BUFFER_DURATION (*buffer) = duration;

  /* the negotiate() method already set caps on the source pad */
  gst_buffer_set_caps (*buffer, GST_PAD_CAPS (GST_BASE_SRC_PAD (niimaqdxsrc)));

  dropped = copied_number - niimaqdxsrc->cumbufnum;
  if (dropped > 0) {
    niimaqdxsrc->n_dropped_frames += dropped;
    GST_WARNING_OBJECT (niimaqdxsrc,
        "Asked to copy buffer #%d but was given #%d; just dropped %d frames (%d total)",
        niimaqdxsrc->cumbufnum, copied_number, dropped,
        niimaqdxsrc->n_dropped_frames);
  }

  /* set cumulative buffer number to get next frame */
  niimaqdxsrc->cumbufnum = copied_number + 1;
  niimaqdxsrc->n_frames++;

  if (G_UNLIKELY (niimaqdxsrc->start_time && !niimaqdxsrc->start_time_sent)) {
    GstTagList *tl =
        gst_tag_list_new_full (GST_TAG_DATE_TIME, niimaqdxsrc->start_time,
        NULL);
    GstEvent *e = gst_event_new_tag (tl);
    GST_DEBUG_OBJECT (niimaqdxsrc, "Sending start time event: %" GST_PTR_FORMAT,
        e);
    gst_pad_push_event (GST_BASE_SRC_PAD (niimaqdxsrc), e);
    niimaqdxsrc->start_time_sent = TRUE;
  }
  return ret;

error:
  {
    return ret;
  }
}

void
listAttributes (IMAQdxSession session)
{
  IMAQdxAttributeInformation *attributeInfoArray = NULL;
  uInt32 attributeCount;
  int i;
  IMAQdxError rval;
  char *attributeTypeStrings[] = { "U32", "I64",
    "F64",
    "String",
    "Enum",
    "Bool",
    "Command",
    "Blob"
  };
  char attributeString[IMAQDX_MAX_API_STRING_LENGTH];

  rval =
      IMAQdxEnumerateAttributes2 (session, NULL, &attributeCount, "",
      IMAQdxAttributeVisibilityAdvanced);
  GST_DEBUG ("Found %d attributes", attributeCount);
  attributeInfoArray = g_new (IMAQdxAttributeInformation, attributeCount);
  rval =
      IMAQdxEnumerateAttributes2 (session, attributeInfoArray, &attributeCount,
      "", IMAQdxAttributeVisibilityAdvanced);
  GST_DEBUG ("Enumerating %d attributes", attributeCount);
  for (i = 0; i < attributeCount; i++) {
    IMAQdxAttributeInformation *info = attributeInfoArray + i;
    g_assert (info);

    rval =
        IMAQdxGetAttribute (session, info->Name, IMAQdxValueTypeString,
        attributeString);
    if (rval != IMAQdxErrorSuccess) {
      GST_WARNING ("Failed to read value of attribute %s", info->Name);
      continue;
    }
    printf ("%s, %s/%s, %s, %s\n", info->Name, info->Readable ? "R" : "-",
        info->Writable ? "W" : "-", attributeTypeStrings[info->Type],
        attributeString);
    //gchar *newOutput = g_strconcat(output, line, NULL);
    //g_free (output);
    //g_free (line);
    //output = newOutput;
  }
  g_free (attributeInfoArray);
}

/**
* gst_niimaqdxsrc_get_cam_caps:
* src: #GstNiImaqDx instance
*
* Get caps of camera attached to open IMAQ interface
*
* Returns: the #GstCaps of the src pad. Unref the caps when you no longer need it.
*/
GstCaps *
gst_niimaqdxsrc_get_cam_caps (GstNiImaqDxSrc * niimaqdxsrc)
{
  GstCaps *caps = NULL;
  IMAQdxError rval;
  uInt32 val;
  char pixel_format[IMAQDX_MAX_API_STRING_LENGTH];
  gint width, height;

  if (!niimaqdxsrc->session) {
    GST_ELEMENT_ERROR (niimaqdxsrc, RESOURCE, FAILED,
        ("Camera not open"), (NULL));
    goto error;
  }

  GST_LOG_OBJECT (niimaqdxsrc, "Retrieving attributes from IMAQdx device");

  listAttributes (niimaqdxsrc->session);

  rval = IMAQdxGetAttribute (niimaqdxsrc->session, IMAQdxAttributePixelFormat,
      IMAQdxValueTypeString, &pixel_format);
  gst_niimaqdxsrc_report_imaq_error (rval);
  rval &= IMAQdxGetAttribute (niimaqdxsrc->session, IMAQdxAttributeWidth,
      IMAQdxValueTypeU32, &val);
  gst_niimaqdxsrc_report_imaq_error (rval);
  width = val;
  rval &= IMAQdxGetAttribute (niimaqdxsrc->session, IMAQdxAttributeHeight,
      IMAQdxValueTypeU32, &val);
  gst_niimaqdxsrc_report_imaq_error (rval);
  height = val;

  if (rval) {
    GST_ELEMENT_ERROR (niimaqdxsrc, STREAM, FAILED,
        ("attempt to read attributes failed"),
        ("attempt to read attributes failed"));
    goto error;
  }
  //TODO: add all available caps by enumerating PixelFormat's available, and query for framerate
  caps =
      gst_niimaqdxsrc_new_caps_from_pixel_format (pixel_format, width, height,
      30, 1, 1, 1);
  if (!caps) {
    GST_ERROR_OBJECT (niimaqdxsrc, "PixelFormat '%s' not supported yet",
        pixel_format);
    goto error;
  }

  GST_LOG_OBJECT (caps, "are the camera caps");

  return caps;

error:

  if (caps) {
    gst_caps_unref (caps);
  }

  return NULL;
}

void
gst_niimaqdxsrc_set_dx_attributes (GstNiImaqDxSrc * niimaqdxsrc)
{
  gchar **pairs;
  int i;
  IMAQdxError rval;

  pairs = g_strsplit (niimaqdxsrc->attributes, ";", 0);

  for (i = 0;; i++) {
    gchar **pair;

    if (!pairs[i])
      break;

    pair = g_strsplit (pairs[i], "=", 2);

    g_assert (pair[0]);
    g_assert (pair[1]);
    GST_DEBUG_OBJECT (niimaqdxsrc, "Setting attribute, '%s'='%s'", pair[0],
        pair[1]);

    IMAQdxSetAttribute (niimaqdxsrc->session, pair[0], IMAQdxValueTypeString,
        (const char *) pair[1]);
    if (rval != IMAQdxErrorSuccess) {
      gst_niimaqdxsrc_report_imaq_error (rval);
    }
    g_strfreev (pair);
  }
  g_strfreev (pairs);
}

/**
* gst_niimaqdxsrc_start:
* src: #GstBaseSrc instance
*
* Open necessary resources
*
* Returns: TRUE on success
*/
static gboolean
gst_niimaqdxsrc_start (GstBaseSrc * src)
{
  GstNiImaqDxSrc *niimaqdxsrc = GST_NIIMAQDXSRC (src);
  IMAQdxError rval;
  gint i;

  gst_niimaqdxsrc_reset (niimaqdxsrc);

  GST_LOG_OBJECT (niimaqdxsrc, "Opening IMAQ interface: %s",
      niimaqdxsrc->device_name);

  /* open IMAQ interface */
  rval = IMAQdxOpenCamera (niimaqdxsrc->device_name,
      IMAQdxCameraControlModeController, &niimaqdxsrc->session);
  if (rval != IMAQdxErrorSuccess) {
    gst_niimaqdxsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqdxsrc, RESOURCE, FAILED,
        ("Failed to open IMAQdx interface"),
        ("Failed to open camera interface %s", niimaqdxsrc->device_name));
    goto error;
  }

  GST_LOG_OBJECT (niimaqdxsrc, "Creating ring with %d buffers",
      niimaqdxsrc->ringbuffer_count);

  /* create array of times */
  niimaqdxsrc->times = g_new (GstClockTime, niimaqdxsrc->ringbuffer_count);
  for (i = 0; i < niimaqdxsrc->ringbuffer_count; i++) {
    niimaqdxsrc->times[i] = GST_CLOCK_TIME_NONE;
  }

  rval = IMAQdxConfigureAcquisition (niimaqdxsrc->session, TRUE,
      niimaqdxsrc->ringbuffer_count);
  if (rval) {
    gst_niimaqdxsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqdxsrc, RESOURCE, FAILED,
        ("Failed to create ring buffer"),
        ("Failed to create ring buffer with %d buffers",
            niimaqdxsrc->ringbuffer_count));
    goto error;
  }

  GST_LOG_OBJECT (niimaqdxsrc, "Registering callback functions");
  //TODO: enable this callback
  //rval = IMAQdxRegisterFrameDoneEvent (niimaqdxsrc->session, 1, gst_niimaqdxsrc_frame_done_callback, niimaqdxsrc);
  if (rval) {
    gst_niimaqdxsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (niimaqdxsrc, RESOURCE, FAILED,
        ("Failed to register callback(s)"), (NULL));
    goto error;
  }

  gst_niimaqdxsrc_set_dx_attributes (niimaqdxsrc);

  return TRUE;

error:
  gst_niimaqdxsrc_close_interface (niimaqdxsrc);

  return FALSE;;

}

/**
* gst_niimaqdxsrc_stop:
* src: #GstBaseSrc instance
*
* Close resources opened by gst_niimaqdxsrc_start
*
* Returns: TRUE on success
*/
static gboolean
gst_niimaqdxsrc_stop (GstBaseSrc * src)
{
  GstNiImaqDxSrc *niimaqdxsrc = GST_NIIMAQDXSRC (src);
  IMAQdxError rval;
  gboolean result = TRUE;

  /* stop IMAQ session */
  if (niimaqdxsrc->session_started) {
    rval = IMAQdxStopAcquisition (niimaqdxsrc->session);
    if (rval != IMAQdxErrorSuccess) {
      gst_niimaqdxsrc_report_imaq_error (rval);
      GST_ELEMENT_ERROR (niimaqdxsrc, RESOURCE, FAILED,
          ("Unable to stop acquisition"), (NULL));
      result = FALSE;
    }
    niimaqdxsrc->session_started = FALSE;
    GST_DEBUG_OBJECT (niimaqdxsrc, "Acquisition stopped");
  }

  result &= gst_niimaqdxsrc_close_interface (niimaqdxsrc);

  gst_niimaqdxsrc_reset (niimaqdxsrc);

  return result;
}

static gboolean
gst_niimaqdxsrc_query (GstBaseSrc * src, GstQuery * query)
{
  GstNiImaqDxSrc *niimaqdxsrc = GST_NIIMAQDXSRC (src);
  gboolean res;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:{
      if (!niimaqdxsrc->session_started) {
        GST_WARNING_OBJECT (niimaqdxsrc,
            "Can't give latency since device isn't open!");
        res = FALSE;
      } else {
        GstClockTime min_latency, max_latency;
        /* TODO: this is a ballpark figure, estimate from FVAL times */
        min_latency = 33 * GST_MSECOND;
        max_latency = 33 * GST_MSECOND * niimaqdxsrc->ringbuffer_count;

        GST_LOG_OBJECT (niimaqdxsrc,
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
* gst_niimaqdxsrc_close_interface:
* niimaqdxsrc: #GstNiImaqDxSrc instance
*
* Close IMAQ session and interface
*
*/
static gboolean
gst_niimaqdxsrc_close_interface (GstNiImaqDxSrc * niimaqdxsrc)
{
  IMAQdxError rval;
  gboolean result = TRUE;

  /* close IMAQ session and interface */
  if (niimaqdxsrc->session) {
    rval = IMAQdxCloseCamera (niimaqdxsrc->session);
    if (rval != IMAQdxErrorSuccess) {
      gst_niimaqdxsrc_report_imaq_error (rval);
      result = FALSE;
    } else
      GST_LOG_OBJECT (niimaqdxsrc, "IMAQdx session closed");
    niimaqdxsrc->session = 0;
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
  GST_DEBUG_CATEGORY_INIT (niimaqdxsrc_debug, "niimaqdxsrc", 0,
      "NI-IMAQdx interface");

  /* we only have one element in this plugin */
  return gst_element_register (plugin, "niimaqdxsrc", GST_RANK_NONE,
      GST_TYPE_NIIMAQDXSRC);

}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, "niimaqdx",
    "NI-IMAQdx source element", plugin_init, VERSION, GST_LICENSE, PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
