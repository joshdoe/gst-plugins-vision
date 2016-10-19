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
 * SECTION:element-gstbitflowsrc
 *
 * The bitflowsrc element is a source for BitFlow framegrabbers.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v bitflowsrc ! videoconvert ! autovideosink
 * ]|
 * Shows video from the default BitFlow framegrabber
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#include "gstbitflowsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_bitflowsrc_debug);
#define GST_CAT_DEFAULT gst_bitflowsrc_debug

/* prototypes */
static void gst_bitflowsrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_bitflowsrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_bitflowsrc_dispose (GObject * object);
static void gst_bitflowsrc_finalize (GObject * object);

static gboolean gst_bitflowsrc_start (GstBaseSrc * src);
static gboolean gst_bitflowsrc_stop (GstBaseSrc * src);
static GstCaps *gst_bitflowsrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_bitflowsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_bitflowsrc_unlock (GstBaseSrc * src);
static gboolean gst_bitflowsrc_unlock_stop (GstBaseSrc * src);

static GstFlowReturn gst_bitflowsrc_create (GstPushSrc * src, GstBuffer ** buf);

static gchar *gst_bitflowsrc_get_error_string (GstBitflowSrc * src,
    BIRC error_num);

enum
{
  PROP_0,
  PROP_CAMERA_FILE,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_BOARD,
  PROP_TIMEOUT
};

#define DEFAULT_PROP_CAMERA_FILE ""
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 3
#define DEFAULT_PROP_BOARD 0
#define DEFAULT_PROP_TIMEOUT 1000

/* pad templates */

static GstStaticPadTemplate gst_bitflowsrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, GRAY16_BE, BGRA }"))
    );

/* class initialization */

G_DEFINE_TYPE (GstBitflowSrc, gst_bitflowsrc, GST_TYPE_PUSH_SRC);

static void
gst_bitflowsrc_class_init (GstBitflowSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_bitflowsrc_set_property;
  gobject_class->get_property = gst_bitflowsrc_get_property;
  gobject_class->dispose = gst_bitflowsrc_dispose;
  gobject_class->finalize = gst_bitflowsrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_bitflowsrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "BitFlow Video Source", "Source/Video",
      "BitFlow framegrabber video source", "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_bitflowsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_bitflowsrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_bitflowsrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_bitflowsrc_set_caps);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_bitflowsrc_unlock);
  gstbasesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_bitflowsrc_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_bitflowsrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_CAMERA_FILE,
      g_param_spec_string ("config-file", "Config file",
          "Filepath of the video file for the selected camera",
          DEFAULT_PROP_CAMERA_FILE,
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
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_TIMEOUT, g_param_spec_int ("timeout",
          "Timeout (ms)",
          "Timeout in ms (0 to use default)", 0, G_MAXINT,
          DEFAULT_PROP_TIMEOUT, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

}

static void
gst_bitflowsrc_reset (GstBitflowSrc * src)
{
  src->board = NULL;
  memset (&src->buffer_array, 0, sizeof (src->buffer_array));
  src->error_string[0] = 0;
  src->last_frame_count = 0;
  src->total_dropped_frames = 0;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }
}

static void
gst_bitflowsrc_init (GstBitflowSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->camera_file = g_strdup (DEFAULT_PROP_CAMERA_FILE);
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;
  src->board_index = DEFAULT_PROP_BOARD;
  src->timeout = DEFAULT_PROP_TIMEOUT;

  src->stop_requested = FALSE;
  src->caps = NULL;

  gst_bitflowsrc_reset (src);
}

void
gst_bitflowsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBitflowSrc *src;

  src = GST_BITFLOW_SRC (object);

  switch (property_id) {
    case PROP_CAMERA_FILE:
      g_free (src->camera_file);
      src->camera_file = g_strdup (g_value_get_string (value));
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      src->num_capture_buffers = g_value_get_uint (value);
      break;
    case PROP_BOARD:
      src->board_index = g_value_get_uint (value);
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
gst_bitflowsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstBitflowSrc *src;

  g_return_if_fail (GST_IS_BITFLOW_SRC (object));
  src = GST_BITFLOW_SRC (object);

  switch (property_id) {
    case PROP_CAMERA_FILE:
      g_value_set_string (value, src->camera_file);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      g_value_set_uint (value, src->num_capture_buffers);
      break;
    case PROP_BOARD:
      g_value_set_uint (value, src->board_index);
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
gst_bitflowsrc_dispose (GObject * object)
{
  GstBitflowSrc *src;

  g_return_if_fail (GST_IS_BITFLOW_SRC (object));
  src = GST_BITFLOW_SRC (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_bitflowsrc_parent_class)->dispose (object);
}

void
gst_bitflowsrc_finalize (GObject * object)
{
  GstBitflowSrc *src;

  g_return_if_fail (GST_IS_BITFLOW_SRC (object));
  src = GST_BITFLOW_SRC (object);

  /* clean up object here */
  g_free (src->camera_file);

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  G_OBJECT_CLASS (gst_bitflowsrc_parent_class)->finalize (object);
}

static gboolean
gst_bitflowsrc_start (GstBaseSrc * bsrc)
{
  GstBitflowSrc *src = GST_BITFLOW_SRC (bsrc);
  BFRC ret;
  guint32 width, height, bpp, stride;
  GstVideoInfo vinfo;

  GST_DEBUG_OBJECT (src, "start");

  if (strlen (src->camera_file)) {
    if (!g_file_test (src->camera_file, G_FILE_TEST_EXISTS)) {
      GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
          ("Camera file does not exist: %s", src->camera_file), (NULL));
      return FALSE;
    }

    ret =
        BiBrdOpenCam (BiTypeAny, src->board_index, &src->board,
        src->camera_file);
    if (ret != BI_OK) {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
          ("Board could not be opened with camera file: %s", src->camera_file),
          (NULL));
      return FALSE;
    }
  } else {
    /* use default camera file set in SysReg */
    ret = BiBrdOpen (BiTypeAny, src->board_index, &src->board);
    if (ret != BI_OK) {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
          ("Board could not be opened with default camera file"), (NULL));
      return FALSE;
    }
  }

  {
    BFCHAR ModelSt[MAX_STRING];
    BFCHAR FamilySt[MAX_STRING];
    BFU32 FamilyIndex;
    BFU32 CiFamily;
    BFGetBoardStrings (src->board, ModelSt, MAX_STRING, FamilySt, MAX_STRING,
        &FamilyIndex, &CiFamily);

    GST_DEBUG_OBJECT (src, "Board \"%s(%d) - %s\" has been opened.\n", FamilySt,
        src->board_index, ModelSt);
  }

  ret =
      BiBufferAllocCam (src->board, &src->buffer_array,
      src->num_capture_buffers);
  if (ret != BI_OK) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to allocate buffers: %s", gst_bitflowsrc_get_error_string (src,
                ret)), (NULL));
    BiBrdClose (src->board);
    return FALSE;
  }

  /* CirErIgnore will cause "tearing" if buffers aren't handled fast enough */
  /* TODO: CirErLast and CirErNew might be available in some hardware,
     which would prevent tearing but would drop frames */
  ret =
      BiCircAqSetup (src->board, &src->buffer_array, CirErIgnore,
      BiAqEngJ | NoResetOnError);
  if (ret != BI_OK) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to setup circular acquisition: %s",
            gst_bitflowsrc_get_error_string (src, ret)), (NULL));
    BiBufferFree (src->board, &src->buffer_array);
    BiBrdClose (src->board);
    return FALSE;
  }

  /* BiCamInqFormat gets tap config, see BFFormatStandard */
  /* BiCamInqCamType gets line/area, see BFCamTypeAreaScan */
  /* BiCamInqBytesPerPix gets bytes per pixel */
  /* BiCamInqFrameSize0 gets total frame size */
  BiBrdInquire (src->board, BiCamInqXSize, &width);
  BiBrdInquire (src->board, BiCamInqYSize0, &height);
  BiBrdInquire (src->board, BiCamInqBitsPerPix, &bpp);
  BiBrdInquire (src->board, BiCamInqFrameWidth, &stride);

  /* create caps */
  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  gst_video_info_init (&vinfo);
  vinfo.width = width;
  vinfo.height = height;

  if (bpp <= 8) {
    vinfo.finfo = gst_video_format_get_info (GST_VIDEO_FORMAT_GRAY8);
    src->caps = gst_video_info_to_caps (&vinfo);
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
  } else {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        ("Unknown or unsupported bit depth (%d).", bpp), (NULL));
    return FALSE;
  }

  src->height = vinfo.height;
  src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);
  src->bf_stride = stride;

  GST_DEBUG_OBJECT (src, "starting acquisition");
  ret = BiCirControl (src->board, &src->buffer_array, BISTART, BiWait);
  if (ret != BI_OK) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to start grabbing: %s", gst_bitflowsrc_get_error_string (src,
                ret)), (NULL));
    BiCircCleanUp (src->board, &src->buffer_array);
    BiBufferFree (src->board, &src->buffer_array);
    BiBrdClose (src->board);
    return GST_FLOW_ERROR;
  }

  /* TODO: check timestamps on buffers vs start time */
  src->acq_start_time =
      gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src)));

  return TRUE;
}

static gboolean
gst_bitflowsrc_stop (GstBaseSrc * bsrc)
{
  GstBitflowSrc *src = GST_BITFLOW_SRC (bsrc);
  BFRC ret;
  GST_DEBUG_OBJECT (src, "stop");

  ret = BiCircCleanUp (src->board, &src->buffer_array);
  if (ret != BI_OK) {
    GST_WARNING_OBJECT (src, "Failed to cleanup circular acquisition: %s",
        gst_bitflowsrc_get_error_string (src, ret));
  }

  ret = BiBufferFree (src->board, &src->buffer_array);
  if (ret != BI_OK) {
    GST_WARNING_OBJECT (src, "Failed to free buffer array: %s",
        gst_bitflowsrc_get_error_string (src, ret));
  }

  ret = BiBrdClose (src->board);
  if (ret != BI_OK) {
    GST_WARNING_OBJECT (src, "Failed to free buffer array: %s",
        gst_bitflowsrc_get_error_string (src, ret));
  }

  gst_bitflowsrc_reset (src);

  return TRUE;
}

static GstCaps *
gst_bitflowsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstBitflowSrc *src = GST_BITFLOW_SRC (bsrc);
  GstCaps *caps;

  if (src->board == NULL) {
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
gst_bitflowsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstBitflowSrc *src = GST_BITFLOW_SRC (bsrc);
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
gst_bitflowsrc_unlock (GstBaseSrc * bsrc)
{
  GstBitflowSrc *src = GST_BITFLOW_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock");

  src->stop_requested = TRUE;

  return TRUE;
}

static gboolean
gst_bitflowsrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstBitflowSrc *src = GST_BITFLOW_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock_stop");

  src->stop_requested = FALSE;

  return TRUE;
}

static GstBuffer *
gst_bitflowsrc_create_buffer_from_circ_handle (GstBitflowSrc * src,
    BiCirHandle * circ_handle)
{
  GstMapInfo minfo;
  GstBuffer *buf;

  /* TODO: use allocator or use from pool */
  buf = gst_buffer_new_and_alloc (src->height * src->gst_stride);

  /* Copy image to buffer from surface */
  gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
  GST_LOG_OBJECT (src,
      "GstBuffer size=%d, gst_stride=%d, buffer_num=%d, frame_count=%d, num_frames_on_queue=%d",
      minfo.size, src->gst_stride, circ_handle->BufferNumber,
      circ_handle->FrameCount, circ_handle->NumItemsOnQueue);
  GST_LOG_OBJECT (src, "Buffer timestamp %02d:%02d:%02d.%06d",
      circ_handle->HiResTimeStamp.hour, circ_handle->HiResTimeStamp.min,
      circ_handle->HiResTimeStamp.sec, circ_handle->HiResTimeStamp.usec);

  /* TODO: use orc_memcpy */
  if (src->gst_stride == src->bf_stride) {
    memcpy (minfo.data, ((guint8 *) circ_handle->pBufData), minfo.size);
  } else {
    int i;
    GST_LOG_OBJECT (src, "Image strides not identical, copy will be slower.");
    for (i = 0; i < src->height; i++) {
      memcpy (minfo.data + i * src->gst_stride,
          ((guint8 *) circ_handle->pBufData) +
          i * src->bf_stride, src->bf_stride);
    }
  }
  gst_buffer_unmap (buf, &minfo);

  return buf;
}

static GstFlowReturn
gst_bitflowsrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstBitflowSrc *src = GST_BITFLOW_SRC (psrc);
  BFRC ret;
  BiCirHandle circ_handle;
  guint32 dropped_frames;
  GstClock *clock;
  GstClockTime clock_time;

  GST_LOG_OBJECT (src, "create");

  /* wait for next frame to be available */
  ret = BiCirWaitDoneFrame (src->board, &src->buffer_array,
      src->timeout, &circ_handle);
  if (ret != BI_OK) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to acquire frame: %s", gst_bitflowsrc_get_error_string (src,
                ret)), (NULL));
    return GST_FLOW_ERROR;
  }

  clock = gst_element_get_clock (GST_ELEMENT (src));
  clock_time = gst_clock_get_time (clock);
  gst_object_unref (clock);

  /* check for dropped frames and disrupted signal */
  dropped_frames = (circ_handle.FrameCount - src->last_frame_count) - 1;
  if (dropped_frames > 0) {
    src->total_dropped_frames += dropped_frames;
    GST_WARNING_OBJECT (src, "Dropped %d frames (%d total)", dropped_frames,
        src->total_dropped_frames);
  } else if (dropped_frames < 0) {
    GST_WARNING_OBJECT (src, "Frame count non-monotonic, signal disrupted?");
  }
  src->last_frame_count = circ_handle.FrameCount;

  /* create GstBuffer then release circ buffer back to acquisition */
  *buf = gst_bitflowsrc_create_buffer_from_circ_handle (src, &circ_handle);
  ret =
      BiCirStatusSet (src->board, &src->buffer_array, circ_handle, BIAVAILABLE);
  if (ret != BI_OK) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to release buffer: %s", gst_bitflowsrc_get_error_string (src,
                ret)), (NULL));
    return GST_FLOW_ERROR;
  }

  /* TODO: understand why timestamps for circ_handle are sometimes 0 */
  //GST_BUFFER_TIMESTAMP (*buf) =
  //    GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
  //    src->acq_start_time + circ_handle.HiResTimeStamp.totalSec * GST_SECOND);
  GST_BUFFER_TIMESTAMP (*buf) =
      GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
      clock_time);
  GST_BUFFER_OFFSET (*buf) = circ_handle.FrameCount - 1;

  if (src->stop_requested) {
    if (*buf != NULL) {
      gst_buffer_unref (*buf);
      *buf = NULL;
    }
    return GST_FLOW_FLUSHING;
  }

  return GST_FLOW_OK;
}

gchar *
gst_bitflowsrc_get_error_string (GstBitflowSrc * src, BIRC error_num)
{
  BFU32 error_string_size = MAX_STRING;
  BiErrorTextGet (src->board, error_num, src->error_string, &error_string_size);
  return src->error_string;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_bitflowsrc_debug, "bitflowsrc", 0,
      "debug category for bitflowsrc element");
  gst_element_register (plugin, "bitflowsrc", GST_RANK_NONE,
      gst_bitflowsrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    bitflow,
    "BitFlow frame grabber source",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
