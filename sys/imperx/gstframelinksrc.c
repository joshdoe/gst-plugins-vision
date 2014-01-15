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
 * SECTION:element-gstframelinksrc
 *
 * The framelinksrc element is a source for IMPERX FrameLink and FrameLink Express framegrabbers.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v framelinksrc ! ffmpegcolorspace ! autovideosink
 * ]|
 * Shows video from the default IMPERX FrameLink framegrabber
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#include "gstframelinksrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_framelinksrc_debug);
#define GST_CAT_DEFAULT gst_framelinksrc_debug

/* prototypes */
static void gst_framelinksrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_framelinksrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_framelinksrc_dispose (GObject * object);
static void gst_framelinksrc_finalize (GObject * object);

static gboolean gst_framelinksrc_start (GstBaseSrc * src);
static gboolean gst_framelinksrc_stop (GstBaseSrc * src);
static GstCaps *gst_framelinksrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_framelinksrc_set_caps (GstBaseSrc * src, GstCaps * caps);

static GstFlowReturn gst_framelinksrc_create (GstPushSrc * src,
    GstBuffer ** buf);

static GstCaps *gst_framelinksrc_create_caps (GstFramelinkSrc * src);
enum
{
  PROP_0,
  PROP_FORMAT_FILE,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_BOARD,
  PROP_CHANNEL
};

#define DEFAULT_PROP_FORMAT_FILE ""
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 2
#define DEFAULT_PROP_BOARD 0
#define DEFAULT_PROP_CHANNEL 0

/* pad templates */

static GstStaticPadTemplate gst_framelinksrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ GRAY8, GRAY16_LE, GRAY16_BE }"))
    );

/* class initialization */

G_DEFINE_TYPE (GstFramelinkSrc, gst_framelinksrc, GST_TYPE_PUSH_SRC);

static void
gst_framelinksrc_class_init (GstFramelinkSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_framelinksrc_set_property;
  gobject_class->get_property = gst_framelinksrc_get_property;
  gobject_class->dispose = gst_framelinksrc_dispose;
  gobject_class->finalize = gst_framelinksrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_framelinksrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "IMPERX FrameLink Express Video Source", "Source/Video",
      "IMPERX FrameLink Express framegrabber video source",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_framelinksrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_framelinksrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_framelinksrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_framelinksrc_set_caps);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_framelinksrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_FORMAT_FILE,
      g_param_spec_string ("format-file", "Format file",
          "Filepath of the video file for the selected camera "
          "(specify only one of format-name or format-file)",
          DEFAULT_PROP_FORMAT_FILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_uint ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 1, G_MAXUINT,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BOARD,
      g_param_spec_uint ("board", "Board", "Board number (0 for auto)", 0, 7,
          DEFAULT_PROP_BOARD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_uint ("channel", "Channel", "Channel number", 0,
          1, DEFAULT_PROP_CHANNEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_framelinksrc_init (GstFramelinkSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->format_file = g_strdup (DEFAULT_PROP_FORMAT_FILE);
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;

  src->grabber = NULL;

  src->buffer_ready = FALSE;
  src->buffer_processed_count = 0;
  src->acq_started = FALSE;

  src->caps = NULL;
  src->buffer = NULL;

  g_mutex_init (&src->mutex);
  g_cond_init (&src->cond);
}

void
gst_framelinksrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFramelinkSrc *src;

  src = GST_FRAMELINK_SRC (object);

  switch (property_id) {
    case PROP_FORMAT_FILE:
      g_free (src->format_file);
      src->format_file = g_strdup (g_value_get_string (value));
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      if (src->acq_started) {
        GST_ELEMENT_WARNING (src, RESOURCE, SETTINGS,
            ("Number of capture buffers cannot be changed after acquisition has started."),
            (NULL));
      } else {
        src->num_capture_buffers = g_value_get_uint (value);
      }
      break;
    case PROP_BOARD:
      src->board = g_value_get_uint (value);
      break;
    case PROP_CHANNEL:
      src->channel = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_framelinksrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstFramelinkSrc *src;

  g_return_if_fail (GST_IS_FRAMELINK_SRC (object));
  src = GST_FRAMELINK_SRC (object);

  switch (property_id) {
    case PROP_FORMAT_FILE:
      g_value_set_string (value, src->format_file);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      g_value_set_uint (value, src->num_capture_buffers);
      break;
    case PROP_BOARD:
      g_value_set_uint (value, src->board);
      break;
    case PROP_CHANNEL:
      g_value_set_uint (value, src->channel);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_framelinksrc_dispose (GObject * object)
{
  GstFramelinkSrc *src;

  g_return_if_fail (GST_IS_FRAMELINK_SRC (object));
  src = GST_FRAMELINK_SRC (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_framelinksrc_parent_class)->dispose (object);
}

void
gst_framelinksrc_finalize (GObject * object)
{
  GstFramelinkSrc *src;

  g_return_if_fail (GST_IS_FRAMELINK_SRC (object));
  src = GST_FRAMELINK_SRC (object);

  /* clean up object here */
  g_free (src->format_file);

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  if (src->buffer) {
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }

  G_OBJECT_CLASS (gst_framelinksrc_parent_class)->finalize (object);
}

static gboolean
gst_framelinksrc_start (GstBaseSrc * bsrc)
{
  GstFramelinkSrc *src = GST_FRAMELINK_SRC (bsrc);
  VCECLB_ConfigurationA camConfig;
  VCECLB_Error err;
  GstVideoInfo vinfo;
  int bpp;

  GST_DEBUG_OBJECT (src, "start");

  if (!strlen (src->format_file)) {
    GST_ERROR_OBJECT (src, "Format file must be specified");
    return FALSE;
  }

  if (!g_file_test (src->format_file, G_FILE_TEST_EXISTS)) {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        ("Format file does not exist: %s", src->format_file), (NULL));
    return FALSE;
  }

  /* get configuration from file */
  camConfig.cchAlias = MAX_PATH;
  camConfig.lpszAlias = g_new (gchar, MAX_PATH);
  camConfig.cchDescription = MAX_PATH;
  camConfig.lpszDescription = g_new (gchar, MAX_PATH);
  camConfig.cchManufacturer = MAX_PATH;
  camConfig.lpszManufacturer = g_new (gchar, MAX_PATH);
  camConfig.cchModel = MAX_PATH;
  camConfig.lpszModel = g_new (gchar, MAX_PATH);

  err = VCECLB_LoadConfigA (src->format_file, &camConfig);
  if (err != VCECLB_Err_Success) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to load configuration file: %s", src->format_file), (NULL));
    return FALSE;
  }

  GST_INFO_OBJECT (src, "Loaded config file for %s - %s",
      camConfig.lpszManufacturer, camConfig.lpszModel);
  g_free (camConfig.lpszAlias);
  g_free (camConfig.lpszDescription);
  g_free (camConfig.lpszManufacturer);
  g_free (camConfig.lpszModel);

  if (camConfig.pixelInfo.cameraData.Packed == 1) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        ("Packed pixel data not supported yet."), (NULL));
    return FALSE;
  }

  /* TODO: use VCECLB_InitByHandle */
  src->grabber = VCECLB_Init ();
  if (src->grabber == NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Failed to initialize grabber (code %d)", VCECLB_CardLastError ()),
        (NULL));
    return FALSE;
  }

  err = VCECLB_GetDMAAccessEx (src->grabber, src->channel);
  if (err != VCECLB_Err_Success) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Failed to get DMA access to port on grabber (code %d)", err), (NULL));
    return FALSE;
  }

  err =
      VCECLB_PrepareEx (src->grabber, src->channel,
      &camConfig.pixelInfo.cameraData);
  if (err != VCECLB_Err_Success) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        ("Failed to configure grabber (code %d)", err), (NULL));
    return FALSE;
  }

  /* create caps */
  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  gst_video_info_init (&vinfo);
  vinfo.width = camConfig.pixelInfo.cameraData.Width;
  vinfo.height = camConfig.pixelInfo.cameraData.Height;

  bpp = camConfig.pixelInfo.cameraData.BitDepth;
  if (bpp <= 8) {
    vinfo.finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_GRAY8);
    src->caps = gst_video_info_to_caps (&vinfo);

    src->flex_stride = vinfo.width;
  } else if (bpp > 8 && bpp <= 16) {
    GValue val = G_VALUE_INIT;
    GstStructure *s;

    if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
      vinfo.finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_GRAY16_LE);
    else if (G_BYTE_ORDER == G_BIG_ENDIAN)
      vinfo.finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_GRAY16_BE);
    src->caps = gst_video_info_to_caps (&vinfo);

    /* set bpp, extra info for GRAY16 so elements can scale properly */
    s = gst_caps_get_structure (src->caps, 0);
    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, bpp);
    gst_structure_set_value (s, "bpp", &val);
    g_value_unset (&val);

    src->flex_stride = vinfo.width * 2;
  } else {
    /* TODO: support 24-bit RGB */
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Unknown or unsupported bit depth (%d).", bpp), (NULL));
    return FALSE;
  }

  src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);
  src->height = vinfo.height;

  return TRUE;
}

static gboolean
gst_framelinksrc_stop (GstBaseSrc * bsrc)
{
  GstFramelinkSrc *src = GST_FRAMELINK_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "stop");

  if (src->acq_started) {
    VCECLB_StopGrabEx (src->grabber, src->channel);
    src->acq_started = FALSE;
  }

  if (src->grabber) {
    VCECLB_ReleaseDMAAccessEx (src->grabber, src->channel);
    VCECLB_Done (src->grabber);
    src->grabber = NULL;
  }

  src->dropped_frame_count = 0;

  return TRUE;
}

static GstCaps *
gst_framelinksrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstFramelinkSrc *src = GST_FRAMELINK_SRC (bsrc);
  GstCaps *caps;

  if (src->grabber == NULL) {
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
gst_framelinksrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstFramelinkSrc *src = GST_FRAMELINK_SRC (bsrc);
  GstVideoInfo vinfo;
  GstStructure *s = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);

  gst_video_info_from_caps (&vinfo, caps);

  if (GST_VIDEO_INFO_FORMAT (&vinfo) != GST_VIDEO_FORMAT_UNKNOWN) {
    src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);
    src->height = vinfo.height;
  } else {
    goto unsupported_caps;
  }

  return TRUE;

unsupported_caps:
  GST_ERROR_OBJECT (src, "Unsupported caps: %" GST_PTR_FORMAT, caps);
  return FALSE;
}

static GstBuffer *
gst_framelinksrc_create_buffer_from_frameinfo (GstFramelinkSrc * src,
    VCECLB_FrameInfoEx * pFrameInfo)
{
  GstMapInfo minfo;
  GstBuffer *buf;

  /* TODO: use allocator or use from pool */
  buf = gst_buffer_new_and_alloc (src->height * src->gst_stride);

  /* Copy image to buffer from surface TODO: use orc_memcpy */
  gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
  GST_LOG_OBJECT (src,
      "GstBuffer size=%d, gst_stride=%d, phx_stride=%d, number=%d, timestamp=%d",
      minfo.size, src->gst_stride, src->flex_stride, pFrameInfo->number,
      pFrameInfo->timestamp);

  if (src->gst_stride == src->flex_stride) {
    memcpy (minfo.data, pFrameInfo->lpRawBuffer, minfo.size);
  } else {
    int i;
    GST_WARNING_OBJECT (src,
        "Image stride not a multiple of 4, copy will be slower.");
    for (i = 0; i < src->height; i++) {
      memcpy (minfo.data + i * src->gst_stride,
          ((guint8 *) pFrameInfo->lpRawBuffer) + i * src->flex_stride,
          src->flex_stride);
    }
  }
  gst_buffer_unmap (buf, &minfo);

  GST_BUFFER_OFFSET (buf) = pFrameInfo->number;
  /* TODO: use timestamp? */
  /* GST_BUFFER_OFFSET (src->timestamp) = pFrameInfo->timestamp * G_GINT64_CONSTANT (1000); */

  return buf;
}

static void
gst_framelinksrc_callback (void *lpUserData, VCECLB_FrameInfoEx * pFrameInfo)
{
  GstFramelinkSrc *src;
  return;
  printf ("callback, status=%d, buffersize=%d, number=%d, timestamp=%d \n",
      pFrameInfo->dma_status, pFrameInfo->bufferSize, pFrameInfo->number,
      pFrameInfo->timestamp);
  src = GST_FRAMELINK_SRC (lpUserData);
  g_assert (src != NULL);

  if (pFrameInfo->dma_status == VCECLB_DMA_STATUS_FRAME_DROP) {
    /* TODO: save this in dropped frame total? */
    GST_WARNING_OBJECT (src, "Frame dropped from DMA system.");
    return;
  } else if (pFrameInfo->dma_status == VCECLB_DMA_STATUS_FIFO_OVERRUN) {
    GST_WARNING_OBJECT (src, "DMA system reports FIFO overrun");
    return;
  } else if (pFrameInfo->dma_status == VCECLB_DMA_STATUS_ABORTED) {
    GST_WARNING_OBJECT (src, "DMA system reports acquisition was aborted");
    return;
  } else if (pFrameInfo->dma_status == VCECLB_DMA_STATUS_DICONNECTED) {
    GST_WARNING_OBJECT (src, "DMA system reports camera is disconnected");
    return;
  } else if (pFrameInfo->dma_status != VCECLB_DMA_STATUS_OK) {
    GST_WARNING_OBJECT (src, "DMA system reports unknown error");
    return;
  }

  GST_LOG_OBJECT (src, "callback received for new buffer");

  g_mutex_lock (&src->mutex);

  if (src->buffer) {
    /* TODO: save this in dropped frame total? */
    GST_WARNING_OBJECT (src,
        "Got new buffer before old handled, dropping old.");
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }

  src->buffer = gst_framelinksrc_create_buffer_from_frameinfo (src, pFrameInfo);

  g_cond_signal (&src->cond);
  g_mutex_unlock (&src->mutex);
}

static GstFlowReturn
gst_framelinksrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstFramelinkSrc *src = GST_FRAMELINK_SRC (psrc);
  VCECLB_Error err;
  VCECLB_FrameInfoEx pFrameInfo;

  /* Start acquisition if not already started */
  if (!src->acq_started) {
#if GST_FRAMELINKSRC_USE_CALLBACK
    err =
        VCECLB_StartGrabEx (src->grabber, src->channel, 0,
        (VCECLB_GrabFrame_CallbackEx) gst_framelinksrc_callback, src);
#else
    err =
        VCECLB_StartGrabEx (src->grabber, src->channel, 0,
        (VCECLB_GrabFrame_CallbackEx) NULL, src);
#endif
    if (err != VCECLB_Err_Success) {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to start grabbing (code %d)", err), (NULL));
      return GST_FLOW_ERROR;
    }
    src->acq_started = TRUE;
  }
#if GST_FRAMELINKSRC_USE_CALLBACK
  /* wait for a buffer to be ready */
  g_mutex_lock (&src->mutex);
  while (!src->buffer) {
    /* TODO: add check for halted acquisition so we don't wait forever */
    g_cond_wait (&src->cond, &src->mutex);
  }

  if (src->buffer) {
    *buf = src->buffer;
    src->buffer = NULL;
  }
  g_mutex_unlock (&src->mutex);
#else
  pFrameInfo.lpRawBuffer = NULL;
  while (pFrameInfo.lpRawBuffer == NULL) {
    err = VCECLB_GetLastBufferData (src->grabber, src->channel, &pFrameInfo);
    if (err != VCECLB_Err_Success) {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to get last buffer (code %d)", err), (NULL));
      return GST_FLOW_ERROR;
    }
  }

  /* TODO: check for missed frames by comparing pFrameInfo.number */
  *buf = gst_framelinksrc_create_buffer_from_frameinfo (src, &pFrameInfo);
#endif

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_framelinksrc_debug, "framelinksrc", 0,
      "debug category for framelinksrc element");
  gst_element_register (plugin, "framelinksrc", GST_RANK_NONE,
      gst_framelinksrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    imperx,
    "IMPERX FrameLink Express frame grabber source",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
