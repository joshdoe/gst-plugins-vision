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

#include "gstmatroxsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_matroxsrc_debug);
#define GST_CAT_DEFAULT gst_matroxsrc_debug

/* prototypes */
static void gst_matroxsrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_matroxsrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_matroxsrc_dispose (GObject * object);
static void gst_matroxsrc_finalize (GObject * object);

static gboolean gst_matroxsrc_start (GstBaseSrc * src);
static gboolean gst_matroxsrc_stop (GstBaseSrc * src);
static GstCaps *gst_matroxsrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_matroxsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_matroxsrc_unlock (GstBaseSrc * src);
static gboolean gst_matroxsrc_unlock_stop (GstBaseSrc * src);

static GstFlowReturn gst_matroxsrc_create (GstPushSrc * src, GstBuffer ** buf);

static GstCaps *gst_matroxsrc_create_caps (GstMatroxSrc * src);
static MIL_INT MFTYPE
gst_matroxsrc_callback (MIL_INT HookType, MIL_ID EventId, void *UserDataPtr);

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_BOARD,
  PROP_CHANNEL,
  PROP_FORMAT,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_TIMEOUT
};

#define DEFAULT_PROP_DEVICE "M_SYSTEM_DEFAULT"
#define DEFAULT_PROP_BOARD -1
#define DEFAULT_PROP_CHANNEL -1
#define DEFAULT_PROP_FORMAT "M_DEFAULT"
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 2
#define DEFAULT_PROP_TIMEOUT 1000

/* pad templates */

static GstStaticPadTemplate gst_matroxsrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, GRAY16_BE, BGRA }"))
    );

/* class initialization */

G_DEFINE_TYPE (GstMatroxSrc, gst_matroxsrc, GST_TYPE_PUSH_SRC);

static void
gst_matroxsrc_class_init (GstMatroxSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_matroxsrc_set_property;
  gobject_class->get_property = gst_matroxsrc_get_property;
  gobject_class->dispose = gst_matroxsrc_dispose;
  gobject_class->finalize = gst_matroxsrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_matroxsrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Matrox Imaging Library Video Source", "Source/Video",
      "Matrox Imaging Library video source",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_matroxsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_matroxsrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_matroxsrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_matroxsrc_set_caps);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_matroxsrc_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_matroxsrc_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_matroxsrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "System descriptor, default is specified in MilConfig",
          DEFAULT_PROP_DEVICE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BOARD,
      g_param_spec_int ("board", "Board",
          "Board number, -1 uses default specified in MilConfig", -1, 15,
          DEFAULT_PROP_BOARD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_int ("channel", "Channel",
          "Channel number, -1 uses default specified in MilConfig", -1, 15,
          DEFAULT_PROP_CHANNEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FORMAT,
      g_param_spec_string ("format", "Format or format file",
          "Format, as predefined string or DCF file path, default is specified in MilConfig",
          DEFAULT_PROP_FORMAT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_uint ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 1, G_MAXUINT,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TIMEOUT,
      g_param_spec_int ("timeout", "Timeout (ms)",
          "Timeout in ms (0 to use default)", 0, G_MAXINT, DEFAULT_PROP_TIMEOUT,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
}

static void
gst_matroxsrc_reset (GstMatroxSrc * src)
{
  gint i;
  src->acq_started = FALSE;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }
  if (src->buffer) {
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }

  if (src->MilGrabBufferList) {
    for (i = 0; src->num_capture_buffers; ++i) {
      if (src->MilGrabBufferList[i]) {
        MbufFree (src->MilGrabBufferList[i]);
      }
    }
    g_free (src->MilGrabBufferList);
    src->MilGrabBufferList = NULL;
  }

  if (src->MilDigitizer) {
    MdigFree (src->MilDigitizer);
    src->MilDigitizer = M_NULL;
  }

  if (src->MilSystem) {
    MsysFree (src->MilSystem);
    src->MilSystem = M_NULL;
  }

  if (src->MilApplication) {
    MappFree (src->MilApplication);
    src->MilApplication = M_NULL;
  }
}

static void
gst_matroxsrc_init (GstMatroxSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->device = g_strdup (DEFAULT_PROP_DEVICE);
  src->board = DEFAULT_PROP_BOARD;
  src->channel = DEFAULT_PROP_CHANNEL;
  src->format = g_strdup (DEFAULT_PROP_FORMAT);
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;
  src->timeout = DEFAULT_PROP_TIMEOUT;

  g_mutex_init (&src->mutex);
  g_cond_init (&src->cond);
  src->stop_requested = FALSE;
  src->caps = NULL;
  src->buffer = NULL;

  src->MilApplication = M_NULL;
  src->MilSystem = M_NULL;
  src->MilDigitizer = M_NULL;
  src->MilGrabBufferList = NULL;

  gst_matroxsrc_reset (src);
}

void
gst_matroxsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMatroxSrc *src;

  src = GST_MATROX_SRC (object);

  switch (property_id) {
    case PROP_DEVICE:
      g_free (src->device);
      src->device = g_strdup (g_value_get_string (value));
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
      src->board = g_value_get_int (value);
      break;
    case PROP_CHANNEL:
      src->channel = g_value_get_int (value);
      break;
    case PROP_FORMAT:
      g_free (src->format);
      src->format = g_strdup (g_value_get_string (value));
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
gst_matroxsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstMatroxSrc *src;

  g_return_if_fail (GST_IS_MATROX_SRC (object));
  src = GST_MATROX_SRC (object);

  switch (property_id) {
    case PROP_DEVICE:
      g_value_set_string (value, src->device);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      g_value_set_uint (value, src->num_capture_buffers);
      break;
    case PROP_BOARD:
      g_value_set_int (value, src->board);
      break;
    case PROP_CHANNEL:
      g_value_set_int (value, src->channel);
      break;
    case PROP_FORMAT:
      g_value_set_string (value, src->format);
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
gst_matroxsrc_dispose (GObject * object)
{
  GstMatroxSrc *src;

  g_return_if_fail (GST_IS_MATROX_SRC (object));
  src = GST_MATROX_SRC (object);

  /* clean up as possible.  may be called multiple times */

  g_mutex_clear (&src->mutex);
  g_cond_clear (&src->cond);

  G_OBJECT_CLASS (gst_matroxsrc_parent_class)->dispose (object);
}

void
gst_matroxsrc_finalize (GObject * object)
{
  GstMatroxSrc *src;

  g_return_if_fail (GST_IS_MATROX_SRC (object));
  src = GST_MATROX_SRC (object);

  /* clean up object here */
  g_free (src->device);
  g_free (src->format);

  gst_matroxsrc_reset (src);

  G_OBJECT_CLASS (gst_matroxsrc_parent_class)->finalize (object);
}

static gboolean
gst_matroxsrc_start (GstBaseSrc * bsrc)
{
  GstMatroxSrc *src = GST_MATROX_SRC (bsrc);
  MIL_ID ret;
  gint i;
  gint width;
  gint height;
  gint bpp;
  gint n_bands;
  GstVideoInfo vinfo;

  GST_DEBUG_OBJECT (src, "start");

  /* create App */
  ret = MappAlloc (M_NULL, M_DEFAULT, &src->MilApplication);
  if (ret == M_NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to allocate a MIL application"), (NULL));
    return FALSE;
  }

  /* create System */
  if (src->board == -1) {
    ret =
        MsysAlloc (src->MilApplication, src->device, M_DEFAULT, M_DEFAULT,
        &src->MilSystem);
  } else {
    ret =
        MsysAlloc (src->MilApplication, src->device, src->board, M_DEFAULT,
        &src->MilSystem);
  }
  if (ret == M_NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to allocate a MIL system"), (NULL));
    return FALSE;
  }

  /* create Digitizer */
  ret = MdigAlloc (src->MilSystem, M_DEFAULT, src->format, M_DEFAULT,
      &src->MilDigitizer);
  if (ret == M_NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to allocate a MIL digitizer"), (NULL));
    return FALSE;
  }

  /* get format info and create caps */
  width = MdigInquire (src->MilDigitizer, M_SIZE_X, M_NULL);
  height = MdigInquire (src->MilDigitizer, M_SIZE_Y, M_NULL);
  bpp = MdigInquire (src->MilDigitizer, M_SIZE_BIT, M_NULL);
  n_bands = MdigInquire (src->MilDigitizer, M_SIZE_BAND, M_NULL);
  src->color_mode = MdigInquire (src->MilDigitizer, M_COLOR_MODE, M_NULL);

  gst_video_info_init (&vinfo);

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  if (src->color_mode == M_MONOCHROME) {
    g_assert (n_bands == 1);
    if (bpp == 8) {
      src->video_format = GST_VIDEO_FORMAT_GRAY8;
    } else if (bpp > 8 && bpp <= 16) {
      GValue val = G_VALUE_INIT;
      GstStructure *s;

      if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        src->video_format = GST_VIDEO_FORMAT_GRAY16_LE;
      } else if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        src->video_format = GST_VIDEO_FORMAT_GRAY16_BE;
      }

      gst_video_info_set_format (&vinfo, src->video_format, width, height);
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
  } else if (src->color_mode == M_BGR24) {
    g_assert (n_bands == 3);
    src->video_format = GST_VIDEO_FORMAT_BGR;
  } else if (src->color_mode == M_BGR32) {
    g_assert (n_bands == 3);
    src->video_format = GST_VIDEO_FORMAT_BGRx;
  } else if (src->color_mode == M_RGB) {
    g_assert (n_bands == 3);
    src->video_format = GST_VIDEO_FORMAT_RGB;
  }                             /*else if (color_mode == M_YUV) {
                                   g_assert (n_bands == 3);
                                   src = GST_VIDEO_FORMAT_YUY2;
                                   } */
  else {
    GST_WARNING_OBJECT (src,
        "Color mode %d not directly supported, will try converting to BGRx",
        src->color_mode);
    src->video_format = GST_VIDEO_FORMAT_BGRx;
    src->color_mode = M_BGR32;
    n_bands = 3;
    bpp = 8;
  }

  if (!src->caps) {
    gst_video_info_set_format (&vinfo, src->video_format, width, height);

    vinfo.finfo = gst_video_format_get_info (src->video_format);
    src->caps = gst_video_info_to_caps (&vinfo);
  }
  src->height = vinfo.height;
  src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);

  g_assert (src->MilGrabBufferList == NULL);
  src->MilGrabBufferList = g_new (MIL_ID, src->num_capture_buffers);
  for (i = 0; i < src->num_capture_buffers; i++) {
    if (src->color_mode == M_MONOCHROME) {
      MbufAlloc2d (src->MilSystem, width, height, bpp,
          M_IMAGE + M_GRAB + M_PROC, &src->MilGrabBufferList[i]);
    } else {
      MbufAllocColor (src->MilSystem,
          n_bands,
          width,
          height,
          bpp, M_IMAGE + M_GRAB + M_PROC + M_PACKED,
          &src->MilGrabBufferList[i]);
    }

    if (src->MilGrabBufferList[i]) {
      MbufClear (src->MilGrabBufferList[i], 0xFF);
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to allocate a MIL buffer"), (NULL));
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gst_matroxsrc_stop (GstBaseSrc * bsrc)
{
  GstMatroxSrc *src = GST_MATROX_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "stop");

  if (src->acq_started) {
    MdigProcess (src->MilDigitizer, src->MilGrabBufferList,
        src->num_capture_buffers, M_STOP, M_DEFAULT, gst_matroxsrc_callback,
        src);
    src->acq_started = FALSE;
  }

  gst_matroxsrc_reset (src);

  return TRUE;
}

static GstCaps *
gst_matroxsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstMatroxSrc *src = GST_MATROX_SRC (bsrc);
  GstCaps *caps;

  if (src->MilDigitizer == M_NULL) {
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
gst_matroxsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstMatroxSrc *src = GST_MATROX_SRC (bsrc);
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
gst_matroxsrc_unlock (GstBaseSrc * bsrc)
{
  GstMatroxSrc *src = GST_MATROX_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock");

  g_mutex_lock (&src->mutex);
  src->stop_requested = TRUE;
  g_cond_signal (&src->cond);
  g_mutex_unlock (&src->mutex);

  return TRUE;
}

static gboolean
gst_matroxsrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstMatroxSrc *src = GST_MATROX_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock_stop");

  src->stop_requested = FALSE;

  return TRUE;
}

static GstBuffer *
gst_matroxsrc_create_buffer_from_id (GstMatroxSrc * src, MIL_ID buffer_id)
{
  GstMapInfo minfo;
  GstBuffer *buf;

  /* TODO: use allocator or use from pool */
  buf = gst_buffer_new_and_alloc (src->height * src->gst_stride);

  /* map buffer so we can copy to it */
  gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
  GST_LOG_OBJECT (src,
      "GstBuffer size=%d, gst_stride=%d", minfo.size, src->gst_stride);

  /* copy MilBuffer to GstBuffer, possibly performing conversion in the process */
  if (src->video_format == GST_VIDEO_FORMAT_GRAY8 ||
      src->video_format == GST_VIDEO_FORMAT_GRAY16_LE ||
      src->video_format == GST_VIDEO_FORMAT_GRAY16_BE) {
    MbufGet (buffer_id, minfo.data);
  } else {
    /* TODO: add support for planar color and YUV */
    MbufGetColor (buffer_id, M_PACKED | src->color_mode, M_ALL_BANDS,
        minfo.data);
  }

  gst_buffer_unmap (buf, &minfo);

  return buf;
}


static MIL_INT MFTYPE
gst_matroxsrc_callback (MIL_INT HookType, MIL_ID EventId, void *UserDataPtr)
{
  GstMatroxSrc *src = GST_MATROX_SRC (UserDataPtr);
  MIL_ID ModifiedBufferId;
  gint dropped_frames;
  static guint64 last_frame_number = 0;
  static guint64 buffers_processed = 0;
  static guint64 total_dropped_frames = 0;
  GstClock *clock;
  GstClockTime clock_time;

  g_assert (src != NULL);

  clock = gst_element_get_clock (GST_ELEMENT (src));
  clock_time = gst_clock_get_time (clock);
  gst_object_unref (clock);

  /* Retrieve the MIL_ID of the grabbed buffer. */
  MdigGetHookInfo (EventId, M_MODIFIED_BUFFER + M_BUFFER_ID, &ModifiedBufferId);

  ///* check for dropped frames and disrupted signal */
  //dropped_frames = (pFrameInfo->number - last_frame_number) - 1;
  //if (dropped_frames > 0) {
  //  total_dropped_frames += dropped_frames;
  //  GST_WARNING_OBJECT (src, "Dropped %d frames (%d total)", dropped_frames,
  //      total_dropped_frames);
  //} else if (dropped_frames < 0) {
  //  GST_WARNING_OBJECT (src,
  //      "Signal disrupted, frames likely dropped and timestamps inaccurate");

  //  /* frame timestamps reset, so adjust start time, accuracy reduced */
  //  src->acq_start_time =
  //      gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src))) -
  //      pFrameInfo->timestamp * GST_USECOND;
  //}
  //last_frame_number = pFrameInfo->number;

  g_mutex_lock (&src->mutex);

  if (src->buffer) {
    /* TODO: save this in dropped frame total? */
    GST_WARNING_OBJECT (src,
        "Got new buffer before old handled, dropping old.");
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }

  src->buffer = gst_matroxsrc_create_buffer_from_id (src, ModifiedBufferId);

  GST_BUFFER_TIMESTAMP (src->buffer) =
      GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
      clock_time);
  GST_BUFFER_OFFSET (src->buffer) = buffers_processed;
  ++buffers_processed;

  g_cond_signal (&src->cond);
  g_mutex_unlock (&src->mutex);

  return M_NULL;
}

static GstFlowReturn
gst_matroxsrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstMatroxSrc *src = GST_MATROX_SRC (psrc);
  gint64 end_time;

  GST_LOG_OBJECT (src, "create");

  /* Start acquisition if not already started */
  if (G_UNLIKELY (!src->acq_started)) {
    GST_LOG_OBJECT (src, "starting acquisition");
    src->acq_start_time =
        gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src)));

    MdigProcess (src->MilDigitizer, src->MilGrabBufferList,
        src->num_capture_buffers, M_START, M_DEFAULT, gst_matroxsrc_callback,
        src);

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
  GST_DEBUG_CATEGORY_INIT (gst_matroxsrc_debug, "matroxsrc", 0,
      "debug category for matroxsrc element");
  gst_element_register (plugin, "matroxsrc", GST_RANK_NONE,
      gst_matroxsrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    matrox,
    "Matrox Imaging Library video source",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
