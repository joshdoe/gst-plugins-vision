/* GStreamer
 * Copyright (C) 2011 FIXME <fixme@example.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstaptinasrc
 *
 * The aptinasrc element is a source for Aptina dev kits.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v aptinasrc ! videoconvert ! autovideosink
 * ]|
 * Shows video from the default Aptina dev kit
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#ifdef HAVE_ORC
#include <orc/orc.h>
#else
#define orc_memcpy memcpy
#endif

#include "gstaptinasrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_aptinasrc_debug);
#define GST_CAT_DEFAULT gst_aptinasrc_debug

/* prototypes */
static void gst_aptinasrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_aptinasrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_aptinasrc_dispose (GObject * object);
static void gst_aptinasrc_finalize (GObject * object);

static gboolean gst_aptinasrc_start (GstBaseSrc * src);
static gboolean gst_aptinasrc_stop (GstBaseSrc * src);
static GstCaps *gst_aptinasrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_aptinasrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_aptinasrc_unlock (GstBaseSrc * src);
static gboolean gst_aptinasrc_unlock_stop (GstBaseSrc * src);

static GstFlowReturn gst_aptinasrc_fill (GstPushSrc * src, GstBuffer * buf);

enum
{
  PROP_0,
  PROP_DEVICE_INDEX,
  PROP_CONFIG_FILE,
  PROP_CONFIG_PRESET,
};

#define DEFAULT_PROP_DEVICE_INDEX 0
#define DEFAULT_PROP_CONFIG_FILE ""
#define DEFAULT_PROP_CONFIG_PRESET ""

/* pad templates */

static GstStaticPadTemplate gst_aptinasrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ BGRx }"))
    );

/* class initialization */

G_DEFINE_TYPE (GstAptinaSrc, gst_aptinasrc, GST_TYPE_PUSH_SRC);

static void
gst_aptinasrc_class_init (GstAptinaSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_aptinasrc_set_property;
  gobject_class->get_property = gst_aptinasrc_get_property;
  gobject_class->dispose = gst_aptinasrc_dispose;
  gobject_class->finalize = gst_aptinasrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_aptinasrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Aptina Video Source", "Source/Video",
      "Aptina Imaging video source", "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_aptinasrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_aptinasrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_aptinasrc_get_caps);
  //gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_aptinasrc_set_caps);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_aptinasrc_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_aptinasrc_unlock_stop);

  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_aptinasrc_fill);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("device", "Device index",
          "Device index", 0, 254, DEFAULT_PROP_DEVICE_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CONFIG_FILE,
      g_param_spec_string ("config-file", "Config file",
          "Filepath of the INI file",
          DEFAULT_PROP_CONFIG_FILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_CONFIG_PRESET,
      g_param_spec_string ("config-preset", "Config preset",
          "Name of the preset in the INI file to run",
          DEFAULT_PROP_CONFIG_PRESET,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
}

static void
gst_aptinasrc_reset (GstAptinaSrc * src)
{
  src->raw_framesize = 0;
  src->rgb_framesize = 0;

  src->is_started = FALSE;

  src->last_frame_count = 0;
  src->total_dropped_frames = 0;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  if (src->buffer) {
    g_free (src->buffer);
    src->buffer = NULL;
  }
}

static void
gst_aptinasrc_init (GstAptinaSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->camera_index = DEFAULT_PROP_DEVICE_INDEX;
  src->config_file = g_strdup (DEFAULT_PROP_CONFIG_FILE);
  src->config_preset = g_strdup (DEFAULT_PROP_CONFIG_PRESET);

  src->apbase = NULL;
  src->stop_requested = FALSE;
  src->caps = NULL;
  src->buffer = NULL;

  gst_aptinasrc_reset (src);
}

void
gst_aptinasrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAptinaSrc *src;

  src = GST_APTINA_SRC (object);

  switch (property_id) {
    case PROP_DEVICE_INDEX:
      src->camera_index = g_value_get_int (value);
      break;
    case PROP_CONFIG_FILE:
      g_free (src->config_file);
      src->config_file = g_strdup (g_value_get_string (value));
      break;
    case PROP_CONFIG_PRESET:
      g_free (src->config_preset);
      src->config_preset = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_aptinasrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstAptinaSrc *src;

  g_return_if_fail (GST_IS_APTINA_SRC (object));
  src = GST_APTINA_SRC (object);

  switch (property_id) {
    case PROP_DEVICE_INDEX:
      g_value_set_int (value, src->camera_index);
      break;
    case PROP_CONFIG_FILE:
      g_value_set_string (value, src->config_file);
      break;
    case PROP_CONFIG_PRESET:
      g_value_set_string (value, src->config_preset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_aptinasrc_dispose (GObject * object)
{
  GstAptinaSrc *src;

  g_return_if_fail (GST_IS_APTINA_SRC (object));
  src = GST_APTINA_SRC (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_aptinasrc_parent_class)->dispose (object);
}

void
gst_aptinasrc_finalize (GObject * object)
{
  GstAptinaSrc *src;

  g_return_if_fail (GST_IS_APTINA_SRC (object));
  src = GST_APTINA_SRC (object);

  /* clean up object here */

  g_free (src->config_file);
  g_free (src->config_preset);

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  G_OBJECT_CLASS (gst_aptinasrc_parent_class)->finalize (object);
}

ap_s32
handle_error_message (void *pContext, const char *szMessage,
    unsigned int mbType)
{
  GstAptinaSrc *src = GST_APTINA_SRC (pContext);
  GST_DEBUG_OBJECT (src, "ApBase error: %s", szMessage);
  return 1;
}

static gboolean
gst_aptinasrc_calculate_caps (GstAptinaSrc * src)
{
  ap_u32 ret;
  ap_u32 rgb_width = 0, rgb_height = 0, rgb_depth = 0;
  guint8 *unpacked;
  GstVideoInfo vinfo;
  char image_type[64];
  gint framesize;

  framesize = ap_GrabFrame (src->apbase, NULL, 0);
  if (framesize != src->raw_framesize) {
    src->raw_framesize = framesize;
    if (src->buffer) {
      g_free (src->buffer);
    }
    src->buffer = (guint8 *) g_malloc (src->raw_framesize);
  }

  ret = ap_GrabFrame (src->apbase, src->buffer, src->raw_framesize);
  if (ret == 0) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Grabbing failed with error %d", ap_GetLastError ()), (NULL));
    return FALSE;
  }

  ap_GetImageFormat (src->apbase, NULL, NULL, image_type, sizeof (image_type));
  GST_DEBUG_OBJECT (src, "Image type is '%s', but will be converted to BGRx",
      image_type);

  unpacked =
      ap_ColorPipe (src->apbase, src->buffer, src->raw_framesize, &rgb_width,
      &rgb_height, &rgb_depth);
  if (rgb_depth != 32) {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Depth is %d, but only 32-bit supported currently.", rgb_depth),
        (NULL));
    return FALSE;
  }

  src->rgb_framesize = rgb_width * rgb_height * rgb_depth / 8;
  gst_base_src_set_blocksize (GST_BASE_SRC (src), src->rgb_framesize);

  gst_video_info_init (&vinfo);
  gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_BGRx, rgb_width,
      rgb_height);
  if (src->caps) {
    gst_caps_unref (src->caps);
  }
  src->caps = gst_video_info_to_caps (&vinfo);
  gst_base_src_set_caps (GST_BASE_SRC (src), src->caps);
  GST_DEBUG_OBJECT (src, "Created caps %" GST_PTR_FORMAT, src->caps);

  return TRUE;
}

static gboolean
gst_aptinasrc_start (GstBaseSrc * bsrc)
{
  GstAptinaSrc *src = GST_APTINA_SRC (bsrc);
  ap_s32 ret;
  gchar *ini_file, *preset;

  GST_DEBUG_OBJECT (src, "start");

  //src->apbase = ap_CreateFromImageFile("C:\\temp\\1.jpg");
  src->apbase = ap_Create (src->camera_index);
  if (!src->apbase) {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        ("Failed to open camera"), (NULL));
    return FALSE;
  }

  ap_SetCallback_ErrorMessage (src->apbase, handle_error_message, src);

  //ap_SetState(src->apbase, "Sensor Reset", 1);
  //g_usleep(1000000);
  //ap_SetState(src->apbase, "Sensor Reset", 0);
  //g_usleep(1000000);

  if (!src->config_file || strlen (src->config_file) == 0) {
    ini_file = NULL;
  } else {
    if (!g_file_test (src->config_file, G_FILE_TEST_EXISTS)) {
      GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
          ("Camera file does not exist: %s", src->config_file), (NULL));
      return FALSE;
    }
    ini_file = g_strdup (src->config_file);
  }

  if (!src->config_preset || strlen (src->config_preset) == 0) {
    preset = NULL;
  } else {
    preset = g_strdup (src->config_preset);
  }

  ret = ap_LoadIniPreset (src->apbase, ini_file, preset);
  g_free (ini_file);
  g_free (preset);

  if (ret != AP_INI_SUCCESS) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to load INI preset"), (NULL));
    return FALSE;
  }
  //ap_CheckSensorState(src->apbase, 0);

  if (!gst_aptinasrc_calculate_caps (src)) {
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_aptinasrc_stop (GstBaseSrc * bsrc)
{
  GstAptinaSrc *src = GST_APTINA_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "stop");

  ap_Destroy (src->apbase);
  ap_Finalize ();

  gst_aptinasrc_reset (src);

  return TRUE;
}

static GstCaps *
gst_aptinasrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstAptinaSrc *src = GST_APTINA_SRC (bsrc);
  GstCaps *caps;

  if (src->caps) {
    caps = gst_caps_copy (src->caps);
  } else {
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  }

  GST_DEBUG_OBJECT (src, "The caps before filtering are %" GST_PTR_FORMAT,
      caps);

  if (filter && caps) {
    GstCaps *tmp = gst_caps_intersect (caps, filter);
    gst_caps_unref (caps);
    caps = tmp;
  }

  GST_DEBUG_OBJECT (src, "The caps after filtering are %" GST_PTR_FORMAT, caps);

  return caps;
}

static gboolean
gst_aptinasrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstAptinaSrc *src = GST_APTINA_SRC (bsrc);
  GstVideoInfo vinfo;
  GstStructure *s = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);

  gst_video_info_from_caps (&vinfo, caps);

  if (GST_VIDEO_INFO_FORMAT (&vinfo) == GST_VIDEO_FORMAT_UNKNOWN) {
    goto unsupported_caps;
  }

  return TRUE;

unsupported_caps:
  GST_ERROR_OBJECT (src, "Unsupported caps: %" GST_PTR_FORMAT, caps);
  return FALSE;
}

static gboolean
gst_aptinasrc_unlock (GstBaseSrc * bsrc)
{
  GstAptinaSrc *src = GST_APTINA_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock");

  src->stop_requested = TRUE;

  return TRUE;
}

static gboolean
gst_aptinasrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstAptinaSrc *src = GST_APTINA_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock_stop");

  src->stop_requested = FALSE;

  return TRUE;
}

static GstFlowReturn
gst_aptinasrc_fill (GstPushSrc * psrc, GstBuffer * buf)
{
  GstAptinaSrc *src = GST_APTINA_SRC (psrc);
  ap_s32 ret;
  GstMapInfo minfo;
  GstClock *clock;
  GstClockTime clock_time;
  char *pBuffer = NULL;
  static int temp_ugly_buf_index = 0;
  guint8 *unpacked;
  ap_u32 rgb_width = 0, rgb_height = 0, rgb_depth = 0;

  GST_LOG_OBJECT (src, "create");

  if (!src->is_started) {
    /* TODO: check timestamps on buffers vs start time */
    src->acq_start_time =
        gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src)));

    src->is_started = TRUE;
  }

  ret = ap_GrabFrame (src->apbase, src->buffer, src->raw_framesize);
  if (ret == 0) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Grabbing failed with error %d", ap_GetLastError ()), (NULL));
    return GST_FLOW_ERROR;
  }

  unpacked =
      ap_ColorPipe (src->apbase, src->buffer, src->raw_framesize, &rgb_width,
      &rgb_height, &rgb_depth);

  clock = gst_element_get_clock (GST_ELEMENT (src));
  clock_time = gst_clock_get_time (clock);
  gst_object_unref (clock);

  gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
  orc_memcpy (minfo.data, unpacked, minfo.size);
  gst_buffer_unmap (buf, &minfo);

  GST_BUFFER_TIMESTAMP (buf) =
      GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
      clock_time);
  GST_BUFFER_OFFSET (buf) = temp_ugly_buf_index++;

  if (src->stop_requested) {
    return GST_FLOW_FLUSHING;
  }

  return GST_FLOW_OK;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_aptinasrc_debug, "aptinasrc", 0,
      "debug category for aptinasrc element");
  gst_element_register (plugin, "aptinasrc", GST_RANK_NONE,
      gst_aptinasrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    aptina,
    "Aptina camera source",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
