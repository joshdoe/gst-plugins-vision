/* GStreamer
 * Copyright (C) 2021 FIXME <fixme@example.com>
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
 * SECTION:element-gstqcamsrc
 *
 * The qcamsrc element is a source for QImaging QCam cameras like the Retiga 2000R
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v qcamsrc ! videoconvert ! autovideosink
 * ]|
 * Shows video from the default QCam device
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#include "gstqcamsrc.h"

#include <QCamApi.h>

GST_DEBUG_CATEGORY_STATIC (gst_qcamsrc_debug);
#define GST_CAT_DEFAULT gst_qcamsrc_debug

/* prototypes */
static void gst_qcamsrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_qcamsrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_qcamsrc_dispose (GObject * object);
static void gst_qcamsrc_finalize (GObject * object);

static gboolean gst_qcamsrc_start (GstBaseSrc * src);
static gboolean gst_qcamsrc_stop (GstBaseSrc * src);
static GstCaps *gst_qcamsrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_qcamsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_qcamsrc_unlock (GstBaseSrc * src);
static gboolean gst_qcamsrc_unlock_stop (GstBaseSrc * src);

static GstFlowReturn gst_qcamsrc_create (GstPushSrc * src, GstBuffer ** buf);

static void gst_qcamsrc_frame_callback (void *userPtr, unsigned long userData,
    QCam_Err errcode, unsigned long flags);

enum
{
  PROP_0,
  PROP_DEVICE_INDEX,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_TIMEOUT,
  PROP_EXPOSURE,
  PROP_GAIN,
  PROP_OFFSET,
  PROP_FORMAT,
  PROP_X,
  PROP_Y,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_BINNING
};

#define DEFAULT_PROP_DEVICE_INDEX 0
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 3
#define DEFAULT_PROP_TIMEOUT 500
#define DEFAULT_PROP_EXPOSURE 16384
#define DEFAULT_PROP_GAIN 1.0
#define DEFAULT_PROP_OFFSET 0
#define DEFAULT_PROP_FORMAT qfmtMono16
#define DEFAULT_PROP_X 0
#define DEFAULT_PROP_Y 0
#define DEFAULT_PROP_WIDTH 0
#define DEFAULT_PROP_HEIGHT 0
#define DEFAULT_PROP_BINNING 1

/* pad templates */
static GstStaticPadTemplate gst_qcamsrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ GRAY16_LE, GRAY8 }")
    )
    );

/* class initialization */

G_DEFINE_TYPE (GstQcamSrc, gst_qcamsrc, GST_TYPE_PUSH_SRC);

static int g_qcam_use_count = 0;

static void
gst_qcamsrc_driver_ref ()
{
  if (g_qcam_use_count == 0) {
    QCam_LoadDriver ();
  }
  g_qcam_use_count++;
}

static void
gst_qcamsrc_driver_unref ()
{
  g_qcam_use_count--;
  if (g_qcam_use_count == 0) {
    QCam_ReleaseDriver ();
  }
}

static void
gst_qcamsrc_class_init (GstQcamSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "qcamsrc", 0,
      "QImaging QCam source");

  gobject_class->set_property = gst_qcamsrc_set_property;
  gobject_class->get_property = gst_qcamsrc_get_property;
  gobject_class->dispose = gst_qcamsrc_dispose;
  gobject_class->finalize = gst_qcamsrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_qcamsrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "QCam Video Source", "Source/Video",
      "QImaging QCam video source", "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_qcamsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_qcamsrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_qcamsrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_qcamsrc_set_caps);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_qcamsrc_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_qcamsrc_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_qcamsrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("device-index", "Device index",
          "Index of device, use -1 to enumerate all and select last", -1,
          G_MAXINT, DEFAULT_PROP_DEVICE_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_uint ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 1, G_MAXUINT,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TIMEOUT,
      g_param_spec_int ("timeout", "Timeout (ms)",
          "Timeout in ms to wait for a frame beyond exposure time", 0, G_MAXINT,
          DEFAULT_PROP_TIMEOUT,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_EXPOSURE,
      g_param_spec_uint ("exposure", "Exposure (us)",
          "Exposure time in microseconds", 0, G_MAXINT, DEFAULT_PROP_EXPOSURE,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_GAIN,
      g_param_spec_double ("gain", "Normalized gain", "Normalized gain", 0,
          1000, DEFAULT_PROP_GAIN,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_OFFSET,
      g_param_spec_int ("offset", "Offset", "Absolute offset", -G_MAXINT,
          G_MAXINT, DEFAULT_PROP_OFFSET,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FORMAT,
      g_param_spec_int ("format", "Image format",
          "Image format (2=GRAY8, 3=GRAY16_LE)", 2, 3, DEFAULT_PROP_FORMAT,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_X,
      g_param_spec_int ("x", "ROI x pixel", "ROI x pixel position", 0, G_MAXINT,
          DEFAULT_PROP_X,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_Y,
      g_param_spec_int ("y", "ROI y pixel", "ROI y pixel position", 0, G_MAXINT,
          DEFAULT_PROP_Y,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_WIDTH,
      g_param_spec_int ("width", "ROI width", "ROI width", 0, G_MAXINT,
          DEFAULT_PROP_WIDTH,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_HEIGHT,
      g_param_spec_int ("height", "ROI height", "ROI height", 0, G_MAXINT,
          DEFAULT_PROP_HEIGHT,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_BINNING,
      g_param_spec_int ("binning", "Binning", "Symmetrical binning", 1, 8,
          DEFAULT_PROP_BINNING,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
}

static void
gst_qcamsrc_reset (GstQcamSrc * src)
{
  src->handle = NULL;

  src->exposure = DEFAULT_PROP_EXPOSURE;
  src->gain = DEFAULT_PROP_GAIN;
  src->offset = DEFAULT_PROP_OFFSET;
  src->format = DEFAULT_PROP_FORMAT;
  src->x = DEFAULT_PROP_X;
  src->y = DEFAULT_PROP_Y;
  src->width = DEFAULT_PROP_WIDTH;
  src->height = DEFAULT_PROP_HEIGHT;
  src->binning = DEFAULT_PROP_BINNING;

  src->last_frame_count = 0;
  src->total_dropped_frames = 0;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  if (src->queue) {
    // TODO: remove dangling buffers
    g_async_queue_unref (src->queue);
  }
  src->queue = g_async_queue_new ();
}

static void
gst_qcamsrc_init (GstQcamSrc * src)
{
  GST_DEBUG_OBJECT (src, "Initialize instance");

  gst_qcamsrc_driver_ref ();

  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->device_index = DEFAULT_PROP_DEVICE_INDEX;
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;
  src->timeout = DEFAULT_PROP_TIMEOUT;

  src->stop_requested = FALSE;
  src->caps = NULL;
  src->queue = NULL;

  gst_qcamsrc_reset (src);
}


static void
gst_qcamsrc_set_exposure (GstQcamSrc * src, unsigned long exposure)
{
  QCam_SetParam (&src->qsettings, qprmExposure, exposure);
}

static void
gst_qcamsrc_set_gain (GstQcamSrc * src, float gain)
{
  QCam_SetParam (&src->qsettings, qprmNormalizedGain,
      (unsigned long) (gain * 1000000));
}

static void
gst_qcamsrc_set_offset (GstQcamSrc * src, long offset)
{
  QCam_SetParamS32 (&src->qsettings, qprmS32AbsoluteOffset, offset);
}


void
gst_qcamsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstQcamSrc *src;

  src = GST_QCAM_SRC (object);

  switch (property_id) {
    case PROP_DEVICE_INDEX:
      src->device_index = g_value_get_int (value);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      src->num_capture_buffers = g_value_get_uint (value);
      break;
    case PROP_TIMEOUT:
      src->timeout = g_value_get_int (value);
      break;
    case PROP_EXPOSURE:
      src->exposure = g_value_get_uint (value);
      src->send_settings = TRUE;
      break;
    case PROP_GAIN:
      src->gain = g_value_get_double (value);
      src->send_settings = TRUE;
      break;
    case PROP_OFFSET:
      src->offset = g_value_get_int (value);
      src->send_settings = TRUE;
      break;
    case PROP_FORMAT:
      src->format = g_value_get_int (value);
      break;
    case PROP_X:
      src->x = g_value_get_int (value);
      break;
    case PROP_Y:
      src->y = g_value_get_int (value);
      break;
    case PROP_WIDTH:
      src->width = GST_ROUND_DOWN_4 (g_value_get_int (value));
      break;
    case PROP_HEIGHT:
      src->height = g_value_get_int (value);
      break;
    case PROP_BINNING:
      src->binning = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_qcamsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstQcamSrc *src;

  g_return_if_fail (GST_IS_QCAM_SRC (object));
  src = GST_QCAM_SRC (object);

  switch (property_id) {
    case PROP_DEVICE_INDEX:
      g_value_set_int (value, src->device_index);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      g_value_set_uint (value, src->num_capture_buffers);
      break;
    case PROP_TIMEOUT:
      g_value_set_int (value, src->timeout);
      break;
    case PROP_EXPOSURE:
      g_value_set_uint (value, src->exposure);
      break;
    case PROP_GAIN:
      g_value_set_double (value, src->gain);
      break;
    case PROP_OFFSET:
      g_value_set_int (value, src->offset);
      break;
    case PROP_FORMAT:
      g_value_set_int (value, src->format);
      break;
    case PROP_X:
      g_value_set_int (value, src->x);
      break;
    case PROP_Y:
      g_value_set_int (value, src->y);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, src->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, src->height);
      break;
    case PROP_BINNING:
      g_value_set_int (value, src->binning);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_qcamsrc_dispose (GObject * object)
{
  GstQcamSrc *src;

  g_return_if_fail (GST_IS_QCAM_SRC (object));
  src = GST_QCAM_SRC (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_qcamsrc_parent_class)->dispose (object);
}

void
gst_qcamsrc_finalize (GObject * object)
{
  GstQcamSrc *src;

  g_return_if_fail (GST_IS_QCAM_SRC (object));
  src = GST_QCAM_SRC (object);

  /* clean up object here */

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  gst_qcamsrc_driver_unref ();

  G_OBJECT_CLASS (gst_qcamsrc_parent_class)->finalize (object);
}

typedef struct
{
  GstQcamSrc *src;
  QCam_Frame frame;
  GstClockTime clock_time;
} VideoFrame;

static void
video_frame_queue (VideoFrame * frame)
{
  QCam_Err err;
  g_assert (frame->src->handle);

  GST_TRACE_OBJECT (frame->src, "Queuing frame 0x%x", frame);
  err = QCam_QueueFrame (frame->src->handle,
      &frame->frame,
      gst_qcamsrc_frame_callback,
      qcCallbackDone | qcCallbackExposeDone, frame, 0);
}

static void
video_frame_release (void *data)
{
  VideoFrame *frame = (VideoFrame *) data;
  if (!frame->src->stop_requested && frame->src->handle) {
    video_frame_queue (frame);
  }
}

static VideoFrame *
video_frame_create (GstQcamSrc * src, gsize buf_size)
{
  VideoFrame *frame = g_new (VideoFrame, 1);
  frame->frame.pBuffer = g_malloc (buf_size);

  if (!frame->frame.pBuffer) {
    GST_ERROR_OBJECT (src, "Failed to allocate buffer of size %d", buf_size);
    g_free (frame);
    return NULL;
  }
  frame->src = src;
  frame->frame.bufferSize = buf_size;

  return frame;
}


static gboolean
gst_qcamsrc_setup_stream (GstQcamSrc * src)
{
  QCam_Err err;
  QCam_CamListItem cam_list[255];
  unsigned long num_cams = 255;
  unsigned long width, height, imageFormat, exposure, gain;
  long offset;

  err = QCam_ListCameras (cam_list, &num_cams);
  if (err != qerrSuccess) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        ("Failed to get list of cameras (errcode=%d)", err), (NULL));
    return FALSE;
  }

  GST_DEBUG_OBJECT (src, "Found %d cameras", num_cams);

  if (src->device_index + 1 > num_cams) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        ("device-index (%d) exceeds number of cameras found (%d)",
            src->device_index, num_cams), (NULL));
    return FALSE;
  }

  err = QCam_OpenCamera (cam_list[src->device_index].cameraId, &src->handle);
  if (err != qerrSuccess) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        ("Failed to open camera (errcode=%d)", err), (NULL));
    return FALSE;
  }

  err = QCam_SetStreaming (src->handle, TRUE);
  if (err != qerrSuccess) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        ("Failed to start streaming (errcode=%d)", err), (NULL));
    QCam_CloseCamera (src->handle);
    return FALSE;
  }

  err = QCam_ReadSettingsFromCam (src->handle, &src->qsettings);
  if (err != qerrSuccess) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        ("Failed to read settings (errcode=%d)", err), (NULL));
    QCam_CloseCamera (src->handle);
    return FALSE;
  }

  err = QCam_SetParam (&src->qsettings, qprmImageFormat, src->format);
  err = QCam_SetParam (&src->qsettings, qprmBinning, src->binning);
  err = QCam_SetParam (&src->qsettings, qprmRoiX, src->x);
  err = QCam_SetParam (&src->qsettings, qprmRoiX, src->y);
  if (src->width > 0)
    err = QCam_SetParam (&src->qsettings, qprmRoiWidth, src->width);
  if (src->height > 0)
    err = QCam_SetParam (&src->qsettings, qprmRoiHeight, src->height);
  gst_qcamsrc_set_exposure (src, src->exposure);
  gst_qcamsrc_set_gain (src, src->gain);
  gst_qcamsrc_set_offset (src, src->offset);
  QCam_SendSettingsToCam (src->handle, &src->qsettings);
  src->send_settings = FALSE;

  err = QCam_GetInfo (src->handle, qinfCcdWidth, &width);
  err = QCam_GetInfo (src->handle, qinfCcdHeight, &height);

  GST_DEBUG_OBJECT (src, "Opened camera with CCD width,height=%d,%d", width,
      height);

  err = QCam_GetParam (&src->qsettings, qprmRoiWidth, &width);
  err = QCam_GetParam (&src->qsettings, qprmRoiHeight, &height);
  err = QCam_GetParam (&src->qsettings, qprmImageFormat, &imageFormat);
  err = QCam_GetParam (&src->qsettings, qprmExposure, &exposure);
  err = QCam_GetParam (&src->qsettings, qprmNormalizedGain, &gain);
  err = QCam_GetParamS32 (&src->qsettings, qprmS32AbsoluteOffset, &offset);
  GST_DEBUG_OBJECT (src,
      "ROI configured with width,height,format,exposure,gain,offset=%d,%d,%d,%d,%d,%d,%d",
      width, height, imageFormat, exposure, gain, offset);

  for (int i = 0; i < src->num_capture_buffers; ++i) {
    unsigned long buf_size;
    QCam_GetInfo (src->handle, qinfImageSize, &buf_size);
    VideoFrame *frame = video_frame_create (src, buf_size);
    video_frame_queue (frame);
  }

  {
    GstStructure *structure;
    GstCaps *caps;
    caps = gst_caps_new_empty ();
    structure = gst_structure_from_string ("video/x-raw", NULL);
    const char *gst_format;
    if (imageFormat == qfmtMono8)
      gst_format = "GRAY8";
    else if (imageFormat == qfmtMono16)
      gst_format = "GRAY16_LE";
    gst_structure_set (structure,
        "format", G_TYPE_STRING, gst_format,
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
    gst_caps_append_structure (caps, structure);

    if (src->caps) {
      gst_caps_unref (src->caps);
    }
    src->caps = caps;
    gst_base_src_set_caps (GST_BASE_SRC (src), src->caps);
  }

  return TRUE;
}

static gboolean
gst_qcamsrc_start (GstBaseSrc * bsrc)
{
  GstQcamSrc *src = GST_QCAM_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "start");

  if (!gst_qcamsrc_setup_stream (src)) {
    /* error already sent */
    goto error;
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_qcamsrc_stop (GstBaseSrc * bsrc)
{
  GstQcamSrc *src = GST_QCAM_SRC (bsrc);
  GST_DEBUG_OBJECT (src, "stop");

  if (src->handle) {
    QCam_CloseCamera (src->handle);
    src->handle = NULL;
  }

  gst_qcamsrc_reset (src);

  return TRUE;
}

static GstCaps *
gst_qcamsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstQcamSrc *src = GST_QCAM_SRC (bsrc);
  GstCaps *caps;

  if (src->caps == NULL) {
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
gst_qcamsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstQcamSrc *src = GST_QCAM_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);

  return TRUE;
}

static gboolean
gst_qcamsrc_unlock (GstBaseSrc * bsrc)
{
  GstQcamSrc *src = GST_QCAM_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock");

  src->stop_requested = TRUE;

  return TRUE;
}

static gboolean
gst_qcamsrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstQcamSrc *src = GST_QCAM_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock_stop");

  src->stop_requested = FALSE;

  return TRUE;
}

static GstFlowReturn
gst_qcamsrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstQcamSrc *src = GST_QCAM_SRC (psrc);
  VideoFrame *video_frame;
  GST_LOG_OBJECT (src, "create");

  video_frame =
      (VideoFrame *) g_async_queue_timeout_pop (src->queue,
      (guint64) src->timeout * 1000 + src->exposure);
  if (!video_frame) {
    if (!src->stop_requested) {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
          ("Failed to get buffer in %d ms", src->timeout), (NULL));
      return GST_FLOW_ERROR;
    } else {
      return GST_FLOW_FLUSHING;
    }
  }

  *buf = gst_buffer_new_wrapped_full ((GstMemoryFlags)
      GST_MEMORY_FLAG_PHYSICALLY_CONTIGUOUS,
      (gpointer) video_frame->frame.pBuffer, video_frame->frame.bufferSize, 0,
      video_frame->frame.bufferSize, video_frame,
      (GDestroyNotify) video_frame_release);


  /* check for dropped frames and disrupted signal */
  //dropped_frames = (circ_handle.FrameCount - src->last_frame_count) - 1;
  //if (dropped_frames > 0) {
  //  src->total_dropped_frames += dropped_frames;
  //  GST_WARNING_OBJECT (src, "Dropped %d frames (%d total)", dropped_frames,
  //      src->total_dropped_frames);
  //} else if (dropped_frames < 0) {
  //  GST_WARNING_OBJECT (src, "Frame count non-monotonic, signal disrupted?");
  //}
  //src->last_frame_count = circ_handle.FrameCount;

  GST_BUFFER_TIMESTAMP (*buf) =
      GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
      video_frame->clock_time);

  if (src->stop_requested) {
    if (*buf != NULL) {
      gst_buffer_unref (*buf);
      *buf = NULL;
    }
    return GST_FLOW_FLUSHING;
  }

  if (src->handle && src->send_settings) {
    gst_qcamsrc_set_exposure (src, src->exposure);
    gst_qcamsrc_set_gain (src, src->gain);
    gst_qcamsrc_set_offset (src, src->offset);
    QCam_QueueSettings (src->handle, &src->qsettings, NULL, 0, 0, 0);
    src->send_settings = FALSE;
  }

  return GST_FLOW_OK;
}

void
gst_qcamsrc_frame_callback (void *userPtr, unsigned long userData,
    QCam_Err errcode, unsigned long flags)
{
  VideoFrame *frame = (VideoFrame *) (userPtr);

  if (flags & qcCallbackExposeDone) {
    GstClock *clock = gst_element_get_clock (GST_ELEMENT (frame->src));
    frame->clock_time = gst_clock_get_time (clock);
    gst_object_unref (clock);
    GST_TRACE_OBJECT (frame->src, "ExposeDone callback for frame 0x%x", frame);
  } else if (flags & qcCallbackDone) {
    GST_TRACE_OBJECT (frame->src, "FrameDone callback for frame 0x%x", frame);

    if (errcode != qerrSuccess) {
      GST_WARNING_OBJECT (frame->src, "Error code in callback: %d", errcode);
    }

    g_async_queue_push (frame->src->queue, frame);
  } else {
    g_assert_not_reached ();
  }
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "qcam", 0,
      "debug category for qcam plugin");

  if (!gst_element_register (plugin, "qcamsrc", GST_RANK_NONE,
          gst_qcamsrc_get_type ())) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    qcam,
    "QImaging QCam video element",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
