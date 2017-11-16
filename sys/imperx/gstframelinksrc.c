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
 * SECTION:element-gstimperxflexsrc
 *
 * The imperxflexsrc element is a source for IMPERX and FrameLink Express framegrabbers.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v imperxflexsrc ! videoconvert ! autovideosink
 * ]|
 * Shows video from the default IMPERX FrameLink Express framegrabber
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
static gboolean gst_framelinksrc_unlock (GstBaseSrc * src);
static gboolean gst_framelinksrc_unlock_stop (GstBaseSrc * src);

static GstFlowReturn gst_framelinksrc_create (GstPushSrc * src,
    GstBuffer ** buf);

static GstCaps *gst_framelinksrc_create_caps (GstFramelinkSrc * src);
enum
{
  PROP_0,
  PROP_FORMAT_FILE,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_BOARD,
  PROP_CHANNEL,
  PROP_TIMEOUT
};

#define DEFAULT_PROP_FORMAT_FILE ""
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 2
#define DEFAULT_PROP_BOARD 0
#define DEFAULT_PROP_CHANNEL 0
#define DEFAULT_PROP_TIMEOUT 1000

/* pad templates */

static GstStaticPadTemplate gst_framelinksrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, GRAY16_BE, BGRA }"))
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
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_framelinksrc_unlock);
  gstbasesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_framelinksrc_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_framelinksrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_FORMAT_FILE,
      g_param_spec_string ("config-file", "Config file",
          "Filepath of the video file for the selected camera",
          DEFAULT_PROP_FORMAT_FILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_uint ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 1, G_MAXUINT,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BOARD,
      g_param_spec_uint ("board", "Board", "Board number", 0, 7,
          DEFAULT_PROP_BOARD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_uint ("channel", "Channel", "Channel number", 0,
          1, DEFAULT_PROP_CHANNEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_TIMEOUT, g_param_spec_int ("timeout",
          "Timeout (ms)",
          "Timeout in ms (0 to use default)", 0, G_MAXINT,
          DEFAULT_PROP_TIMEOUT, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

}

static void
gst_framelinksrc_reset (GstFramelinkSrc * src)
{
  g_assert (src->grabber == NULL);

  src->acq_started = FALSE;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }
  if (src->buffer) {
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }
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
  src->board = DEFAULT_PROP_BOARD;
  src->channel = DEFAULT_PROP_CHANNEL;
  src->timeout = DEFAULT_PROP_TIMEOUT;

  g_mutex_init (&src->mutex);
  g_cond_init (&src->cond);
  src->stop_requested = FALSE;
  src->caps = NULL;
  src->buffer = NULL;

  gst_framelinksrc_reset (src);
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
    case PROP_TIMEOUT:
      src->timeout = g_value_get_int (value);
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
    case PROP_TIMEOUT:
      g_value_set_int (value, src->timeout);
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

  g_mutex_clear (&src->mutex);
  g_cond_clear (&src->cond);

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
  VCECLB_CameraDataEx *ci;
  VCECLB_EnumData enumData;
  HANDLE hDevEnum;

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

  /* use shortcut since we use this struct a lot */
  ci = &camConfig.pixelInfo.cameraData;

  /* copy pixel info struct to be used when unpacking data */
  memcpy (&src->pixInfo, &camConfig.pixelInfo, sizeof (VCECLB_RawPixelInfoEx));

  if (ci->Packed == 1) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        ("Packed pixel data not supported yet."), (NULL));
    return FALSE;
  }

  /* enumerate devices */
  enumData.cbSize = sizeof (VCECLB_EnumData);
  hDevEnum = VCECLB_EnumInit ();
  while (VCECLB_EnumNext (hDevEnum, &enumData) == VCECLB_Err_Success) {
    GST_DEBUG_OBJECT (src, "Found device: slot #%d, name '%s'",
        enumData.dwSlot, enumData.pSlotName);

    if (enumData.dwSlot == src->board) {
      src->grabber = VCECLB_InitByHandle (enumData.pDeviceData, 0);
      break;
    }
  }
  VCECLB_EnumClose (hDevEnum);

  if (src->grabber == NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Invalid board/device number or failed to initialize grabber (code %d)",
            VCECLB_CardLastError ()), (NULL));
    return FALSE;
  }

  err = VCECLB_GetDMAAccessEx (src->grabber, src->channel);
  if (err != VCECLB_Err_Success) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Failed to get DMA access to port on grabber (code %d)", err), (NULL));
    return FALSE;
  }

  err = VCECLB_PrepareEx (src->grabber, src->channel, ci);
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

  ci->BitDepth = ci->BitDepth;
  if (ci->BitDepth <= 8) {
    GstVideoFormat format;
    if (src->pixInfo.BayerPattern == 0) {
      format = GST_VIDEO_FORMAT_GRAY8;
    } else if (src->pixInfo.BayerPattern == 1 || src->pixInfo.BayerPattern == 2) {
      /* Bayer and TRUESENSE will be demosaiced by Imperx into BGRA */
      format = GST_VIDEO_FORMAT_BGRA;
    }
    gst_video_info_set_format (&vinfo, format, ci->Width, ci->Height);
    src->caps = gst_video_info_to_caps (&vinfo);
  } else if (ci->BitDepth > 8 && ci->BitDepth <= 16) {
    GstVideoFormat format;
    GValue val = G_VALUE_INIT;
    GstStructure *s;

    if (src->pixInfo.BayerPattern != 0) {
      GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
          ("Bayer greater than 8-bit not supported yet."), (NULL));
      return FALSE;
    }

    if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
      format = GST_VIDEO_FORMAT_GRAY16_LE;
    } else if (G_BYTE_ORDER == G_BIG_ENDIAN) {
      format = GST_VIDEO_FORMAT_GRAY16_BE;
    }
    gst_video_info_set_format (&vinfo, format, ci->Width, ci->Height);
    src->caps = gst_video_info_to_caps (&vinfo);

    /* set bpp, extra info for GRAY16 so elements can scale properly */
    s = gst_caps_get_structure (src->caps, 0);
    g_value_init (&val, G_TYPE_INT);
    g_value_set_int (&val, ci->BitDepth);
    gst_structure_set_value (s, "bpp", &val);
    g_value_unset (&val);
  } else if (ci->BitDepth == 24) {
    gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_BGRA, ci->Width,
        ci->Height);
    src->caps = gst_video_info_to_caps (&vinfo);
  } else {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Unknown or unsupported bit depth (%d).", ci->BitDepth), (NULL));
    return FALSE;
  }

  src->height = vinfo.height;
  src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);

  return TRUE;
}

static gboolean
gst_framelinksrc_stop (GstBaseSrc * bsrc)
{
  GstFramelinkSrc *src = GST_FRAMELINK_SRC (bsrc);
  VCECLB_Error err;

  GST_DEBUG_OBJECT (src, "stop");

  if (src->acq_started) {
    err = VCECLB_StopGrabEx (src->grabber, src->channel);
    if (err) {
      GST_WARNING_OBJECT (src, "Error calling VCECLB_StopGrabEx: %d", err);
    }
    src->acq_started = FALSE;
  }

  if (src->grabber) {
    err = VCECLB_ReleaseDMAAccessEx (src->grabber, src->channel);
    if (err) {
      GST_WARNING_OBJECT (src, "Error calling VCECLB_ReleaseDMAAccessEx: %d",
          err);
    }
    err = VCECLB_Done (src->grabber);
    if (err) {
      GST_WARNING_OBJECT (src, "Error calling VCECLB_Done: %d", err);
    }
    src->grabber = NULL;
  }

  gst_framelinksrc_reset (src);

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
  } else {
    goto unsupported_caps;
  }

  return TRUE;

unsupported_caps:
  GST_ERROR_OBJECT (src, "Unsupported caps: %" GST_PTR_FORMAT, caps);
  return FALSE;
}

static gboolean
gst_framelinksrc_unlock (GstBaseSrc * bsrc)
{
  GstFramelinkSrc *src = GST_FRAMELINK_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock");

  g_mutex_lock (&src->mutex);
  src->stop_requested = TRUE;
  g_cond_signal (&src->cond);
  g_mutex_unlock (&src->mutex);

  return TRUE;
}

static gboolean
gst_framelinksrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstFramelinkSrc *src = GST_FRAMELINK_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock_stop");

  src->stop_requested = FALSE;

  return TRUE;
}

static GstBuffer *
gst_framelinksrc_create_buffer_from_frameinfo (GstFramelinkSrc * src,
    VCECLB_FrameInfoEx * pFrameInfo)
{
  GstMapInfo minfo;
  GstBuffer *buf;
  INT_PTR strideSize;
  unsigned long outputBitDepth;
  VCECLB_Error err;
  unsigned char outputFormat;

  /* TODO: use allocator or use from pool */
  buf = gst_buffer_new_and_alloc (src->height * src->gst_stride);

  /* Copy image to buffer from surface */
  gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
  GST_LOG_OBJECT (src,
      "GstBuffer size=%d, gst_stride=%d, number=%d, timestamp=%d",
      minfo.size, src->gst_stride, pFrameInfo->number, pFrameInfo->timestamp);

#if GST_FRAMELINKSRC_COPY_EXPLICITLY
  guint flex_stride =
      (ci->WidthPreValid + ci->Width + ci->WidthPostValid) * Bpp;
  gint widthBytesPreValid = ci->WidthPreValid * Bpp;
  gint widthBytes = ci->Width * Bpp;
  gint heightPreValid = ci->HeightPreValid;
  /* TODO: use orc_memcpy */
  if (src->gst_stride == flex_stride) {
    memcpy (minfo.data,
        ((guint8 *) pFrameInfo->lpRawBuffer) +
        flex_stride * heightPreValid, minfo.size);
  } else {
    int i;
    GST_LOG_OBJECT (src, "Image strides not identical, copy will be slower.");
    for (i = 0; i < src->height; i++) {
      memcpy (minfo.data + i * src->gst_stride,
          ((guint8 *) pFrameInfo->lpRawBuffer) +
          (heightPreValid + i) * flex_stride + widthBytesPreValid, widthBytes);
    }
  }
#else
  outputFormat =
      VCECLB_EX_FMT_16BIT | VCECLB_EX_FMT_TopDown | VCECLB_EX_FMT_4Channel;
  strideSize = src->gst_stride;
  err =
      VCECLB_UnpackRawPixelsEx (&src->pixInfo, pFrameInfo->lpRawBuffer,
      minfo.data, &strideSize, outputFormat, &outputBitDepth);
  if (err != VCECLB_Err_Success) {
    GST_ELEMENT_ERROR (src, STREAM, DECODE,
        ("Failed to unpack raw pixels (code %d)", err), (NULL));
    goto Error;
  }
#endif
  gst_buffer_unmap (buf, &minfo);

  return buf;

Error:
  if (minfo.memory != NULL)
    gst_buffer_unmap (buf, &minfo);

  if (buf)
    gst_buffer_unref (buf);

  return NULL;
}

static void __stdcall
gst_framelinksrc_callback (void *lpUserData, VCECLB_FrameInfoEx * pFrameInfo)
{
  GstFramelinkSrc *src = GST_FRAMELINK_SRC (lpUserData);
  gint dropped_frames;
  static guint64 last_frame_number = 0;
  static guint64 buffers_processed = 0;
  static guint64 total_dropped_frames = 0;

  g_assert (src != NULL);

  /* check for DMA errors */
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

  /* check for dropped frames and disrupted signal */
  dropped_frames = (pFrameInfo->number - last_frame_number) - 1;
  if (dropped_frames > 0) {
    total_dropped_frames += dropped_frames;
    GST_WARNING_OBJECT (src, "Dropped %d frames (%d total)", dropped_frames,
        total_dropped_frames);
  } else if (dropped_frames < 0) {
    GST_WARNING_OBJECT (src,
        "Signal disrupted, frames likely dropped and timestamps inaccurate");

    /* frame timestamps reset, so adjust start time, accuracy reduced */
    src->acq_start_time =
        gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src))) -
        pFrameInfo->timestamp * GST_USECOND;
  }
  last_frame_number = pFrameInfo->number;

  g_mutex_lock (&src->mutex);

  if (src->buffer) {
    /* TODO: save this in dropped frame total? */
    GST_WARNING_OBJECT (src,
        "Got new buffer before old handled, dropping old.");
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }

  src->buffer = gst_framelinksrc_create_buffer_from_frameinfo (src, pFrameInfo);

  GST_BUFFER_TIMESTAMP (src->buffer) =
      GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
      src->acq_start_time + pFrameInfo->timestamp * GST_USECOND);
  GST_BUFFER_OFFSET (src->buffer) = buffers_processed;
  ++buffers_processed;

  g_cond_signal (&src->cond);
  g_mutex_unlock (&src->mutex);
}

static GstFlowReturn
gst_framelinksrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstFramelinkSrc *src = GST_FRAMELINK_SRC (psrc);
  VCECLB_Error err;
  gint64 end_time;

  GST_LOG_OBJECT (src, "create");

  /* Start acquisition if not already started */
  if (G_UNLIKELY (!src->acq_started)) {
    GST_LOG_OBJECT (src, "starting acquisition");
    src->acq_start_time =
        gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src)));
    err =
        VCECLB_StartGrabEx (src->grabber, src->channel, 0,
        gst_framelinksrc_callback, src);

    if (err != VCECLB_Err_Success) {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to start grabbing (code %d)", err), (NULL));
      return GST_FLOW_ERROR;
    }
    src->acq_started = TRUE;
  }

  /* wait for a buffer to be ready */
  g_mutex_lock (&src->mutex);
  end_time = g_get_monotonic_time () + src->timeout * G_TIME_SPAN_MILLISECOND;
  while (!src->buffer && !src->stop_requested) {
    if (!g_cond_wait_until (&src->cond, &src->mutex, end_time)) {
      g_mutex_unlock (&src->mutex);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Timeout, no data received after %d ms", src->timeout), (NULL));
      return GST_FLOW_ERROR;
    }
  }
  *buf = src->buffer;
  src->buffer = NULL;
  g_mutex_unlock (&src->mutex);

  if (src->stop_requested) {
    if (*buf != NULL) {
      gst_buffer_unref (*buf);
      *buf = NULL;
    }
    return GST_FLOW_FLUSHING;
  }

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_framelinksrc_debug, "imperxflexsrc", 0,
      "debug category for framelinksrc element");
  gst_element_register (plugin, "imperxflexsrc", GST_RANK_NONE,
      gst_framelinksrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    imperx,
    "IMPERX FrameLink Express frame grabber source",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
