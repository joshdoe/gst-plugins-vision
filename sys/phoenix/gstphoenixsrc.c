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
 * SECTION:element-gstphoenixsrc
 *
 * The phoenixsrc element is a source for framegrabbers supported by the Active Silicon Phoenix driver.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v phoenixsrc ! ffmpegcolorspace ! autovideosink
 * ]|
 * Shows video from the default Phoenix framegrabber
 * </refsect2>
 */

/* TODO:
    * allow for use of onboard LUT (rare, since we usually want raw data)
    * allow for colorspace conversions (again rare)
    * test sending of API-provided buffers, using gst_memory_new_wrapped
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#include "gstphoenixsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_phoenixsrc_debug);
#define GST_CAT_DEFAULT gst_phoenixsrc_debug

/* prototypes */
static void gst_phoenixsrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_phoenixsrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_phoenixsrc_dispose (GObject * object);
static void gst_phoenixsrc_finalize (GObject * object);

static gboolean gst_phoenixsrc_start (GstBaseSrc * src);
static gboolean gst_phoenixsrc_stop (GstBaseSrc * src);
static GstCaps *gst_phoenixsrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_phoenixsrc_set_caps (GstBaseSrc * src, GstCaps * caps);

static GstFlowReturn gst_phoenixsrc_create (GstPushSrc * src, GstBuffer ** buf);

static GstCaps *gst_phoenixsrc_create_caps (GstPhoenixSrc * src);
enum
{
  PROP_0,
  PROP_CAMERA_CONFIG_FILEPATH,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_BOARD,
  PROP_CHANNEL
};

#define DEFAULT_PROP_CAMERA_CONFIG_FILEPATH NULL        /* defaults to 640x480x8bpp */
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 2
#define DEFAULT_PROP_BOARD 0
#define DEFAULT_PROP_CHANNEL 0

/* pad templates */

static GstStaticPadTemplate gst_phoenixsrc_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, GRAY16_BE, RGB, xRGB, RGB_15, RGB_16 }") ";"
        "video/x-bayer,format=(string){bggr,grbg,gbrg,rggb},"
        "width=(int)[1,MAX],height=(int)[1,MAX],framerate=(fraction)[0/1,MAX];"
        "video/x-bayer,format=(string){bggr16,grbg16,gbrg16,rggb16},"
        "bpp=(int){10,12,14,16},endianness={1234,4321},"
        "width=(int)[1,MAX],height=(int)[1,MAX],framerate=(fraction)[0/1,MAX]")
    );

/* class initialization */

G_DEFINE_TYPE (GstPhoenixSrc, gst_phoenixsrc, GST_TYPE_PUSH_SRC);

static void
gst_phoenixsrc_class_init (GstPhoenixSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_phoenixsrc_set_property;
  gobject_class->get_property = gst_phoenixsrc_get_property;
  gobject_class->dispose = gst_phoenixsrc_dispose;
  gobject_class->finalize = gst_phoenixsrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_phoenixsrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Active Silicon Phoenix Video Source", "Source/Video",
      "Active Silicon Phoenix framegrabber video source",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_phoenixsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_phoenixsrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_phoenixsrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_phoenixsrc_set_caps);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_phoenixsrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_CAMERA_CONFIG_FILEPATH,
      g_param_spec_string ("config-file", "Config file",
          "Camera configuration filepath", DEFAULT_PROP_CAMERA_CONFIG_FILEPATH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_uint ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 1, G_MAXUINT,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BOARD,
      g_param_spec_uint ("board", "Board",
          "Board number (0 for auto)", 0, 7,
          DEFAULT_PROP_BOARD,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_uint ("channel", "Channel",
          "Channel number (0 for auto)", 0, 2,
          DEFAULT_PROP_CHANNEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_phoenixsrc_init (GstPhoenixSrc * phoenixsrc)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (phoenixsrc), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (phoenixsrc), GST_FORMAT_TIME);

  /* initialize member variables */
  phoenixsrc->config_filepath = g_strdup (DEFAULT_PROP_CAMERA_CONFIG_FILEPATH);
  phoenixsrc->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;

  phoenixsrc->first_phoenix_ts = GST_CLOCK_TIME_NONE;
  phoenixsrc->frame_start_times =
      g_new (guint64, phoenixsrc->num_capture_buffers);
  phoenixsrc->frame_end_times =
      g_new (guint64, phoenixsrc->num_capture_buffers);
  phoenixsrc->buffer_ready = FALSE;
  phoenixsrc->timeout_occurred = FALSE;
  phoenixsrc->fifo_overflow_occurred = FALSE;

  phoenixsrc->buffer_ready_count = 0;
  phoenixsrc->buffer_processed_count = 0;
  phoenixsrc->frame_end_count = 0;
  phoenixsrc->frame_start_count = 0;
  /*phoenixsrc->frame_count = 0; */

  phoenixsrc->hCamera = 0;

  g_mutex_init (&phoenixsrc->mutex);
  g_cond_init (&phoenixsrc->cond);
}

void
gst_phoenixsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPhoenixSrc *phoenixsrc;

  g_return_if_fail (GST_IS_PHOENIX_SRC (object));
  phoenixsrc = GST_PHOENIX_SRC (object);

  switch (property_id) {
    case PROP_CAMERA_CONFIG_FILEPATH:
      g_free (phoenixsrc->config_filepath);
      phoenixsrc->config_filepath = g_strdup (g_value_get_string (value));
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      if (phoenixsrc->acq_started) {
        GST_ELEMENT_WARNING (phoenixsrc, RESOURCE, SETTINGS,
            ("Number of capture buffers cannot be changed after acquisition has started."),
            (NULL));
      } else {
        phoenixsrc->num_capture_buffers = g_value_get_uint (value);

        g_free (phoenixsrc->frame_start_times);
        phoenixsrc->frame_start_times =
            g_new (guint64, phoenixsrc->num_capture_buffers);

        g_free (phoenixsrc->frame_end_times);
        phoenixsrc->frame_end_times =
            g_new (guint64, phoenixsrc->num_capture_buffers);
      }
      break;
    case PROP_BOARD:
      phoenixsrc->board = g_value_get_uint (value);
      break;
    case PROP_CHANNEL:
      phoenixsrc->channel = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_phoenixsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstPhoenixSrc *phoenixsrc;

  g_return_if_fail (GST_IS_PHOENIX_SRC (object));
  phoenixsrc = GST_PHOENIX_SRC (object);

  switch (property_id) {
    case PROP_CAMERA_CONFIG_FILEPATH:
      g_value_set_string (value, phoenixsrc->config_filepath);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      g_value_set_uint (value, phoenixsrc->num_capture_buffers);
      break;
    case PROP_BOARD:
      g_value_set_uint (value, phoenixsrc->board);
      break;
    case PROP_CHANNEL:
      g_value_set_uint (value, phoenixsrc->channel);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_phoenixsrc_dispose (GObject * object)
{
  GstPhoenixSrc *phoenixsrc;

  g_return_if_fail (GST_IS_PHOENIX_SRC (object));
  phoenixsrc = GST_PHOENIX_SRC (object);

  /* clean up as possible.  may be called multiple times */

  g_free (phoenixsrc->config_filepath);

  G_OBJECT_CLASS (gst_phoenixsrc_parent_class)->dispose (object);
}

void
gst_phoenixsrc_finalize (GObject * object)
{
  GstPhoenixSrc *phoenixsrc;

  g_return_if_fail (GST_IS_PHOENIX_SRC (object));
  phoenixsrc = GST_PHOENIX_SRC (object);

  /* clean up object here */
  g_free (phoenixsrc->frame_start_times);
  g_free (phoenixsrc->frame_end_times);

  G_OBJECT_CLASS (gst_phoenixsrc_parent_class)->finalize (object);
}

static inline GstClockTime
gst_phoenix_get_timestamp (GstPhoenixSrc * phoenixsrc)
{
  ui32 dwParam;
  guint64 timestamp;

  /* get time in microseconds from start of acquisition */
  /* TODO: check for rollover */
  PHX_ParameterGet (phoenixsrc->hCamera, PHX_EVENTCOUNT, &dwParam);
  timestamp = (guint64) 1000 *dwParam;

  if (phoenixsrc->first_phoenix_ts == GST_CLOCK_TIME_NONE) {
    phoenixsrc->first_phoenix_ts = timestamp;
  }
  return timestamp - phoenixsrc->first_phoenix_ts;
}

/* Callback function to handle image capture events. */
void
phx_callback (tHandle hCamera, ui32 dwMask, void *pvParams)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (pvParams);
  GstClockTime ct = gst_phoenix_get_timestamp (phoenixsrc);
  gboolean signal_create_func = FALSE;
  guint n;

  g_mutex_lock (&phoenixsrc->mutex);

  /* Note that more than one interrupt can be sent, so no "else if" */

  /* called when frame valid signal goes high */
  if (PHX_INTRPT_FRAME_START & dwMask) {
    /* FIXME: this will work until frames are dropped */
    n = phoenixsrc->frame_start_count % phoenixsrc->num_capture_buffers;
    phoenixsrc->frame_start_times[n] = ct;

    phoenixsrc->frame_start_count++;
  }

  /* called when frame valid signal goes low */
  if (PHX_INTRPT_FRAME_END & dwMask) {
    /* FIXME: this will work until frames are dropped */
    n = (phoenixsrc->frame_end_count - 1) % phoenixsrc->num_capture_buffers;
    phoenixsrc->frame_end_times[n] = ct;

    phoenixsrc->frame_end_count++;
  }

  if (PHX_INTRPT_BUFFER_READY & dwMask) {
    /* we have a buffer */
    phoenixsrc->buffer_ready = TRUE;
    phoenixsrc->buffer_ready_count++;
    signal_create_func = TRUE;
  }

  if (PHX_INTRPT_TIMEOUT & dwMask) {
    /* TODO: we could offer to try and ABORT then re-START capture */
    phoenixsrc->timeout_occurred = TRUE;
    signal_create_func = TRUE;
  }

  if (PHX_INTRPT_FIFO_OVERFLOW & dwMask) {
    phoenixsrc->fifo_overflow_occurred = TRUE;
    signal_create_func = TRUE;
  }



  if (signal_create_func)
    g_cond_signal (&phoenixsrc->cond);
  g_mutex_unlock (&phoenixsrc->mutex);
  /* after unlocking, _create will check for these errors and copy data */
}

static gboolean
gst_phoenixsrc_start (GstBaseSrc * src)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);
  etStat eStat = PHX_OK;        /* Status variable */
  etParamValue eParamValue = PHX_INVALID_PARAMVALUE;
  ui32 dwParamValue = 0;
  guint eCamConfig;

  GST_DEBUG_OBJECT (phoenixsrc, "start");

  if (phoenixsrc->config_filepath == NULL) {
    GST_WARNING_OBJECT (phoenixsrc,
        "No config file set, using default 640x480x8bpp");
  } else if (!g_file_test (phoenixsrc->config_filepath, G_FILE_TEST_EXISTS)) {
    GST_ELEMENT_ERROR (phoenixsrc, RESOURCE, NOT_FOUND,
        ("Camera config file does not exist: %s", phoenixsrc->config_filepath),
        (NULL));
    goto Error;
  }

  /* TODO: hacky, use enums or something */
  eCamConfig = PHX_DIGITAL;
  switch (phoenixsrc->board) {
    case 0:
      eCamConfig |= PHX_BOARD_AUTO;
      break;
    case 1:
      eCamConfig |= PHX_BOARD1;
      break;
    case 2:
      eCamConfig |= PHX_BOARD2;
      break;
    case 3:
      eCamConfig |= PHX_BOARD3;
      break;
    case 4:
      eCamConfig |= PHX_BOARD4;
      break;
    case 5:
      eCamConfig |= PHX_BOARD5;
      break;
    case 6:
      eCamConfig |= PHX_BOARD6;
      break;
    case 7:
      eCamConfig |= PHX_BOARD7;
      break;
    default:
      g_assert_not_reached ();
  }

  switch (phoenixsrc->channel) {
    case 0:
      eCamConfig |= PHX_CHANNEL_AUTO;
      break;
    case 1:
      eCamConfig |= PHX_CHANNEL1;
      break;
    case 2:
      eCamConfig |= PHX_CHANNEL2;
      break;
  }

  /* Initialize board */
  eStat =
      PHX_CameraConfigLoad (&phoenixsrc->hCamera, phoenixsrc->config_filepath,
      (etCamConfigLoad) eCamConfig, PHX_ErrHandlerDefault);
  if (eStat != PHX_OK) {
    GST_ELEMENT_ERROR (phoenixsrc, LIBRARY, INIT, (NULL), (NULL));
    goto Error;
  }

  /* capture frames continuously */
  eParamValue = PHX_ENABLE;
  eStat =
      PHX_ParameterSet (phoenixsrc->hCamera, PHX_ACQ_CONTINUOUS, &eParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;

  /* capture in blocking fashion, i.e. don't overwrite un-processed buffers */
  eParamValue = PHX_DISABLE;
  eStat =
      PHX_ParameterSet (phoenixsrc->hCamera, PHX_ACQ_BLOCKING, &eParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;

  /* use event counter to count time from start of acquisition */
  eParamValue = PHX_EVENTCOUNT_TIME;
  eStat =
      PHX_ParameterSet (phoenixsrc->hCamera, PHX_EVENTCOUNT_SRC, &eParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  eParamValue = PHX_EVENTGATE_ACQ;
  eStat =
      PHX_ParameterSet (phoenixsrc->hCamera, PHX_EVENTGATE_SRC, &eParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;

  /* Tell Phoenix to use N buffers.  */
  eParamValue = phoenixsrc->num_capture_buffers;
  PHX_ParameterSet (phoenixsrc->hCamera, PHX_ACQ_NUM_IMAGES, &eParamValue);

  /* Setup a one second timeout value (milliseconds) */
  dwParamValue = 1000;
  eStat =
      PHX_ParameterSet (phoenixsrc->hCamera, PHX_TIMEOUT_DMA,
      (void *) &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;

  /* The BUFFER_READY interrupt is already enabled by default,
   * but we must enable other interrupts here. */
  eParamValue =
      PHX_INTRPT_TIMEOUT | PHX_INTRPT_FIFO_OVERFLOW | PHX_INTRPT_FRAME_END |
      PHX_INTRPT_FRAME_START;
  eStat =
      PHX_ParameterSet (phoenixsrc->hCamera, PHX_INTRPT_SET,
      (void *) &eParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;

  return TRUE;

ResourceSettingsError:
  GST_ELEMENT_ERROR (phoenixsrc, RESOURCE, SETTINGS,
      (("Failed to get Phoenix parameters.")), (NULL));

Error:
  /* Now cease all captures */
  if (phoenixsrc->hCamera)
    PHX_Acquire (phoenixsrc->hCamera, PHX_ABORT, NULL);

  /* TODO Free all the user allocated memory */
  //psImageBuff = pasImageBuffs;
  //if ( NULL != pasImageBuffs ) {
  //  while ( NULL != psImageBuff->pvAddress ) {
  //    free( psImageBuff->pvAddress );
  //    psImageBuff++;
  //  }
  //  free( pasImageBuffs );
  //}

  /* Release the Phoenix board */
  if (phoenixsrc->hCamera)
    PHX_CameraRelease (&phoenixsrc->hCamera);

  return FALSE;
}

static gboolean
gst_phoenixsrc_stop (GstBaseSrc * src)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);

  GST_DEBUG_OBJECT (phoenixsrc, "stop");

  if (phoenixsrc->hCamera) {
    /* Stop the acquisition */
    /* TODO: should we use PHX_STOP (finished current image) instead? */
    PHX_Acquire (phoenixsrc->hCamera, PHX_ABORT, NULL);

    /* Deallocates hardware and software resources, setting handle to null */
    PHX_CameraRelease (&phoenixsrc->hCamera);
  }

  phoenixsrc->dropped_frame_count = 0;
  /*phoenixsrc->last_time_code = -1; */

  return TRUE;
}

static GstCaps *
gst_phoenixsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstPhoenixSrc *src = GST_PHOENIX_SRC (bsrc);
  etStat eStat = PHX_OK;        /* Status variable */
  etParamValue eParamValue = PHX_INVALID_PARAMVALUE;
  ui32 dwParamValue = 0;
  guint32 phx_format;
  gint width, height;
  gint bpp, depth, endianness;
  GstVideoFormat videoFormat;
  gboolean is_gray16 = FALSE, is_bayer = FALSE;
  GstVideoInfo vinfo;
  GstCaps *caps;

  if (!src->hCamera) {
    return gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  }

  /* Create video info */
  gst_video_info_init (&vinfo);

  /* Get format (mono, Bayer, RBG, etc.) */
  eStat = PHX_ParameterGet (src->hCamera, PHX_DST_FORMAT, &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  phx_format = dwParamValue;

  /* Get endianness */
  eStat = PHX_ParameterGet (src->hCamera, PHX_DST_ENDIAN, &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  endianness =
      (dwParamValue == PHX_DST_LITTLE_ENDIAN) ? G_LITTLE_ENDIAN : G_BIG_ENDIAN;

  /* get width */
  eStat =
      PHX_ParameterGet (src->hCamera, PHX_ROI_XLENGTH_SCALED, &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  width = dwParamValue;

  /* get height */
  eStat =
      PHX_ParameterGet (src->hCamera, PHX_ROI_YLENGTH_SCALED, &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  height = dwParamValue;

  switch (phx_format) {
    case PHX_DST_FORMAT_Y8:
      videoFormat = GST_VIDEO_FORMAT_GRAY8;
      break;
    case PHX_DST_FORMAT_Y10:
      bpp = 10;
      is_gray16 = TRUE;
      break;
    case PHX_DST_FORMAT_Y12:
      bpp = 12;
      is_gray16 = TRUE;
      break;
    case PHX_DST_FORMAT_Y14:
      bpp = 14;
      is_gray16 = TRUE;
      break;
    case PHX_DST_FORMAT_Y16:
      bpp = 16;
      is_gray16 = TRUE;
      break;
    case PHX_DST_FORMAT_BAY8:
      bpp = 8;
      depth = 8;
      is_bayer = TRUE;
      break;
    case PHX_DST_FORMAT_BAY10:
      bpp = 10;
      depth = 16;
      is_bayer = TRUE;
      break;
    case PHX_DST_FORMAT_BAY12:
      bpp = 12;
      depth = 16;
      is_bayer = TRUE;
      break;
    case PHX_DST_FORMAT_BAY14:
      bpp = 14;
      depth = 16;
      is_bayer = TRUE;
      break;
    case PHX_DST_FORMAT_BAY16:
      bpp = 16;
      depth = 16;
      is_bayer = TRUE;
      break;
    case PHX_DST_FORMAT_RGB15:
      videoFormat = GST_VIDEO_FORMAT_RGB15;
      break;
    case PHX_DST_FORMAT_RGB16:
      videoFormat = GST_VIDEO_FORMAT_RGB16;
      break;
    case PHX_DST_FORMAT_RGB24:
      videoFormat = GST_VIDEO_FORMAT_RGB;
      break;
    case PHX_DST_FORMAT_RGB32: /* FIXME: what is the format of this? */
    case PHX_DST_FORMAT_XRGB32:
      videoFormat = GST_VIDEO_FORMAT_xRGB;
      break;
    default:
      videoFormat = GST_VIDEO_FORMAT_UNKNOWN;
  }

  if (is_gray16)
    videoFormat = (endianness == G_LITTLE_ENDIAN) ?
        GST_VIDEO_FORMAT_GRAY16_LE : GST_VIDEO_FORMAT_GRAY16_BE;

  if (is_bayer) {
    const gchar *bay_fmt;
    eStat = PHX_ParameterGet (src->hCamera, PHX_CAM_SRC_COL, &dwParamValue);
    if (PHX_OK != eStat)
      goto ResourceSettingsError;
    switch (dwParamValue) {
      case PHX_CAM_SRC_BAY_RGGB:
        bay_fmt = (depth == 16) ? "rggb16" : "rggb";
        break;
      case PHX_CAM_SRC_BAY_GRBG:
        bay_fmt = (depth == 16) ? "grbg16" : "grbg";
        break;
      case PHX_CAM_SRC_BAY_GBRG:
        bay_fmt = (depth == 16) ? "gbrg16" : "gbrg";
        break;
      case PHX_CAM_SRC_BAY_BGGR:
        bay_fmt = (depth == 16) ? "bggr16" : "bggr";
        break;
      default:
        GST_ERROR_OBJECT (src, "Unknown PHX_CAM_SRC_COL=%d", dwParamValue);
        goto Error;
    }

    if (depth == 8) {
      caps = gst_caps_new_simple ("video/x-bayer",
          "format", G_TYPE_STRING, bay_fmt,
          "width", G_TYPE_INT, width,
          "height", G_TYPE_INT, height,
          "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
    } else if (depth == 16) {
      caps = gst_caps_new_simple ("video/x-bayer",
          "format", G_TYPE_STRING, bay_fmt,
          "bpp", G_TYPE_INT, bpp,
          "endianness", G_TYPE_INT, endianness,
          "width", G_TYPE_INT, width,
          "height", G_TYPE_INT, height,
          "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
    }
  } else if (videoFormat != GST_VIDEO_FORMAT_UNKNOWN) {
    vinfo.finfo = gst_video_format_get_info (videoFormat);
    vinfo.width = width;
    vinfo.height = height;

    caps = gst_video_info_to_caps (&vinfo);

    if (is_gray16) {
      GValue val = G_VALUE_INIT;
      GstStructure *s = gst_caps_get_structure (caps, 0);
      g_value_init (&val, G_TYPE_INT);
      g_value_set_int (&val, bpp);
      gst_structure_set_value (s, "bpp", &val);
      g_value_unset (&val);
    }
  } else {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        (("Unknown or unsupported color format.")), (NULL));
    goto Error;
  }

  /* get buffer size; width (in bytes) and height (in lines) */
  eStat = PHX_ParameterGet (src->hCamera, PHX_BUF_DST_XLENGTH, &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  src->phx_stride = dwParamValue;
  eStat = PHX_ParameterGet (src->hCamera, PHX_BUF_DST_YLENGTH, &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  /* TODO: should we be using PHX_BUF_DST_YLENGTH or PHX_ROI_YLENGTH_SCALED
     for height? */
  g_assert (dwParamValue == height);

  GST_DEBUG_OBJECT (src, "The caps before filtering are %" GST_PTR_FORMAT,
      caps);

  if (filter) {
    GstCaps *tmp = gst_caps_intersect (caps, filter);
    gst_caps_unref (caps);
    caps = tmp;
  }

  GST_DEBUG_OBJECT (src, "The caps after filtering are %" GST_PTR_FORMAT, caps);

  return caps;

ResourceSettingsError:
  GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
      (("Failed to get Phoenix parameters.")), (NULL));

Error:
  return NULL;
}

static gboolean
gst_phoenixsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstPhoenixSrc *src = GST_PHOENIX_SRC (bsrc);
  GstVideoInfo vinfo;
  GstStructure *s = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);

  gst_video_info_from_caps (&vinfo, caps);

  if (g_str_equal ("video/x-bayer", gst_structure_get_name (s))) {
    gint width;
    const gchar *format;
    gst_structure_get_int (s, "width", &width);
    gst_structure_get_int (s, "height", &src->height);
    format = gst_structure_get_string (s, "format");
    if (g_str_has_suffix (format, "16"))
      src->gst_stride = GST_ROUND_UP_4 (width * 2);
    else
      src->gst_stride = GST_ROUND_UP_4 (width);
  } else if (GST_VIDEO_INFO_FORMAT (&vinfo) != GST_VIDEO_FORMAT_UNKNOWN) {
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

static GstFlowReturn
gst_phoenixsrc_create (GstPushSrc * src, GstBuffer ** buf)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);
  etStat eStat = PHX_OK;        /* Phoenix status variable */
  ui32 dwParamValue = 0;        /* Phoenix Get/Set intermediate variable */
  stImageBuff phx_buffer;
  guint dropped_frame_count = 0;
  guint new_dropped_frames;
  gint i;
  guint n;
  GstMapInfo minfo;

  /* Start acquisition */
  if (!phoenixsrc->acq_started) {
    /* make class instance pointer available to the callback, and flush cache */
    PHX_ParameterSet (phoenixsrc->hCamera,
        PHX_EVENT_CONTEXT | PHX_CACHE_FLUSH, (void *) phoenixsrc);

    /* Now start our capture */
    eStat = PHX_Acquire (phoenixsrc->hCamera, PHX_START, (void *) phx_callback);
    if (PHX_OK != eStat) {
      GST_ELEMENT_ERROR (phoenixsrc, RESOURCE, FAILED,
          (("Failed to start acquisition.")), (NULL));
      return GST_FLOW_ERROR;    /* TODO: make sure _stop is called if this happens to release resources */
    }
    phoenixsrc->acq_started = TRUE;
  }

  /* about to read/write variables modified by phx_callback */
  g_mutex_lock (&phoenixsrc->mutex);

  /* wait for callback (we should always get at least a timeout( */
  g_cond_wait (&phoenixsrc->cond, &phoenixsrc->mutex);

  if (phoenixsrc->fifo_overflow_occurred) {
    /* TODO: we could offer to try and ABORT then re-START capture */
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        (("Acquisition failure due to FIFO overflow.")), (NULL));
    g_mutex_unlock (&phoenixsrc->mutex);
    return GST_FLOW_ERROR;
  }

  if (phoenixsrc->timeout_occurred) {
    /* TODO: we could offer to try and ABORT then re-START capture */
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        (("Acquisition failure due to timeout.")), (NULL));
    g_mutex_unlock (&phoenixsrc->mutex);
    return GST_FLOW_ERROR;
  }

  if (!phoenixsrc->buffer_ready) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        (("You should not see this error, something very bad happened.")),
        (NULL));
    g_mutex_unlock (&phoenixsrc->mutex);
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (phoenixsrc,
      "Processing new buffer %d (Frame start: %d), ready-processed = %d",
      phoenixsrc->buffer_ready_count, phoenixsrc->frame_start_count,
      phoenixsrc->buffer_ready_count - phoenixsrc->buffer_processed_count);
  phoenixsrc->buffer_ready = FALSE;

  /* frame_start is always >= buffer_ready */
  dropped_frame_count =
      phoenixsrc->frame_start_count - phoenixsrc->buffer_ready_count;

  g_mutex_unlock (&phoenixsrc->mutex);

  eStat = PHX_Acquire (phoenixsrc->hCamera, PHX_BUFFER_GET, &phx_buffer);
  if (PHX_OK != eStat) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        (("Failed to get buffer.")), (NULL));
    return GST_FLOW_ERROR;
  }

  /* TODO: use allocator or use from Phoenix pool */
  *buf = gst_buffer_new_and_alloc (phoenixsrc->height * phoenixsrc->gst_stride);

  /* Copy image to buffer from surface TODO: use orc_memcpy */
  gst_buffer_map (*buf, &minfo, GST_MAP_WRITE);
  GST_LOG_OBJECT (phoenixsrc,
      "GstBuffer size=%d, gst_stride=%d, phx_stride=%d", minfo.size,
      phoenixsrc->gst_stride, phoenixsrc->phx_stride);
  for (i = 0; i < phoenixsrc->height; i++) {
    memcpy (minfo.data + i * phoenixsrc->gst_stride,
        ((guint8 *) phx_buffer.pvAddress) + i * phoenixsrc->phx_stride,
        phoenixsrc->gst_stride);
  }
  gst_buffer_unmap (*buf, &minfo);

  /* Having processed the data, release the buffer ready for further image data */
  eStat = PHX_Acquire (phoenixsrc->hCamera, PHX_BUFFER_RELEASE, NULL);
  phoenixsrc->buffer_processed_count++;

  /* check for dropped frames (can only detect more than one) */
  new_dropped_frames = dropped_frame_count - phoenixsrc->dropped_frame_count;
  if (new_dropped_frames > 0) {
    phoenixsrc->dropped_frame_count = dropped_frame_count;
    GST_WARNING ("Dropped %d frames (%d total)", new_dropped_frames,
        phoenixsrc->dropped_frame_count);
    /* TODO: emit message here about dropped frames */
  }

  /* use time from capture board */
  n = (phoenixsrc->buffer_processed_count -
      1) % phoenixsrc->num_capture_buffers;
  GST_BUFFER_TIMESTAMP (*buf) = phoenixsrc->frame_start_times[n];
  GST_BUFFER_DURATION (*buf) =
      GST_CLOCK_DIFF (phoenixsrc->frame_start_times[n],
      phoenixsrc->frame_end_times[n]);
  GST_BUFFER_OFFSET (*buf) = phoenixsrc->buffer_processed_count - 1;
  GST_BUFFER_OFFSET_END (*buf) = GST_BUFFER_OFFSET (*buf);

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_phoenixsrc_debug, "phoenixsrc", 0,
      "debug category for phoenixsrc element");
  gst_element_register (plugin, "phoenixsrc", GST_RANK_NONE,
      gst_phoenixsrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    phoenix,
    "Phoenix frame grabber source",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
