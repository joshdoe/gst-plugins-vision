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
 * SECTION:element-gstidsueyesrc
 *
 * The idsueyesrc element is a source for IDS uEye framegrabbers.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v idsueyesrc ! videoconvert ! autovideosink
 * ]|
 * Shows video from the default IDS uEye framegrabber
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#include "gstidsueyesrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_idsueyesrc_debug);
#define GST_CAT_DEFAULT gst_idsueyesrc_debug

/* prototypes */
static void gst_idsueyesrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_idsueyesrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_idsueyesrc_dispose (GObject * object);
static void gst_idsueyesrc_finalize (GObject * object);

static gboolean gst_idsueyesrc_start (GstBaseSrc * src);
static gboolean gst_idsueyesrc_stop (GstBaseSrc * src);
static GstCaps *gst_idsueyesrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_idsueyesrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_idsueyesrc_unlock (GstBaseSrc * src);
static gboolean gst_idsueyesrc_unlock_stop (GstBaseSrc * src);

static GstFlowReturn gst_idsueyesrc_create (GstPushSrc * src, GstBuffer ** buf);

static gboolean gst_idsueyesrc_set_framerate_exposure (GstIdsueyeSrc * src);
static gchar *gst_idsueyesrc_get_error_string (GstIdsueyeSrc * src,
    INT error_num);

enum
{
  PROP_0,
  PROP_CAMERA_ID,
  PROP_CONFIG_FILE,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_TIMEOUT,
  PROP_EXPOSURE,
  PROP_FRAMERATE
};

#define DEFAULT_PROP_CAMERA_ID 0
#define DEFAULT_PROP_CONFIG_FILE ""
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 3
#define DEFAULT_PROP_TIMEOUT 1000
#define DEFAULT_PROP_EXPOSURE 0
#define DEFAULT_PROP_FRAMERATE 0

/* pad templates */

static GstStaticPadTemplate gst_idsueyesrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, GRAY16_BE, BGRA }"))
    );

/* class initialization */

G_DEFINE_TYPE (GstIdsueyeSrc, gst_idsueyesrc, GST_TYPE_PUSH_SRC);

static void
gst_idsueyesrc_class_init (GstIdsueyeSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_idsueyesrc_set_property;
  gobject_class->get_property = gst_idsueyesrc_get_property;
  gobject_class->dispose = gst_idsueyesrc_dispose;
  gobject_class->finalize = gst_idsueyesrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_idsueyesrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "IDS uEye Video Source", "Source/Video",
      "IDS uEye framegrabber video source", "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_idsueyesrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_idsueyesrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_idsueyesrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_idsueyesrc_set_caps);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_idsueyesrc_unlock);
  gstbasesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_idsueyesrc_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_idsueyesrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_CAMERA_ID,
      g_param_spec_int ("camera-id", "Camera ID",
          "Camera ID (0 is first found)", 0, 254, DEFAULT_PROP_CAMERA_ID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CONFIG_FILE,
      g_param_spec_string ("config-file", "Config file",
          "Filepath of the uEye parameter file (*.ini)",
          DEFAULT_PROP_CONFIG_FILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_int ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 2, MAX_SEQ_BUFFERS,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_int ("timeout", "Timeout (ms)",
          "Timeout in ms (0 to use default)", 0, G_MAXINT, DEFAULT_PROP_TIMEOUT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_EXPOSURE,
      g_param_spec_double ("exposure", "Exposure (ms)",
          "Exposure in ms (0 to max of 1/FPS)", 0, G_MAXDOUBLE,
          DEFAULT_PROP_EXPOSURE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FRAMERATE,
      g_param_spec_double ("framerate", "Framerate (Hz)",
          "Framerate in frames per second", 0, G_MAXDOUBLE,
          DEFAULT_PROP_FRAMERATE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_idsueyesrc_reset (GstIdsueyeSrc * src)
{
  src->hCam = 0;
  src->is_started = FALSE;

  src->last_frame_count = 0;
  src->total_dropped_frames = 0;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }
}

static void
gst_idsueyesrc_init (GstIdsueyeSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->camera_id = DEFAULT_PROP_CAMERA_ID;
  src->config_file = g_strdup (DEFAULT_PROP_CONFIG_FILE);
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;
  src->timeout = DEFAULT_PROP_TIMEOUT;
  src->exposure = DEFAULT_PROP_EXPOSURE;
  src->framerate = DEFAULT_PROP_FRAMERATE;

  src->stop_requested = FALSE;
  src->caps = NULL;

  gst_idsueyesrc_reset (src);
}

void
gst_idsueyesrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstIdsueyeSrc *src;

  src = GST_IDSUEYE_SRC (object);

  switch (property_id) {
    case PROP_CAMERA_ID:
      src->camera_id = g_value_get_int (value);
      break;
    case PROP_CONFIG_FILE:
      g_free (src->config_file);
      src->config_file = g_strdup (g_value_get_string (value));
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      src->num_capture_buffers = g_value_get_int (value);
      break;
    case PROP_TIMEOUT:
      src->timeout = g_value_get_int (value);
      break;
    case PROP_EXPOSURE:
      src->exposure = g_value_get_double (value);
      if (src->is_started) {
        gst_idsueyesrc_set_framerate_exposure (src);
      }
      break;
    case PROP_FRAMERATE:
      src->framerate = g_value_get_double (value);
      if (src->is_started) {
        gst_idsueyesrc_set_framerate_exposure (src);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_idsueyesrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstIdsueyeSrc *src;

  g_return_if_fail (GST_IS_IDSUEYE_SRC (object));
  src = GST_IDSUEYE_SRC (object);

  switch (property_id) {
    case PROP_CAMERA_ID:
      g_value_set_int (value, src->camera_id);
      break;
    case PROP_CONFIG_FILE:
      g_value_set_string (value, src->config_file);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      g_value_set_int (value, src->num_capture_buffers);
      break;
    case PROP_TIMEOUT:
      g_value_set_int (value, src->timeout);
      break;
    case PROP_EXPOSURE:
      g_value_set_double (value, src->exposure);
      break;
    case PROP_FRAMERATE:
      g_value_set_double (value, src->framerate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_idsueyesrc_dispose (GObject * object)
{
  GstIdsueyeSrc *src;

  g_return_if_fail (GST_IS_IDSUEYE_SRC (object));
  src = GST_IDSUEYE_SRC (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_idsueyesrc_parent_class)->dispose (object);
}

void
gst_idsueyesrc_finalize (GObject * object)
{
  GstIdsueyeSrc *src;

  g_return_if_fail (GST_IS_IDSUEYE_SRC (object));
  src = GST_IDSUEYE_SRC (object);

  /* clean up object here */

  g_free (src->config_file);

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  G_OBJECT_CLASS (gst_idsueyesrc_parent_class)->finalize (object);
}


static void
gst_idsueyesrc_set_caps_from_camera (GstIdsueyeSrc * src)
{
  guint bpp;
  gint idsColorMode;
  GstVideoFormat videoFormat = GST_VIDEO_FORMAT_UNKNOWN;
  GstVideoInfo vinfo;
  IS_SIZE_2D imageSize;
  INT ret;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  idsColorMode = is_SetColorMode (src->hCam, IS_GET_COLOR_MODE);

  switch (idsColorMode) {
    case IS_CM_SENSOR_RAW8:
      bpp = 8;
      videoFormat = GST_VIDEO_FORMAT_GRAY8;
      break;
    case IS_CM_MONO8:
      bpp = 8;
      videoFormat = GST_VIDEO_FORMAT_GRAY8;
      break;
    case IS_CM_MONO10:
      bpp = 10;
      videoFormat = GST_VIDEO_FORMAT_GRAY16_LE;
      break;
    case IS_CM_MONO12:
      bpp = 12;
      videoFormat = GST_VIDEO_FORMAT_GRAY16_LE;
      break;
    case IS_CM_MONO16:
      bpp = 16;
      videoFormat = GST_VIDEO_FORMAT_GRAY16_LE;
      break;
    case IS_CM_BGR8_PACKED:
      bpp = 24;
      videoFormat = GST_VIDEO_FORMAT_BGR;
      break;
    case IS_CM_RGB8_PACKED:
      bpp = 24;
      videoFormat = GST_VIDEO_FORMAT_RGB;
      break;
    case IS_CM_RGBA8_PACKED:
      bpp = 32;
      videoFormat = GST_VIDEO_FORMAT_RGBA;
      break;
    case IS_CM_BGRA8_PACKED:
      bpp = 32;
      videoFormat = GST_VIDEO_FORMAT_BGRA;
      break;
  }

  if (videoFormat == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Unknown or unsupported color format: %d", idsColorMode), (NULL));
    return;
  }

  ret = is_AOI (src->hCam, IS_AOI_IMAGE_GET_SIZE, (void *) &imageSize,
      sizeof (imageSize));
  if (ret != IS_SUCCESS) {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Failed to query AOI size"), (NULL));
    return;
  }

  src->width = imageSize.s32Width;
  src->height = imageSize.s32Height;

  gst_video_info_init (&vinfo);

  gst_video_info_set_format (&vinfo, videoFormat, src->width, src->height);
  src->caps = gst_video_info_to_caps (&vinfo);

  if (videoFormat == GST_VIDEO_FORMAT_GRAY16_BE) {
    GValue val = G_VALUE_INIT;
    GstStructure *s;

    /* set bpp, extra info for GRAY16 so elements can scale properly */
    s = gst_caps_get_structure (src->caps, 0);
    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, bpp);
    gst_structure_set_value (s, "bpp", &val);
    g_value_unset (&val);
  }

  src->bitsPerPixel = GST_ROUND_UP_8 (bpp);
}

static gboolean
gst_idsueyesrc_alloc_memory (GstIdsueyeSrc * src)
{
  INT ret = IS_SUCCESS;
  /* this should be identical to GStreamer buffer size */
  gint bufferSize = src->width * src->height * src->bitsPerPixel / 8;
  gint i;

  /* alloc seq buffers in a loop */
  for (i = 0; i < src->num_capture_buffers; i++) {
    ret = is_AllocImageMem (src->hCam, src->width, src->height,
        src->bitsPerPixel, &src->seqImgMem[i], &src->seqMemId[i]);
    if (ret != IS_SUCCESS) {
      break;
    }

    ret = is_AddToSequence (src->hCam, src->seqImgMem[i], src->seqMemId[i]);
    //m_nSeqNumId[i] = i+1;   // store sequence buffer number Id
    if (ret != IS_SUCCESS) {
      // free latest buffer
      is_FreeImageMem (src->hCam, src->seqImgMem[i], src->seqMemId[i]);
      break;
    }
  }

  if (ret != IS_SUCCESS) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to allocate memory: %s", gst_idsueyesrc_get_error_string (src,
                ret)), (NULL));
    return FALSE;
  } else {
    return TRUE;
  }
}

#ifdef G_OS_WIN32
void *
char_to_ids_unicode (const char *str)
{
  size_t newsize = strlen (str) + 1;
  wchar_t *wcstring = g_new (wchar_t, newsize);
  size_t convertedChars = 0;
  mbstowcs_s (&convertedChars, wcstring, newsize, str, _TRUNCATE);
  return wcstring;
}
#else
void *
char_to_ids_unicode (const char *str)
{
  return g_utf8_to_ucs4 (str, -1, NULL, NULL, NULL);
}
#endif

static gboolean
gst_idsueyesrc_start (GstBaseSrc * bsrc)
{
  GstIdsueyeSrc *src = GST_IDSUEYE_SRC (bsrc);
  INT ret;
  INT numCameras;

  GST_DEBUG_OBJECT (src, "start");

  if (is_GetNumberOfCameras (&numCameras) != IS_SUCCESS) {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        ("No IDS uEye cameras found"), (NULL));
    return FALSE;
  } else {
    UEYE_CAMERA_LIST *pucl;
    pucl =
        (UEYE_CAMERA_LIST *) g_malloc (sizeof (DWORD) +
        numCameras * sizeof (UEYE_CAMERA_INFO));
    pucl->dwCount = numCameras;

    if (is_GetCameraList (pucl) == IS_SUCCESS) {
      int iCamera;
      for (iCamera = 0; iCamera < (int) pucl->dwCount; iCamera++) {
        UEYE_CAMERA_INFO *c = &pucl->uci[iCamera];
        GST_DEBUG_OBJECT (src,
            "Found IDS uEye camera: idx=%d, id=%d, ser=%s, model=%s", iCamera,
            c->dwCameraID, c->SerNo, c->FullModelName);
      }
    }
    g_free (pucl);
  }

  src->hCam = src->camera_id;
  ret = is_InitCamera (&src->hCam, NULL);
  if (ret != IS_SUCCESS) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to initialize camera: %s",
            gst_idsueyesrc_get_error_string (src, ret)), (NULL));
    return FALSE;
  }

  {
    /* has cam ID,  type, date, version, ser no */
    CAMINFO cInfo;
    SENSORINFO sInfo;
    ret = is_GetCameraInfo (src->hCam, &cInfo);
    ret = is_GetSensorInfo (src->hCam, &sInfo);
  }

  if (strlen (src->config_file)) {
    void *filepath;
    if (!g_file_test (src->config_file, G_FILE_TEST_EXISTS)) {
      GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
          ("Camera file does not exist: %s", src->config_file), (NULL));
      return FALSE;
    }

    /* function requires Unicode */
    filepath = char_to_ids_unicode (src->config_file);
    ret =
        is_ParameterSet (src->hCam, IS_PARAMETERSET_CMD_LOAD_FILE, filepath, 0);
    g_free (filepath);
    if (ret != IS_SUCCESS) {
      switch (ret) {
        case IS_INCOMPATIBLE_SETTING:
          GST_ELEMENT_WARNING (src, RESOURCE, OPEN_READ,
              ("IS_INCOMPATIBLE_SETTING: incompatible setting"), (NULL));
          break;
        case IS_INVALID_CAMERA_TYPE:
          GST_ELEMENT_WARNING (src, RESOURCE, OPEN_READ,
              ("IS_INVALID_CAMERA_TYPE: invalid camera type"), (NULL));
          break;
        case IS_NO_SUCCESS:
          GST_ELEMENT_WARNING (src, RESOURCE, OPEN_READ,
              ("IS_NO_SUCCESS: general error"), (NULL));
          break;
        default:
          GST_ELEMENT_WARNING (src, RESOURCE, OPEN_READ,
              ("UNKNOWN ERROR: return value=%d", ret), (NULL));
          break;
      }
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
          ("Failed to load parameter file: %s", src->config_file), (NULL));
      return FALSE;
    }
  } else {
    ret =
        is_ParameterSet (src->hCam, IS_PARAMETERSET_CMD_LOAD_EEPROM, NULL,
        NULL);
    if (ret != IS_SUCCESS) {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
          ("Failed to load parameters from EEPROM"), (NULL));
      return FALSE;
    }
  }

  gst_idsueyesrc_set_caps_from_camera (src);
  if (!src->caps) {
    return FALSE;
  }

  if (!gst_idsueyesrc_alloc_memory (src)) {
    /* element error already sent */
    return FALSE;
  }

  ret = is_SetDisplayMode (src->hCam, IS_SET_DM_DIB);
  if (ret != IS_SUCCESS) {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Failed to set display mode"), (NULL));
    return FALSE;
  }

  ret = is_InitImageQueue (src->hCam, 0);
  if (ret != IS_SUCCESS) {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Failed to init image queue"), (NULL));
    return FALSE;
  }

  gst_idsueyesrc_set_framerate_exposure (src);

  return TRUE;
}

static gboolean
gst_idsueyesrc_stop (GstBaseSrc * bsrc)
{
  GstIdsueyeSrc *src = GST_IDSUEYE_SRC (bsrc);
  INT ret;
  gint i;

  GST_DEBUG_OBJECT (src, "stop");

  ret = is_StopLiveVideo (src->hCam, IS_FORCE_VIDEO_STOP);
  if (ret != IS_SUCCESS) {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Failed to stop live video"), (NULL));
  }

  ret = is_ExitImageQueue (src->hCam);
  if (ret != IS_SUCCESS) {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Failed to stop image queue"), (NULL));
  }

  ret = is_ClearSequence (src->hCam);
  if (ret != IS_SUCCESS) {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Failed to clear sequence"), (NULL));
  }

  for (i = 0; i < src->num_capture_buffers; ++i) {
    ret = is_FreeImageMem (src->hCam, src->seqImgMem[i], src->seqMemId[i]);
    if (ret != IS_SUCCESS) {
      GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
          ("Failed to free image memory"), (NULL));
    }
  }

  ret = is_ExitCamera (src->hCam);
  if (ret != IS_SUCCESS) {
    GST_WARNING_OBJECT (src, "Failed to release camera: %s",
        gst_idsueyesrc_get_error_string (src, ret));
  }

  gst_idsueyesrc_reset (src);

  return TRUE;
}

static GstCaps *
gst_idsueyesrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstIdsueyeSrc *src = GST_IDSUEYE_SRC (bsrc);
  GstCaps *caps;

  if (src->hCam == 0) {
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  } else {
    caps = gst_caps_copy (src->caps);
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
gst_idsueyesrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstIdsueyeSrc *src = GST_IDSUEYE_SRC (bsrc);
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
gst_idsueyesrc_unlock (GstBaseSrc * bsrc)
{
  GstIdsueyeSrc *src = GST_IDSUEYE_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock");

  src->stop_requested = TRUE;

  return TRUE;
}

static gboolean
gst_idsueyesrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstIdsueyeSrc *src = GST_IDSUEYE_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock_stop");

  src->stop_requested = FALSE;

  return TRUE;
}

static GstFlowReturn
gst_idsueyesrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstIdsueyeSrc *src = GST_IDSUEYE_SRC (psrc);
  INT ret;
  GstMapInfo minfo;
  guint32 dropped_frames;
  GstClock *clock;
  GstClockTime clock_time;
  char *pBuffer = NULL;
  INT nMemID = 0;
  UEYEIMAGEINFO imageInfo;
  static int temp_ugly_buf_index = 0;

  GST_LOG_OBJECT (src, "create");

  if (!src->is_started) {
    ret = is_CaptureVideo (src->hCam, IS_DONT_WAIT);
    if (ret != IS_SUCCESS) {
      GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
          ("Failed to start video capture"), (NULL));
      return GST_FLOW_ERROR;
    }

    /* TODO: check timestamps on buffers vs start time */
    src->acq_start_time =
        gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src)));

    src->is_started = TRUE;
  }

  while (TRUE) {
    ret = is_WaitForNextImage (src->hCam, src->timeout, &pBuffer, &nMemID);
    if (ret == IS_SUCCESS) {
      break;
    } else if (ret == IS_CAPTURE_STATUS) {
      UEYE_CAPTURE_STATUS_INFO captureStatusInfo;
      ret =
          is_CaptureStatus (src->hCam, IS_CAPTURE_STATUS_INFO_CMD_GET,
          (void *) &captureStatusInfo, sizeof (captureStatusInfo));
      if (ret != IS_SUCCESS) {
        GST_WARNING_OBJECT (src,
            "Some sort of error occurred with capture, but failed to query capture status");
      } else {
        GST_WARNING_OBJECT (src,
            "Capture errors occurred, perhaps dropped frames, total of %d errors",
            captureStatusInfo.dwCapStatusCnt_Total);
      }

      continue;
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to acquire frame before timeout: %s",
              gst_idsueyesrc_get_error_string (src, ret)), (NULL));
      return GST_FLOW_ERROR;
    }
  }

  clock = gst_element_get_clock (GST_ELEMENT (src));
  clock_time = gst_clock_get_time (clock);
  gst_object_unref (clock);


  /* TODO: check for dropped frames and disrupted signal */
  //dropped_frames = (circ_handle.FrameCount - src->last_frame_count) - 1;
  //if (dropped_frames > 0) {
  //  src->total_dropped_frames += dropped_frames;
  //  GST_WARNING_OBJECT (src, "Dropped %d frames (%d total)", dropped_frames,
  //      src->total_dropped_frames);
  //} else if (dropped_frames < 0) {
  //  GST_WARNING_OBJECT (src, "Frame count non-monotonic, signal disrupted?");
  //}
  //src->last_frame_count = circ_handle.FrameCount;

  // TODO: use is_GetImageInfo to get timestamp, dropped frames
  ret = is_GetImageInfo (src->hCam, nMemID, &imageInfo, sizeof (imageInfo));
  GST_LOG_OBJECT (src, "frame number %d", imageInfo.u64FrameNumber);

  /* TODO: use allocator or use from pool */
  *buf =
      gst_buffer_new_and_alloc (src->width * src->height * src->bitsPerPixel /
      8);
  gst_buffer_map (*buf, &minfo, GST_MAP_WRITE);
  /* TODO: use orc_memcpy */
  //memcpy (minfo.data, ((guint8 *) circ_handle->pBufData), minfo.size);
  is_CopyImageMem (src->hCam, pBuffer, nMemID, (char *) minfo.data);
  gst_buffer_unmap (*buf, &minfo);

  ret = is_UnlockSeqBuf (src->hCam, nMemID, pBuffer);
  if (ret != IS_SUCCESS) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to unlock buffer: %s", gst_idsueyesrc_get_error_string (src,
                ret)), (NULL));
    return GST_FLOW_ERROR;
  }

  GST_BUFFER_TIMESTAMP (*buf) =
      GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
      clock_time);
  GST_BUFFER_OFFSET (*buf) = temp_ugly_buf_index++;

  if (src->stop_requested) {
    if (*buf != NULL) {
      gst_buffer_unref (*buf);
      *buf = NULL;
    }
    return GST_FLOW_FLUSHING;
  }

  return GST_FLOW_OK;
}


gboolean
gst_idsueyesrc_set_framerate_exposure (GstIdsueyeSrc * src)
{
  INT ret;
  gboolean success = TRUE;
  double new_fps;

  ret = is_SetFrameRate (src->hCam, src->framerate, &new_fps);
  if (ret != IS_SUCCESS) {
    GST_WARNING_OBJECT (src, "Failed to set framerate to %.3f (error %d)",
        src->framerate, ret);
    success = FALSE;
  }

  ret =
      is_Exposure (src->hCam, IS_EXPOSURE_CMD_SET_EXPOSURE, &src->exposure, 8);
  if (ret != IS_SUCCESS) {
    GST_WARNING_OBJECT (src, "Failed to set exposure to %.3f (error %d)",
        src->exposure, ret);
    success = FALSE;
  }

  return success;
}

gchar *
gst_idsueyesrc_get_error_string (GstIdsueyeSrc * src, INT error_num)
{
  INT err = error_num;
  IS_CHAR *err_string;
  is_GetError (src->hCam, &err, &err_string);
  return err_string;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_idsueyesrc_debug, "idsueyesrc", 0,
      "debug category for idsueyesrc element");
  gst_element_register (plugin, "idsueyesrc", GST_RANK_NONE,
      gst_idsueyesrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    idsueye,
    "IDS uEye frame grabber source",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
