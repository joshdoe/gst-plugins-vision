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
 * SECTION:element-gstimperxsdisrc
 *
 * The imperxsdisrc element is a source for IMPERX HD-SDI Express framegrabbers.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v imperxsdisrc ! videoconvert ! autovideosink
 * ]|
 * Shows video from the default IMPERX HD-SDI Express framegrabber
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

#include "gstimperxsdisrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_imperxsdisrc_debug);
#define GST_CAT_DEFAULT gst_imperxsdisrc_debug

#define PORT_VIDEO 0
#define PORT_AUDIO 1

/* prototypes */
static void gst_imperxsdisrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_imperxsdisrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_imperxsdisrc_dispose (GObject * object);
static void gst_imperxsdisrc_finalize (GObject * object);

static gboolean gst_imperxsdisrc_start (GstBaseSrc * src);
static gboolean gst_imperxsdisrc_stop (GstBaseSrc * src);
static GstCaps *gst_imperxsdisrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_imperxsdisrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_imperxsdisrc_unlock (GstBaseSrc * src);
static gboolean gst_imperxsdisrc_unlock_stop (GstBaseSrc * src);

static GstFlowReturn gst_imperxsdisrc_create (GstPushSrc * src,
    GstBuffer ** buf);

static GstCaps *gst_imperxsdisrc_create_caps (GstImperxSdiSrc * src);
enum
{
  PROP_0,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_BOARD,
  PROP_TIMEOUT
};

#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 3
#define DEFAULT_PROP_BOARD 0
#define DEFAULT_PROP_TIMEOUT 1000

/* pad templates */

static GstStaticPadTemplate gst_imperxsdisrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ v210, AYUV64, YUY2, BGR }"))
    );

/* class initialization */

G_DEFINE_TYPE (GstImperxSdiSrc, gst_imperxsdisrc, GST_TYPE_PUSH_SRC);

static void
gst_imperxsdisrc_class_init (GstImperxSdiSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_imperxsdisrc_set_property;
  gobject_class->get_property = gst_imperxsdisrc_get_property;
  gobject_class->dispose = gst_imperxsdisrc_dispose;
  gobject_class->finalize = gst_imperxsdisrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_imperxsdisrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "IMPERX HD-SDI Express Video Source", "Source/Video",
      "IMPERX HD-SDI Express framegrabber video source",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_imperxsdisrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_imperxsdisrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_imperxsdisrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_imperxsdisrc_set_caps);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_imperxsdisrc_unlock);
  gstbasesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_imperxsdisrc_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_imperxsdisrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_uint ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 3, G_MAXUINT,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BOARD,
      g_param_spec_uint ("board", "Board", "Board number", 0, 7,
          DEFAULT_PROP_BOARD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_TIMEOUT, g_param_spec_int ("timeout",
          "Timeout (ms)",
          "Timeout in ms (0 to use default)", 0, G_MAXINT,
          DEFAULT_PROP_TIMEOUT,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));

}

static void
gst_imperxsdisrc_reset (GstImperxSdiSrc * src)
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

  src->width = 0;
  src->height = 0;
  src->framerate = 0;
  src->is_interlaced = FALSE;
}

static void
gst_imperxsdisrc_init (GstImperxSdiSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;
  src->board = DEFAULT_PROP_BOARD;
  src->timeout = DEFAULT_PROP_TIMEOUT;

  g_mutex_init (&src->mutex);
  g_cond_init (&src->cond);
  src->stop_requested = FALSE;
  src->caps = NULL;
  src->buffer = NULL;

  gst_imperxsdisrc_reset (src);
}

void
gst_imperxsdisrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstImperxSdiSrc *src;

  src = GST_IMPERX_SDI_SRC (object);

  switch (property_id) {
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
    case PROP_TIMEOUT:
      src->timeout = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_imperxsdisrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstImperxSdiSrc *src;

  g_return_if_fail (GST_IS_IMPERX_SDI_SRC (object));
  src = GST_IMPERX_SDI_SRC (object);

  switch (property_id) {
    case PROP_NUM_CAPTURE_BUFFERS:
      g_value_set_uint (value, src->num_capture_buffers);
      break;
    case PROP_BOARD:
      g_value_set_uint (value, src->board);
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
gst_imperxsdisrc_dispose (GObject * object)
{
  GstImperxSdiSrc *src;

  g_return_if_fail (GST_IS_IMPERX_SDI_SRC (object));
  src = GST_IMPERX_SDI_SRC (object);

  /* clean up as possible.  may be called multiple times */

  g_mutex_clear (&src->mutex);
  g_cond_clear (&src->cond);

  G_OBJECT_CLASS (gst_imperxsdisrc_parent_class)->dispose (object);
}

void
gst_imperxsdisrc_finalize (GObject * object)
{
  GstImperxSdiSrc *src;

  g_return_if_fail (GST_IS_IMPERX_SDI_SRC (object));
  src = GST_IMPERX_SDI_SRC (object);

  /* clean up object here */

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  if (src->buffer) {
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }

  G_OBJECT_CLASS (gst_imperxsdisrc_parent_class)->finalize (object);
}

static const gchar *
gst_imperxsdisrc_get_error (VCESDI_Error err)
{
  switch (err) {
    case VCESDI_Err_Success:
      return "Operation successful";
    case VCESDI_Err_badArgument:
      return "Argument is invalid";
    case VCESDI_Err_noDriver:
      "Driver is not installed";
    case VCESDI_Err_noDevicePresent:
      return "Device is not present";
    case VCESDI_Err_DeviceBusy:
      return "Device busy or not responding";
    case VCESDI_Err_noMemory:
      return "Not enough memory to perform operation";
    case VCESDI_Err_notInitialized:
      return "Sub-component has not been initialized";
    case VCESDI_Err_notSupported:
      return "Operation is not supported";
    case VCESDI_Err_UnknownError:
      return "Unknown (system) error occurs";
    default:
      return "Invalid error code";
  }
}

static gboolean
gst_imperxsdisrc_start (GstBaseSrc * bsrc)
{
  GstImperxSdiSrc *src = GST_IMPERX_SDI_SRC (bsrc);
  VCESDI_Error err;
  VCESDI_EnumData enumData;
  VCESDI_ENUM hDevEnum;
  guint8 camera_connected;
  VCESDI_CameraStatus camera_status;
  gint fps_n, fps_d;

  GST_DEBUG_OBJECT (src, "start");

  /* enumerate devices */
  enumData.cbSize = sizeof (VCESDI_EnumData);
  hDevEnum = VCESDI_EnumInit ();
  while (VCESDI_EnumNext (hDevEnum, &enumData) == VCESDI_Err_Success) {
    GST_DEBUG_OBJECT (src, "Found device: slot #%d, name '%s'",
        enumData.dwSlot, enumData.pSlotName);

    if (enumData.dwSlot == src->board) {
      src->grabber = VCESDI_InitByHandle (enumData.deviceData);
      break;
    }
  }
  VCESDI_EnumClose (hDevEnum);

  if (src->grabber == NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Invalid board/device number or failed to initialize grabber"),
        (NULL));
    return FALSE;
  }

  err = VCESDI_IsCameraConnected (src->grabber, PORT_VIDEO, &camera_connected);
  if (err != VCESDI_Err_Success) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        ("Failed to get camera connected status (%s)",
            gst_imperxsdisrc_get_error (err)), (NULL));
    return FALSE;
  }

  if (camera_connected) {
    GST_DEBUG_OBJECT (src, "Camera has been detected");
  } else {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("No camera detected"), (NULL));
    return FALSE;
  }

  err = VCESDI_GetCameraStatus (src->grabber, PORT_VIDEO, &camera_status);
  if (err != VCESDI_Err_Success) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        ("Failed to get camera status (%s)", gst_imperxsdisrc_get_error (err)),
        (NULL));
    return FALSE;
  }

  src->width = camera_status.Width;
  src->height = camera_status.Height;
  src->framerate = camera_status.FrameRate;
  src->is_interlaced = (camera_status.Mode == VCESDI_CameraMode_Interlaced);

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }
  fps_n = (gint) (src->framerate * 1000);
  fps_d = 1000;
  src->caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  src->caps = gst_caps_make_writable (src->caps);
  gst_caps_set_simple (src->caps,
      "width", G_TYPE_INT, src->width,
      "height", G_TYPE_INT, src->height,
      "framerate", GST_TYPE_FRACTION, fps_n, fps_d, NULL);
  GST_DEBUG_OBJECT (src, "Detected video is %dx%d@%d", src->width, src->height,
      src->framerate);

  return TRUE;
}

static void
unpack_YVYU10 (gpointer dest, const gpointer data,
    const gint stride, gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = (guint8 *) (data) + stride * y;
  guint16 *restrict d = (guint16 *) dest;
  guint32 a0, a1, a2, a3;
  guint16 y0, y1, y2, y3, y4, y5;
  guint16 u0, u2, u4;
  guint16 v0, v2, v4;

  s += x * 2;
  for (i = 0; i < width; i += 6) {
    a0 = GST_READ_UINT32_LE (s + (i / 6) * 16 + 0);
    a1 = GST_READ_UINT32_LE (s + (i / 6) * 16 + 4);
    a2 = GST_READ_UINT32_LE (s + (i / 6) * 16 + 8);
    a3 = GST_READ_UINT32_LE (s + (i / 6) * 16 + 12);

    y0 = ((a0 >> 0) & 0x3ff) << 6;
    u0 = ((a0 >> 10) & 0x3ff) << 6;
    y1 = ((a0 >> 20) & 0x3ff) << 6;
    v0 = (((a0 >> 30) | (a1 << 2)) & 0x3ff) << 6;
    y2 = ((a1 >> 8) & 0x3ff) << 6;
    u2 = ((a1 >> 18) & 0x3ff) << 6;

    y3 = ((a2 >> 0) & 0x3ff) << 6;
    v2 = ((a2 >> 10) & 0x3ff) << 6;
    y4 = ((a2 >> 20) & 0x3ff) << 6;
    u4 = (((a2 >> 30) | (a3 << 2)) & 0x3ff) << 6;
    y5 = ((a3 >> 8) & 0x3ff) << 6;
    v4 = ((a3 >> 18) & 0x3ff) << 6;

    d[4 * (i + 0) + 0] = 0xffff;
    d[4 * (i + 0) + 1] = y0;
    d[4 * (i + 0) + 2] = u0;
    d[4 * (i + 0) + 3] = v0;

    if (i < width - 1) {
      d[4 * (i + 1) + 0] = 0xffff;
      d[4 * (i + 1) + 1] = y1;
      d[4 * (i + 1) + 2] = u0;
      d[4 * (i + 1) + 3] = v0;
    }
    if (i < width - 2) {
      d[4 * (i + 2) + 0] = 0xffff;
      d[4 * (i + 2) + 1] = y2;
      d[4 * (i + 2) + 2] = u2;
      d[4 * (i + 2) + 3] = v2;
    }
    if (i < width - 3) {
      d[4 * (i + 3) + 0] = 0xffff;
      d[4 * (i + 3) + 1] = y3;
      d[4 * (i + 3) + 2] = u2;
      d[4 * (i + 3) + 3] = v2;
    }
    if (i < width - 4) {
      d[4 * (i + 4) + 0] = 0xffff;
      d[4 * (i + 4) + 1] = y4;
      d[4 * (i + 4) + 2] = u4;
      d[4 * (i + 4) + 3] = v4;
    }
    if (i < width - 5) {
      d[4 * (i + 5) + 0] = 0xffff;
      d[4 * (i + 5) + 1] = y5;
      d[4 * (i + 5) + 2] = u4;
      d[4 * (i + 5) + 3] = v4;
    }
  }
}

static void
repack_YVYU10_to_v210 (gpointer dest, const gpointer data,
    const gint stride, gint x, gint y, gint width)
{
  int i;
  const guint8 *restrict s = (guint8 *) (data) + stride * y;
  guint32 *restrict d = (guint32 *) dest;
  guint32 a0, a1, a2, a3;
  guint16 y0, y1, y2, y3, y4, y5;
  guint16 u0, u2, u4;
  guint16 v0, v2, v4;

  s += x * 2;
  for (i = 0; i < width; i += 4) {
    a0 = GST_READ_UINT32_LE (s + (i / 4) * 16 + 0);
    a1 = GST_READ_UINT32_LE (s + (i / 4) * 16 + 4);
    a2 = GST_READ_UINT32_LE (s + (i / 4) * 16 + 8);
    a3 = GST_READ_UINT32_LE (s + (i / 4) * 16 + 12);

    y0 = ((a0 >> 0) & 0x3ff);
    u0 = ((a0 >> 10) & 0x3ff);
    y1 = ((a0 >> 20) & 0x3ff);
    v0 = (((a0 >> 30) | (a1 << 2)) & 0x3ff);
    y2 = ((a1 >> 8) & 0x3ff);
    u2 = ((a1 >> 18) & 0x3ff);

    y3 = ((a2 >> 0) & 0x3ff);
    v2 = ((a2 >> 10) & 0x3ff);
    y4 = ((a2 >> 20) & 0x3ff);
    u4 = (((a2 >> 30) | (a3 << 2)) & 0x3ff);
    y5 = ((a3 >> 8) & 0x3ff);
    v4 = ((a3 >> 18) & 0x3ff);

    d[i + 0] = (v0 << 20) | (y0 << 10) | u0;
    d[i + 1] = (y2 << 20) | (u2 << 10) | y1;
    d[i + 2] = (u4 << 20) | (y3 << 10) | v2;
    d[i + 3] = (y5 << 20) | (v4 << 10) | y4;
  }
}

static GstBuffer *
gst_imperxsdisrc_create_buffer_from_frameinfo (GstImperxSdiSrc * src,
    VCESDI_FrameInfo * pFrameInfo)
{
  GstMapInfo minfo;
  GstBuffer *buf;
  int buffer_size;

  if (src->is_interlaced
      && src->camera_data.Format != VCESDI_OutputFormat_YCrCb10) {
    VCESDI_Decode (&src->camera_data, NULL, NULL, &src->imperx_stride,
        src->camera_data.Format, NULL, NULL);
    buffer_size = src->camera_data.CameraStatus.Height * src->imperx_stride;
  } else {
    buffer_size = src->height * src->gst_stride;
  }

  buf = gst_buffer_new_and_alloc (buffer_size);

  /* Copy image to buffer from surface */
  gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
  GST_LOG_OBJECT (src,
      "SrcSize=%d, SrcStride=%d, DstSize=%d, DstStride=%d, number=%d, timestamp=%d",
      pFrameInfo->bufferSize, src->imperx_stride, minfo.size, src->gst_stride,
      pFrameInfo->number, pFrameInfo->timestamp);

  if (src->is_interlaced
      && src->camera_data.Format != VCESDI_OutputFormat_YCrCb10) {
    VCESDI_Decode (&src->camera_data, pFrameInfo->lpRawBuffer, minfo.data,
        &src->imperx_stride, src->camera_data.Format, NULL, NULL);
  } else {
    if (src->format == GST_VIDEO_FORMAT_AYUV64) {
      gint row;
      if (src->is_interlaced) {
        gint hh = src->height / 2;
        /* assumes even number of rows, which is the case for all formats */
        for (row = 0; row < hh; row++) {
          unpack_YVYU10 (minfo.data + src->gst_stride * (row * 2),
              pFrameInfo->lpRawBuffer, src->imperx_stride, 0, row, src->width);
          unpack_YVYU10 (minfo.data + src->gst_stride * (row * 2 + 1),
              pFrameInfo->lpRawBuffer, src->imperx_stride, 0, row + hh,
              src->width);
        }
      } else {
        for (row = 0; row < src->height; row++) {
          unpack_YVYU10 (minfo.data + src->gst_stride * row,
              pFrameInfo->lpRawBuffer, src->imperx_stride, 0, row, src->width);
        }
      }
    } else if (src->format == GST_VIDEO_FORMAT_v210) {
      gint row;
      if (src->is_interlaced) {
        gint hh = src->height / 2;
        /* assumes even number of rows, which is the case for all formats */
        for (row = 0; row < hh; row++) {
          repack_YVYU10_to_v210 (minfo.data + src->gst_stride * (row * 2),
              pFrameInfo->lpRawBuffer, src->imperx_stride, 0, row, src->width);
          repack_YVYU10_to_v210 (minfo.data + src->gst_stride * (row * 2 + 1),
              pFrameInfo->lpRawBuffer, src->imperx_stride, 0, row + hh,
              src->width);
        }
      } else {
        for (row = 0; row < src->height; row++) {
          repack_YVYU10_to_v210 (minfo.data + src->gst_stride * row,
              pFrameInfo->lpRawBuffer, src->imperx_stride, 0, row, src->width);
        }
      }
    } else {
      /* all supported formats should already be 4-byte aligned */
      g_assert (pFrameInfo->bufferSize >= minfo.size);
      orc_memcpy (minfo.data, pFrameInfo->lpRawBuffer, minfo.size);
    }
  }

  gst_buffer_unmap (buf, &minfo);

  return buf;
}

static void __stdcall
gst_imperxsdisrc_callback (void *lpUserData, VCESDI_FrameInfo * pFrameInfo)
{
  GstImperxSdiSrc *src = GST_IMPERX_SDI_SRC (lpUserData);
  gint dropped_frames;
  static guint32 last_frame_number = 0;
  static guint64 buffers_processed = 0;
  static guint64 total_dropped_frames = 0;

  g_assert (src != NULL);

  /* check for DMA errors */
  if (pFrameInfo->dma_status == DMA_STATUS_FRAME_DROPED) {
    /* TODO: save this in dropped frame total? */
    GST_WARNING_OBJECT (src, "Frame dropped from DMA system.");
    return;
  } else if (pFrameInfo->dma_status == DMA_STATUS_FIFO_OVERRUN) {
    GST_WARNING_OBJECT (src, "DMA system reports FIFO overrun");
    return;
  } else if (pFrameInfo->dma_status == DMA_STATUS_ABORTED) {
    GST_WARNING_OBJECT (src, "DMA system reports acquisition was aborted");
    return;
  } else if (pFrameInfo->dma_status == DMA_STATUS_DICONNECTED) {
    GST_WARNING_OBJECT (src, "DMA system reports camera is disconnected");
    return;
  } else if (pFrameInfo->dma_status != DMA_STATUS_OK) {
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

  src->buffer = gst_imperxsdisrc_create_buffer_from_frameinfo (src, pFrameInfo);

  GST_BUFFER_TIMESTAMP (src->buffer) =
      GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
      src->acq_start_time + pFrameInfo->timestamp * GST_USECOND);
  GST_BUFFER_OFFSET (src->buffer) = buffers_processed;
  ++buffers_processed;

  g_cond_signal (&src->cond);
  g_mutex_unlock (&src->mutex);
}

static gboolean
gst_imperxsdisrc_start_grab (GstImperxSdiSrc * src)
{
  GstBaseSrc *basesrc = GST_BASE_SRC (src);
  GstCaps *peercaps;
  VCESDI_Error err;
  GstVideoInfo vinfo;

  /* we've already fixated width, height, framerate */
  peercaps = gst_pad_peer_query_caps (GST_BASE_SRC_PAD (basesrc), src->caps);
  peercaps = gst_caps_fixate (peercaps);
  gst_caps_replace (&src->caps, peercaps);
  gst_video_info_from_caps (&vinfo, src->caps);
  src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);

  src->format = GST_VIDEO_INFO_FORMAT (&vinfo);
  switch (src->format) {
    case GST_VIDEO_FORMAT_v210:
    case GST_VIDEO_FORMAT_AYUV64:
      src->camera_data.Format = VCESDI_OutputFormat_YCrCb10;
      /* 12 components (6 pixels), in 16 bytes */
      src->imperx_stride = GST_ROUND_UP_4 ((int) ((src->width * 16) / 6.0));
      break;
    case GST_VIDEO_FORMAT_YUY2:
      src->camera_data.Format = VCESDI_OutputFormat_YCrCb8;
      src->imperx_stride = src->width * 2;
      break;
    case GST_VIDEO_FORMAT_BGR:
      src->camera_data.Format = VCESDI_OutputFormat_RGB24;
      src->imperx_stride = src->width * 3;
      break;
    default:
      g_assert_not_reached ();
  }

  err = VCESDI_GetDMAAccess (src->grabber, PORT_VIDEO);
  if (err != VCESDI_Err_Success) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Failed to get DMA access to port on grabber (%s)",
            gst_imperxsdisrc_get_error (err)), (NULL));
    return FALSE;
  }

  err =
      VCESDI_SetBufferCount (src->grabber, PORT_VIDEO,
      src->num_capture_buffers);
  if (err != VCESDI_Err_Success) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Failed to set buffer count (%s)", gst_imperxsdisrc_get_error (err)),
        (NULL));
    return FALSE;
  }

  src->camera_data.TransferMode = VCESDI_TransferMode_Frames;
  err =
      VCESDI_StartGrab (src->grabber, PORT_VIDEO, 0, &src->camera_data,
      (VCESDI_GrabFrame_Callback) gst_imperxsdisrc_callback, src);

  return TRUE;
}

static gboolean
gst_imperxsdisrc_stop (GstBaseSrc * bsrc)
{
  GstImperxSdiSrc *src = GST_IMPERX_SDI_SRC (bsrc);
  VCESDI_Error err;

  GST_DEBUG_OBJECT (src, "stop");

  if (src->acq_started) {
    err = VCESDI_StopGrab (src->grabber, PORT_VIDEO);
    if (err) {
      GST_WARNING_OBJECT (src, "Error stopping grab: '%s'",
          gst_imperxsdisrc_get_error (err));
    }
    src->acq_started = FALSE;
  }

  if (src->grabber) {
    err = VCESDI_ReleaseDMAAccess (src->grabber, PORT_VIDEO);
    if (err) {
      GST_WARNING_OBJECT (src, "Error releasing DMA accessx: %s",
          gst_imperxsdisrc_get_error (err));
    }
    err = VCESDI_Done (src->grabber);
    if (err) {
      GST_WARNING_OBJECT (src, "Error closing grabber: %s",
          gst_imperxsdisrc_get_error (err));
    }
    src->grabber = NULL;
  }

  gst_imperxsdisrc_reset (src);

  return TRUE;
}

static GstCaps *
gst_imperxsdisrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstImperxSdiSrc *src = GST_IMPERX_SDI_SRC (bsrc);
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
gst_imperxsdisrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstImperxSdiSrc *src = GST_IMPERX_SDI_SRC (bsrc);
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
gst_imperxsdisrc_unlock (GstBaseSrc * bsrc)
{
  GstImperxSdiSrc *src = GST_IMPERX_SDI_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock");

  g_mutex_lock (&src->mutex);
  src->stop_requested = TRUE;
  g_cond_signal (&src->cond);
  g_mutex_unlock (&src->mutex);

  return TRUE;
}

static gboolean
gst_imperxsdisrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstImperxSdiSrc *src = GST_IMPERX_SDI_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock_stop");

  src->stop_requested = FALSE;

  return TRUE;
}

static GstFlowReturn
gst_imperxsdisrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstImperxSdiSrc *src = GST_IMPERX_SDI_SRC (psrc);
  gint64 end_time;

  GST_LOG_OBJECT (src, "create");

  /* Start acquisition if not already started */
  if (G_UNLIKELY (!src->acq_started)) {
    GST_LOG_OBJECT (src, "starting acquisition");
    src->acq_start_time =
        gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src)));
    if (!gst_imperxsdisrc_start_grab (src)) {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to start grabbing"), (NULL));
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
  GST_DEBUG_CATEGORY_INIT (gst_imperxsdisrc_debug, "imperxsdisrc", 0,
      "debug category for imperxsdisrc element");
  gst_element_register (plugin, "imperxsdisrc", GST_RANK_NONE,
      gst_imperxsdisrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    imperxsdi,
    "IMPERX HD-SDI Express frame grabber source",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
