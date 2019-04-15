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

/* TODO: Firewire cameras that have an ROI less than the full frame will be
         corrupted, the only fix is to use NI Vision library */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstniimaqdx.h"

#include <time.h>
#include <string.h>

#include <gst/video/video.h>

GST_DEBUG_CATEGORY (niimaqdxsrc_debug);
#define GST_CAT_DEFAULT niimaqdxsrc_debug

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_RING_BUFFER_COUNT,
  PROP_ATTRIBUTES,
  PROP_BAYER_AS_GRAY,
  PROP_IS_CONTROLLER
};

#define DEFAULT_PROP_DEVICE "cam0"
#define DEFAULT_PROP_RING_BUFFER_COUNT 3
#define DEFAULT_PROP_ATTRIBUTES ""
#define DEFAULT_PROP_BAYER_AS_GRAY FALSE
#define DEFAULT_PROP_IS_CONTROLLER TRUE

static void gst_niimaqdxsrc_init_interfaces (GType type);

G_DEFINE_TYPE (GstNiImaqDxSrc, gst_niimaqdxsrc, GST_TYPE_PUSH_SRC);

/* GObject virtual methods */
static void gst_niimaqdxsrc_dispose (GObject * object);
static void gst_niimaqdxsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_niimaqdxsrc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

/* GstBaseSrc virtual methods */
static gboolean gst_niimaqdxsrc_start (GstBaseSrc * src);
static gboolean gst_niimaqdxsrc_stop (GstBaseSrc * src);
static gboolean gst_niimaqdxsrc_query (GstBaseSrc * src, GstQuery * query);
static GstCaps *gst_niimaqdxsrc_get_caps (GstBaseSrc * bsrc,
    GstCaps * caps_filter);
static gboolean gst_niimaqdxsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps);

/* GstPushSrc virtual methods */
static GstFlowReturn gst_niimaqdxsrc_fill (GstPushSrc * src, GstBuffer * buf);

/* GstNiImaqDx methods */
static GstCaps *gst_niimaqdxsrc_get_cam_caps (GstNiImaqDxSrc * src);
static gboolean gst_niimaqdxsrc_close_interface (GstNiImaqDxSrc * src);
static void gst_niimaqdxsrc_reset (GstNiImaqDxSrc * src);
static void gst_niimaqdxsrc_set_dx_attributes (GstNiImaqDxSrc * src);

IMAQdxError
gst_niimaqdxsrc_report_imaq_error (IMAQdxError code)
{
  static char imaqdx_error_string[IMAQDX_MAX_API_STRING_LENGTH];
  if (code) {
    IMAQdxGetErrorString (code, imaqdx_error_string,
        IMAQDX_MAX_API_STRING_LENGTH);
    GST_ERROR ("IMAQdx error %d: %s", code, imaqdx_error_string);
  }
  return code;
}

typedef struct _GstNiImaqDxSrcTimeEntry GstNiImaqDxSrcTimeEntry;
struct _GstNiImaqDxSrcTimeEntry
{
  guint64 frame_index;
  GstClockTime clock_time;
};

/* This will be called "when a frame done event occurs", so not start of frame */
uInt32 NI_FUNC
gst_niimaqdxsrc_frame_done_callback (IMAQdxSession session, uInt32 bufferNumber,
    void *userdata)
{
  GstNiImaqDxSrc *src = GST_NIIMAQDXSRC (userdata);
  GstNiImaqDxSrcTimeEntry *time_entry;

  time_entry = g_new (GstNiImaqDxSrcTimeEntry, 1);

  /* get clock time */
  time_entry->clock_time =
      gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src)));
  time_entry->frame_index = bufferNumber;

  g_async_queue_push (src->time_queue, time_entry);

  /* return 1 to rearm the callback */
  return 1;
}

#define VIDEO_CAPS_MAKE_BAYER8(format)                     \
   "video/x-bayer, "                                       \
  "format = (string) { " format " }, "                     \
  "width = " GST_VIDEO_SIZE_RANGE ", "                     \
  "height = " GST_VIDEO_SIZE_RANGE ", "                    \
  "framerate = " GST_VIDEO_FPS_RANGE

#define VIDEO_CAPS_MAKE_BAYER16(format,endianness)         \
  "video/x-bayer, "                                        \
  "format = (string) { " format " }, "                     \
  "endianness = (int) { " endianness " }, "                \
  "bpp = (int) {16, 14, 12, 10}, "                         \
  "width = " GST_VIDEO_SIZE_RANGE ", "                     \
  "height = " GST_VIDEO_SIZE_RANGE ", "                    \
  "framerate = " GST_VIDEO_FPS_RANGE

/* TODO: handle the format mappings more intelligently */
ImaqDxCapsInfo imaq_dx_caps_infos[] = {
  {"Mono8", 0, GST_VIDEO_CAPS_MAKE ("GRAY8"), 8, 8, 4}
  ,
  {"Mono10", G_LITTLE_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_LE"), 10, 16, 4}
  ,
  {"Mono10", G_BIG_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_BE"), 10, 16, 4}
  ,
  {"Mono12", G_LITTLE_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_LE"), 12, 16, 4}
  ,
  {"Mono12", G_BIG_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_BE"), 12, 16, 4}
  ,
  {"Mono14", G_LITTLE_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_LE"), 14, 16, 4}
  ,
  {"Mono14", G_BIG_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_BE"), 14, 16, 4}
  ,
  {"Mono16", G_LITTLE_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_LE"), 16, 16, 4}
  ,
  {"Mono16", G_BIG_ENDIAN, GST_VIDEO_CAPS_MAKE ("GRAY16_BE"), 16, 16, 4}
  ,
  {"RGB8", 0, GST_VIDEO_CAPS_MAKE ("RGB"), 24, 24, 4}
  ,
  {"BGR8", 0, GST_VIDEO_CAPS_MAKE ("BGR"), 24, 24, 4}
  ,
  {"RGBa8", 0, GST_VIDEO_CAPS_MAKE ("RGBA"), 32, 32, 4}
  ,
  {"BGRa8", 0, GST_VIDEO_CAPS_MAKE ("BGRA"), 32, 32, 4}
  ,
  {"BGRA8Packed", 0, GST_VIDEO_CAPS_MAKE ("BGRA"), 32, 32, 4}
  ,
  {"YUV411_8_UYYVYY", 0, GST_VIDEO_CAPS_MAKE ("IYU1"), 16, 16, 4}
  ,                             /* deprecated by YUV411_8_UYYVYY */
  {"YUV411Packed", 0, GST_VIDEO_CAPS_MAKE ("IYU1"), 16, 16, 4}
  ,
  {"YUV422_8_UYVY", 0, GST_VIDEO_CAPS_MAKE ("UYVY"), 16, 16, 4}
  ,                             /* deprecated by YUV422_8_UYVY */
  {"YUV422Packed", 0, GST_VIDEO_CAPS_MAKE ("UYVY"), 16, 16, 4}
  ,
  {"YUV8_UYV", 0, GST_VIDEO_CAPS_MAKE ("v308"), 16, 16, 4}
  ,                             /* deprecated by YUV8_UYV */
  {"YUV444Packed", 0, GST_VIDEO_CAPS_MAKE ("v308"), 16, 16, 4}
  ,
  {"BayerBG8", 0, VIDEO_CAPS_MAKE_BAYER8 ("bggr"), 8, 8, 1}
  ,
  {"BayerGR8", 0, VIDEO_CAPS_MAKE_BAYER8 ("grbg"), 8, 8, 1}
  ,
  {"BayerRG8", 0, VIDEO_CAPS_MAKE_BAYER8 ("rggb"), 8, 8, 1}
  ,
  {"BayerGB8", 0, VIDEO_CAPS_MAKE_BAYER8 ("gbrg"), 8, 8, 1}
  ,
  {"BayerBG10", 0, VIDEO_CAPS_MAKE_BAYER16 ("bggr16", "1234"), 10, 16, 1}
  ,
  {"BayerGR10", 0, VIDEO_CAPS_MAKE_BAYER16 ("grbg16", "1234"), 10, 16, 1}
  ,
  {"BayerRG10", 0, VIDEO_CAPS_MAKE_BAYER16 ("rggb16", "1234"), 10, 16, 1}
  ,
  {"BayerGB10", 0, VIDEO_CAPS_MAKE_BAYER16 ("gbrg16", "1234"), 10, 16, 1}
  ,
  {"BayerBG12", 0, VIDEO_CAPS_MAKE_BAYER16 ("bggr16", "1234"), 12, 16, 1}
  ,
  {"BayerGR12", 0, VIDEO_CAPS_MAKE_BAYER16 ("grbg16", "1234"), 12, 16, 1}
  ,
  {"BayerRG12", 0, VIDEO_CAPS_MAKE_BAYER16 ("rggb16", "1234"), 12, 16, 1}
  ,
  {"BayerGB12", 0, VIDEO_CAPS_MAKE_BAYER16 ("gbrg16", "1234"), 12, 16, 1}
  ,
  //TODO: use a caps string that agrees with Aravis
  {"BayerBG16", 0, VIDEO_CAPS_MAKE_BAYER16 ("bggr16", "1234"), 16, 16, 1}
  ,
  {"BayerGR16", 0, VIDEO_CAPS_MAKE_BAYER16 ("grbg16", "1234"), 16, 16, 1}
  ,
  {"BayerRG16", 0, VIDEO_CAPS_MAKE_BAYER16 ("rggb16", "1234"), 16, 16, 1}
  ,
  {"BayerGB16", 0, VIDEO_CAPS_MAKE_BAYER16 ("gbrg16", "1234"), 16, 16, 1}
  ,
  {"JPEG", 0, "image/jpeg", 8, 8, 1}
};

static const ImaqDxCapsInfo *
gst_niimaqdxsrc_get_caps_info (const char *pixel_format, int endianness)
{
  int i;
  /* some cameras include spaces in pixel format names, so remove them */
  char **split = g_strsplit (pixel_format, " ", -1);
  char *pix_fmt = g_strjoinv (NULL, split);
  g_strfreev (split);

  for (i = 0; i < G_N_ELEMENTS (imaq_dx_caps_infos); i++) {
    ImaqDxCapsInfo *info = &imaq_dx_caps_infos[i];
    if (g_strcmp0 (pix_fmt, info->pixel_format) == 0 &&
        (info->endianness == endianness || info->endianness == 0)) {
      g_free (pix_fmt);
      return info;
    }
  }

  g_free (pix_fmt);

  GST_WARNING ("PixelFormat '%s' is not supported", pixel_format);

  return NULL;
}

static const char *
gst_niimaqdxsrc_pixel_format_to_caps_string (const char *pixel_format,
    int endianness)
{
  const ImaqDxCapsInfo *info =
      gst_niimaqdxsrc_get_caps_info (pixel_format, endianness);

  if (!info)
    return NULL;

  return info->gst_caps_string;
}

static const char *
gst_niimaqdxsrc_pixel_format_from_caps (const GstCaps * caps, int *endianness)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (imaq_dx_caps_infos); i++) {
    GstCaps *super_caps;
    super_caps = gst_caps_from_string (imaq_dx_caps_infos[i].gst_caps_string);
    if (gst_caps_is_subset (caps, super_caps)) {
      *endianness = imaq_dx_caps_infos[i].endianness;
      return imaq_dx_caps_infos[i].pixel_format;
    }
  }

  return NULL;
}

static int
gst_niimaqdxsrc_pixel_format_get_depth (const char *pixel_format,
    int endianness)
{
  const ImaqDxCapsInfo *info =
      gst_niimaqdxsrc_get_caps_info (pixel_format, endianness);

  if (!info)
    return 0;

  return info->depth;
}

static int
gst_niimaqdxsrc_pixel_format_get_stride (const char *pixel_format,
    int endianness, int width)
{
  return width * gst_niimaqdxsrc_pixel_format_get_depth (pixel_format,
      endianness) / 8;
}

static GstCaps *
gst_niimaqdxsrc_new_caps_from_pixel_format (const char *pixel_format,
    int endianness, int width, int height, int framerate_n, int framerate_d,
    int par_n, int par_d)
{
  const char *caps_string;
  GstCaps *caps;
  GstStructure *structure;

  GST_DEBUG
      ("Trying to create caps from: %s, endianness=%d, %dx%d, fps=%d/%d, par=%d/%d",
      pixel_format, endianness, width, height, framerate_n, framerate_d, par_n,
      par_d);

  caps_string =
      gst_niimaqdxsrc_pixel_format_to_caps_string (pixel_format, endianness);
  if (caps_string == NULL)
    return NULL;

  GST_DEBUG ("Got caps string: %s", caps_string);

  structure = gst_structure_from_string (caps_string, NULL);
  if (structure == NULL)
    return NULL;

  gst_structure_set (structure,
      "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height,
      "framerate", GST_TYPE_FRACTION, framerate_n, framerate_d,
      "pixel-aspect-ratio", GST_TYPE_FRACTION, par_n, par_d, NULL);

  caps = gst_caps_new_empty ();
  gst_caps_append_structure (caps, structure);

  return caps;
}


static void _____BEGIN_FUNCTIONS_____ ();

static gboolean _imaqdx_init = FALSE;
static GList *_imaqdx_devices = NULL;

#if 0
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
#endif

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
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

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
          "Attributes", "Attributes to change, comma separated key=value pairs",
          DEFAULT_PROP_ATTRIBUTES, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BAYER_AS_GRAY,
      g_param_spec_boolean ("bayer-as-gray", "Bayer as gray",
          "For Bayer sources use GRAY caps", DEFAULT_PROP_BAYER_AS_GRAY,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_IS_CONTROLLER,
      g_param_spec_boolean ("is-controller", "Open as controller",
          "True for controller mode, false for listener mode",
          DEFAULT_PROP_IS_CONTROLLER,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  {
    GstCaps *caps = gst_caps_new_empty ();
    int i;

    for (i = 0; i < G_N_ELEMENTS (imaq_dx_caps_infos); i++) {
      ImaqDxCapsInfo *info = &imaq_dx_caps_infos[i];
      gst_caps_merge (caps, gst_caps_from_string (info->gst_caps_string));
    }
    caps = gst_caps_simplify (caps);
    gst_element_class_add_pad_template (gstelement_class,
        gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
  }

  gst_element_class_set_static_metadata (gstelement_class,
      "NI-IMAQdx Video Source", "Source/Video",
      "National Instruments IMAQdx source, supports FireWire, USB, and GigE Vision cameras",
      "Joshua M. Doe <oss@nvl.army.mil>");

  /* install GstBaseSrc vmethod implementations */
  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_niimaqdxsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_niimaqdxsrc_stop);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_niimaqdxsrc_query);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_niimaqdxsrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_niimaqdxsrc_set_caps);

  /* install GstPushSrc vmethod implementations */
  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_niimaqdxsrc_fill);
}

/**
* gst_niimaqdxsrc_init:
* src: the #GstNiImaqDx instance to initialize
* g_class: #GstNiImaqDxClass
*
* Initialize this instance of #GstNiImaqDx
*/
static void
gst_niimaqdxsrc_init (GstNiImaqDxSrc * src)
{
  GstPad *srcpad = GST_BASE_SRC_PAD (src);

  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize properties */
  src->ringbuffer_count = DEFAULT_PROP_RING_BUFFER_COUNT;
  src->device_name = g_strdup (DEFAULT_PROP_DEVICE);
  src->attributes = g_strdup (DEFAULT_PROP_ATTRIBUTES);
  src->bayer_as_gray = DEFAULT_PROP_BAYER_AS_GRAY;
  src->is_controller = DEFAULT_PROP_IS_CONTROLLER;

  /* initialize pointers, then call reset to initialize the rest */
  src->temp_buffer = NULL;
  src->time_queue = NULL;
  gst_niimaqdxsrc_reset (src);
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
  GstNiImaqDxSrc *src = GST_NIIMAQDXSRC (object);

  gst_niimaqdxsrc_close_interface (src);

  /* free memory allocated */
  g_free (src->device_name);
  src->device_name = NULL;

  /* unref objects */
  g_async_queue_unref (src->time_queue);

  /* chain dispose fuction of parent class */
  G_OBJECT_CLASS (gst_niimaqdxsrc_parent_class)->dispose (object);
}

static void
gst_niimaqdxsrc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNiImaqDxSrc *src = GST_NIIMAQDXSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      if (src->device_name)
        g_free (src->device_name);
      src->device_name = g_strdup (g_value_get_string (value));
      break;
    case PROP_RING_BUFFER_COUNT:
      src->ringbuffer_count = g_value_get_int (value);
      break;
    case PROP_ATTRIBUTES:
      if (src->attributes)
        g_free (src->attributes);
      src->attributes = g_strdup (g_value_get_string (value));
      break;
    case PROP_BAYER_AS_GRAY:
      src->bayer_as_gray = g_value_get_boolean (value);
      break;
    case PROP_IS_CONTROLLER:
      src->is_controller = g_value_get_boolean (value);
      break;
    default:
      break;
  }
}

static void
gst_niimaqdxsrc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNiImaqDxSrc *src = GST_NIIMAQDXSRC (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, src->device_name);
      break;
    case PROP_RING_BUFFER_COUNT:
      g_value_set_int (value, src->ringbuffer_count);
      break;
    case PROP_ATTRIBUTES:
      g_value_set_string (value, src->attributes);
      break;
    case PROP_BAYER_AS_GRAY:
      g_value_set_boolean (value, src->bayer_as_gray);
      break;
    case PROP_IS_CONTROLLER:
      g_value_set_boolean (value, src->is_controller);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_niimaqdxsrc_reset (GstNiImaqDxSrc * src)
{
  GST_LOG_OBJECT (src, "Resetting instance");

  /* initialize member variables */
  src->cumbufnum = 0;
  src->n_dropped_frames = 0;
  src->session = 0;
  src->session_started = FALSE;
  src->width = 0;
  src->height = 0;
  src->dx_row_stride = 0;
  src->dx_framesize = 0;
  src->gst_row_stride = 0;
  src->gst_framesize = 0;
  src->caps_info = NULL;
  src->pixel_format[0] = 0;

  g_free (src->temp_buffer);
  src->temp_buffer = NULL;

  if (src->time_queue) {
    g_async_queue_unref (src->time_queue);
  }
  src->time_queue = g_async_queue_new ();
}

static gboolean
gst_niimaqdxsrc_start_acquisition (GstNiImaqDxSrc * src)
{
  int i;
  IMAQdxError rval;

  g_assert (!src->session_started);

  GST_DEBUG_OBJECT (src, "Starting acquisition");

  /* try to open the camera five times */
  for (i = 0; i < 5; i++) {
    rval = IMAQdxStartAcquisition (src->session);
    if (rval == IMAQdxErrorSuccess) {
      src->session_started = TRUE;
      return TRUE;
    } else {
      gst_niimaqdxsrc_report_imaq_error (rval);
      GST_LOG_OBJECT (src, "camera is still off , wait 50ms and retry");
      g_usleep (50000);
    }
  }

  /* we tried five times and failed, so we error */
  gst_niimaqdxsrc_close_interface (src);

  return FALSE;
}

#define ROUND_UP_N(num, n)  (((num)+((n)-1))&~((n)-1))

static GstFlowReturn
gst_niimaqdxsrc_fill (GstPushSrc * psrc, GstBuffer * buf)
{
  GstNiImaqDxSrc *src = GST_NIIMAQDXSRC (psrc);
  GstClockTime timestamp = GST_CLOCK_TIME_NONE;
  GstClockTime duration;
  uInt32 copied_number;
  IMAQdxError rval;
  uInt32 dropped;
  gboolean do_align_stride;
  GstMapInfo minfo;

  /* start the IMAQ acquisition session if we haven't done so yet */
  if (!src->session_started) {
    if (!gst_niimaqdxsrc_start_acquisition (src)) {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Unable to start acquisition."), (NULL));
      return GST_FLOW_ERROR;
    }
  }

  /* will change attributes if provided */
  gst_niimaqdxsrc_set_dx_attributes (src);

  if (src->caps_info == NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to create caps, possibly unsupported format (%s).",
            src->pixel_format), (NULL));
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (src, "Copying IMAQ buffer #%d, buffersize %d",
      src->cumbufnum, gst_buffer_get_size (buf));

  do_align_stride = src->dx_row_stride != src->gst_row_stride;

  if (!do_align_stride) {
    gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
    // we have properly aligned strides, copy directly to buffer
    rval = IMAQdxGetImageData (src->session, minfo.data,
        minfo.size, IMAQdxBufferNumberModeBufferNumber,
        src->cumbufnum, &copied_number);
    gst_buffer_unmap (buf, &minfo);
  } else {
    // we don't have aligned strides, copy to temp buffer
    rval = IMAQdxGetImageData (src->session, src->temp_buffer,
        src->dx_framesize, IMAQdxBufferNumberModeBufferNumber,
        src->cumbufnum, &copied_number);
  }

  if (rval) {
    gst_niimaqdxsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("failed to copy buffer %d", src->cumbufnum), (NULL));
    return GST_FLOW_ERROR;
  }

  if (src->is_jpeg) {
    /* JPEG sources don't seem to give reliable callbacks, just pull clock */
    timestamp = gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src)));
  }

  while (timestamp == GST_CLOCK_TIME_NONE) {
    /* wait 100 ms, shouldn't be needed if callback is working as expected */
    GstNiImaqDxSrcTimeEntry *entry =
        (GstNiImaqDxSrcTimeEntry *) g_async_queue_timeout_pop (src->time_queue,
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

  // adjust for row stride if needed (must be multiple of 4)
  if (do_align_stride) {
    int i;
    guint8 *tmpbuf = src->temp_buffer;
    guint8 *dst;

    gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
    dst = minfo.data;
    GST_LOG_OBJECT (src,
        "Row stride not aligned, copying %d -> %d",
        src->dx_row_stride, src->gst_row_stride);
    g_assert (minfo.size >= src->gst_framesize);
    for (i = 0; i < src->height; i++)
      memcpy (dst + i * src->gst_row_stride, tmpbuf + i * src->dx_row_stride,
          src->dx_row_stride);
    gst_buffer_unmap (buf, &minfo);
  }

  /* make guess of duration from timestamp and cumulative buffer number */
  if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
    duration = timestamp / (copied_number + 1);
  } else {
    duration = 33 * GST_MSECOND;
  }

  GST_BUFFER_OFFSET (buf) = copied_number;
  GST_BUFFER_OFFSET_END (buf) = copied_number + 1;
  GST_BUFFER_TIMESTAMP (buf) =
      timestamp - gst_element_get_base_time (GST_ELEMENT (src));
  // TODO: fix duration
  //GST_BUFFER_DURATION (buf) = duration;

  dropped = copied_number - src->cumbufnum;
  if (dropped > 0) {
    GstStructure *infoStruct;

    src->n_dropped_frames += dropped;
    GST_WARNING_OBJECT (src,
        "Asked to copy buffer #%d but was given #%d; just dropped %d frames (%d total)",
        src->cumbufnum, copied_number, dropped, src->n_dropped_frames);

    infoStruct = gst_structure_new ("dropped-frame-info",
        "num-dropped-frames", G_TYPE_INT, dropped,
        "total-dropped-frames", G_TYPE_INT, src->n_dropped_frames,
        "timestamp", GST_TYPE_CLOCK_TIME, GST_BUFFER_TIMESTAMP (buf), NULL);
    gst_element_post_message (GST_ELEMENT (src),
        gst_message_new_element (GST_OBJECT (src), infoStruct));
  }

  /* set cumulative buffer number to get next frame */
  src->cumbufnum = copied_number + 1;

  return GST_FLOW_OK;
}

void
gst_niimaqdxsrc_list_attributes (GstNiImaqDxSrc * src)
{
  IMAQdxAttributeInformation *attributeInfoArray = NULL;
  uInt32 attributeCount;
  guint i;
  IMAQdxError rval;
  IMAQdxSession session = src->session;
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
  attributeInfoArray = g_new (IMAQdxAttributeInformation, attributeCount);
  rval =
      IMAQdxEnumerateAttributes2 (session, attributeInfoArray, &attributeCount,
      "", IMAQdxAttributeVisibilityAdvanced);
  GST_DEBUG_OBJECT (src, "Enumerating %d attributes", attributeCount);
  for (i = 0; i < attributeCount; i++) {
    IMAQdxAttributeInformation *info = attributeInfoArray + i;
    g_assert (info);

    if (info->Readable) {
      rval =
          IMAQdxGetAttribute (session, info->Name, IMAQdxValueTypeString,
          attributeString);
      if (rval != IMAQdxErrorSuccess) {
        GST_WARNING_OBJECT (src,
            "Failed to read value of attribute %s", info->Name);
        continue;
      }
    } else
      attributeString[0] = 0;

    GST_LOG_OBJECT (src, "%s, %s/%s, %s, %s\n",
        info->Name, info->Readable ? "R" : "-",
        info->Writable ? "W" : "-",
        attributeTypeStrings[info->Type], attributeString);
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
gst_niimaqdxsrc_get_cam_caps (GstNiImaqDxSrc * src)
{
  GstCaps *caps = NULL;
  IMAQdxError rval;
  uInt32 val;
  char pixel_format[IMAQDX_MAX_API_STRING_LENGTH];
  int endianness;
  IMAQdxBusType bus_type;
  gint width, height;

  if (!src->session) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Camera not open"), (NULL));
    goto error;
  }

  GST_LOG_OBJECT (src, "Retrieving attributes from IMAQdx device");

  rval = IMAQdxGetAttribute (src->session, IMAQdxAttributePixelFormat,
      IMAQdxValueTypeString, &pixel_format);
  gst_niimaqdxsrc_report_imaq_error (rval);
  rval &= IMAQdxGetAttribute (src->session, IMAQdxAttributeBusType,
      IMAQdxValueTypeU32, &val);
  bus_type = (IMAQdxBusType) val;
  gst_niimaqdxsrc_report_imaq_error (rval);
  rval &= IMAQdxGetAttribute (src->session, IMAQdxAttributeWidth,
      IMAQdxValueTypeU32, &val);
  gst_niimaqdxsrc_report_imaq_error (rval);
  width = val;
  rval &= IMAQdxGetAttribute (src->session, IMAQdxAttributeHeight,
      IMAQdxValueTypeU32, &val);
  gst_niimaqdxsrc_report_imaq_error (rval);
  height = val;

  if (rval) {
    GST_ELEMENT_ERROR (src, STREAM, FAILED,
        ("attempt to read attributes failed"),
        ("attempt to read attributes failed"));
    goto error;
  }

  g_strlcpy (src->pixel_format, pixel_format, IMAQDX_MAX_API_STRING_LENGTH);

  /* confirmed FireWire is big-endian, GigE and USB3 are little-endian */
  if (bus_type == IMAQdxBusTypeEthernet || bus_type == IMAQdxBusTypeUSB3Vision) {
    endianness = G_LITTLE_ENDIAN;
  } else {
    endianness = G_BIG_ENDIAN;
  }

  GST_DEBUG_OBJECT (src, "Camera has pixel format '%s'", pixel_format);

  if (g_str_has_prefix (pixel_format, "Bayer") && src->bayer_as_gray) {
    const ImaqDxCapsInfo *info =
        gst_niimaqdxsrc_get_caps_info (pixel_format, endianness);
    if (info->depth == 8) {
      g_strlcpy (pixel_format, "Mono 8", IMAQDX_MAX_API_STRING_LENGTH);
    } else if (info->depth == 16) {
      g_strlcpy (pixel_format, "Mono 16", IMAQDX_MAX_API_STRING_LENGTH);
    }
  }
  //TODO: add all available caps by enumerating PixelFormat's available, and query for framerate
  caps =
      gst_niimaqdxsrc_new_caps_from_pixel_format (pixel_format, endianness,
      width, height, 30, 1, 1, 1);
  if (!caps) {
    GST_ERROR_OBJECT (src, "PixelFormat '%s' not supported yet", pixel_format);
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

static void
gst_niimaqdxsrc_set_dx_attributes (GstNiImaqDxSrc * src)
{
  gchar **pairs;
  int i;
  IMAQdxError rval;

  if (!src->attributes || src->attributes == 0) {
    return;
  }

  GST_DEBUG_OBJECT (src, "Trying to set following attributes: '%s'",
      src->attributes);

  pairs = g_strsplit (src->attributes, ";", 0);

  for (i = 0;; i++) {
    gchar **pair;

    if (!pairs[i])
      break;

    pair = g_strsplit (pairs[i], "=", 2);

    if (!pair[0] || !pair[1]) {
      GST_WARNING_OBJECT (src, "Failed to parse attribute/value: '%s'", pair);
      continue;
    }

    GST_DEBUG_OBJECT (src, "Setting attribute, '%s'='%s'", pair[0], pair[1]);

    rval =
        IMAQdxSetAttribute (src->session, pair[0],
        IMAQdxValueTypeString, (const char *) pair[1]);
    if (rval != IMAQdxErrorSuccess) {
      gst_niimaqdxsrc_report_imaq_error (rval);
    }
    g_strfreev (pair);
  }
  g_strfreev (pairs);

  if (src->attributes) {
    g_free (src->attributes);
    src->attributes = NULL;
  }
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
gst_niimaqdxsrc_start (GstBaseSrc * bsrc)
{
  GstNiImaqDxSrc *src = GST_NIIMAQDXSRC (bsrc);
  IMAQdxError rval;
  IMAQdxCameraControlMode control_mode;

  gst_niimaqdxsrc_reset (src);

  if (src->is_controller) {
    control_mode = IMAQdxCameraControlModeController;
    GST_LOG_OBJECT (src, "Opening IMAQdx interface '%s' in controller mode",
        src->device_name);
  } else {
    control_mode = IMAQdxCameraControlModeListener;
    GST_LOG_OBJECT (src, "Opening IMAQxd interface '%s' in listener mode",
        src->device_name);
  }

  /* open IMAQ interface */
  rval = IMAQdxOpenCamera (src->device_name, control_mode, &src->session);
  if (rval != IMAQdxErrorSuccess) {
    gst_niimaqdxsrc_report_imaq_error (rval);
    GST_WARNING_OBJECT (src, "Failed to open camera '%s', will try resetting.",
        src->device_name);

    rval = IMAQdxResetCamera (src->device_name, FALSE);
    rval = IMAQdxOpenCamera (src->device_name, control_mode, &src->session);
    if (rval != IMAQdxErrorSuccess) {
      gst_niimaqdxsrc_report_imaq_error (rval);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to open IMAQdx interface"),
          ("Failed to open camera interface %s", src->device_name));
      goto error;
    }
  }

  gst_niimaqdxsrc_list_attributes (src);

  GST_LOG_OBJECT (src, "Creating ring with %d buffers", src->ringbuffer_count);

  rval = IMAQdxConfigureAcquisition (src->session, TRUE, src->ringbuffer_count);
  if (rval) {
    gst_niimaqdxsrc_report_imaq_error (rval);
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to create ring buffer"),
        ("Failed to create ring buffer with %d buffers",
            src->ringbuffer_count));
    goto error;
  }

  if (src->is_jpeg) {
    GST_DEBUG_OBJECT (src, "Source is JPEG, just use clock time");
  } else {
    GST_LOG_OBJECT (src, "Registering callback functions");
    rval =
        IMAQdxRegisterFrameDoneEvent (src->session, 1,
        gst_niimaqdxsrc_frame_done_callback, src);
    if (rval) {
      gst_niimaqdxsrc_report_imaq_error (rval);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to register callback(s)"), (NULL));
      goto error;
    }
  }

  gst_niimaqdxsrc_set_dx_attributes (src);

  return TRUE;

error:
  gst_niimaqdxsrc_close_interface (src);

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
gst_niimaqdxsrc_stop (GstBaseSrc * bsrc)
{
  GstNiImaqDxSrc *src = GST_NIIMAQDXSRC (bsrc);
  IMAQdxError rval;
  gboolean result = TRUE;

  /* stop IMAQ session */
  if (src->session_started) {
    rval = IMAQdxStopAcquisition (src->session);
    if (rval != IMAQdxErrorSuccess) {
      gst_niimaqdxsrc_report_imaq_error (rval);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Unable to stop acquisition"), (NULL));
      result = FALSE;
    }
    src->session_started = FALSE;
    GST_DEBUG_OBJECT (src, "Acquisition stopped");
  }

  result &= gst_niimaqdxsrc_close_interface (src);

  gst_niimaqdxsrc_reset (src);

  return result;
}

static gboolean
gst_niimaqdxsrc_query (GstBaseSrc * bsrc, GstQuery * query)
{
  GstNiImaqDxSrc *src = GST_NIIMAQDXSRC (bsrc);
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
        max_latency = 33 * GST_MSECOND * src->ringbuffer_count;

        GST_LOG_OBJECT (src,
            "report latency min %" GST_TIME_FORMAT " max %" GST_TIME_FORMAT,
            GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

        gst_query_set_latency (query, TRUE, min_latency, max_latency);

        res = TRUE;
      }
    }
    default:
      res =
          GST_BASE_SRC_CLASS (gst_niimaqdxsrc_parent_class)->query (bsrc,
          query);
      break;
  }

  return res;
}

static GstCaps *
gst_niimaqdxsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter_caps)
{
  GstNiImaqDxSrc *src = GST_NIIMAQDXSRC (bsrc);
  GstCaps *caps;

  if (!src->session) {
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  } else
    caps = gst_niimaqdxsrc_get_cam_caps (src);

  if (caps == NULL) {
    GST_ERROR_OBJECT (src, "Unable to create caps");
    return NULL;
  }

  GST_DEBUG_OBJECT (src, "get_caps, pre-filter=%" GST_PTR_FORMAT, caps);

  if (filter_caps) {
    GstCaps *tmp = gst_caps_intersect (caps, filter_caps);
    gst_caps_unref (caps);
    caps = tmp;
  }

  GST_DEBUG_OBJECT (src,
      "with filter %" GST_PTR_FORMAT ", post-filter=%" GST_PTR_FORMAT,
      filter_caps, caps);

  return caps;
}

static gboolean
gst_niimaqdxsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstNiImaqDxSrc *src = GST_NIIMAQDXSRC (bsrc);
  GstStructure *structure;
  const char *pixel_format;
  int endianness;

  GST_DEBUG_OBJECT (src, "set_caps with caps=%" GST_PTR_FORMAT, caps);

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_get_int (structure, "width", &src->width);
  gst_structure_get_int (structure, "height", &src->height);

  pixel_format = gst_niimaqdxsrc_pixel_format_from_caps (caps, &endianness);
  g_assert (pixel_format);

  src->caps_info = gst_niimaqdxsrc_get_caps_info (pixel_format, endianness);

  if (g_strcmp0 (pixel_format, "JPEG") == 0) {
    src->is_jpeg = TRUE;
  } else {
    src->is_jpeg = FALSE;
  }

  src->dx_row_stride =
      gst_niimaqdxsrc_pixel_format_get_stride (pixel_format, endianness,
      src->width);

  src->dx_framesize = src->dx_row_stride * src->height;

  src->gst_row_stride =
      ROUND_UP_N (src->dx_row_stride, src->caps_info->row_multiple);

  src->gst_framesize = src->gst_row_stride * src->height;

  /* TODO: don't use default_alloc, app can change blocksize */
  gst_base_src_set_blocksize (bsrc, src->gst_framesize);

  if (src->temp_buffer)
    g_free (src->temp_buffer);
  src->temp_buffer = g_malloc (src->dx_framesize);

  GST_DEBUG ("Size %dx%d", src->width, src->height);

  GST_LOG_OBJECT (src, "Caps set, framesize=%d", src->dx_framesize);

  return TRUE;
}

/**
* gst_niimaqdxsrc_close_interface:
* src: #GstNiImaqDxSrc instance
*
* Close IMAQ session and interface
*
*/
static gboolean
gst_niimaqdxsrc_close_interface (GstNiImaqDxSrc * src)
{
  IMAQdxError rval;
  gboolean result = TRUE;

  /* close IMAQ session and interface */
  if (src->session) {
    rval = IMAQdxCloseCamera (src->session);
    if (rval != IMAQdxErrorSuccess) {
      gst_niimaqdxsrc_report_imaq_error (rval);
      result = FALSE;
    } else
      GST_LOG_OBJECT (src, "IMAQdx session closed");
    src->session = 0;
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

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, niimaqdx,
    "NI-IMAQdx source element", plugin_init, GST_PACKAGE_VERSION,
    GST_PACKAGE_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
