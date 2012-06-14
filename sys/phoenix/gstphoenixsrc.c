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
    * test sending of API-provided buffers, using GST_BUFFER_FREE_FUNC
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>
#include <gst/gst-i18n-lib.h>

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

static GstCaps *gst_phoenixsrc_get_caps (GstBaseSrc * src);
static gboolean gst_phoenixsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_phoenixsrc_newsegment (GstBaseSrc * src);
static gboolean gst_phoenixsrc_start (GstBaseSrc * src);
static gboolean gst_phoenixsrc_stop (GstBaseSrc * src);
static void
gst_phoenixsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_phoenixsrc_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_phoenixsrc_is_seekable (GstBaseSrc * src);
static gboolean gst_phoenixsrc_query (GstBaseSrc * src, GstQuery * query);
static gboolean gst_phoenixsrc_check_get_range (GstBaseSrc * src);
static void gst_phoenixsrc_fixate (GstBaseSrc * src, GstCaps * caps);
static GstFlowReturn gst_phoenixsrc_create (GstPushSrc * src, GstBuffer ** buf);

enum
{
  PROP_0,
  PROP_CAMERA_CONFIG_FILEPATH,
  PROP_NUM_CAPTURE_BUFFERS
};

#define DEFAULT_PROP_CAMERA_CONFIG_FILEPATH NULL        /* defaults to 640x480x8bpp */
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 2

/* pad templates */

static GstStaticPadTemplate gst_phoenixsrc_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_GRAY8 ";"
        GST_VIDEO_CAPS_GRAY16 ("BIG_ENDIAN") ";"
        GST_VIDEO_CAPS_GRAY16 ("LITTLE_ENDIAN") ";"
        GST_VIDEO_CAPS_RGB ";"
        GST_VIDEO_CAPS_xRGB ";" GST_VIDEO_CAPS_RGB_15 ";" GST_VIDEO_CAPS_RGB_16)
    );

/* class initialization */

GST_BOILERPLATE (GstPhoenixSrc, gst_phoenixsrc, GstPushSrc, GST_TYPE_PUSH_SRC);


static GstVideoFormat
gst_phoenixsrc_color_format_to_video_format (int dst_format, int dst_endian)
{
  switch (dst_format) {
    case PHX_DST_FORMAT_Y8:
      return GST_VIDEO_FORMAT_GRAY8;
      /* TODO: possibly use different formats for each of the following */
    case PHX_DST_FORMAT_Y10:
    case PHX_DST_FORMAT_Y12:
    case PHX_DST_FORMAT_Y14:
    case PHX_DST_FORMAT_Y16:
      if (dst_endian == PHX_DST_LITTLE_ENDIAN)
        return GST_VIDEO_FORMAT_GRAY16_LE;
      else if (dst_endian == PHX_DST_BIG_ENDIAN)
        return GST_VIDEO_FORMAT_GRAY16_BE;
      else
        return GST_VIDEO_FORMAT_UNKNOWN;
      /* TODO: Bayer here */
    case PHX_DST_FORMAT_RGB15:
      return GST_VIDEO_FORMAT_RGB15;
    case PHX_DST_FORMAT_RGB16:
      return GST_VIDEO_FORMAT_RGB16;
    case PHX_DST_FORMAT_RGB24:
      return GST_VIDEO_FORMAT_RGB;
    case PHX_DST_FORMAT_RGB32: /* FIXME: what is the format of this? */
    case PHX_DST_FORMAT_XRGB32:
      return GST_VIDEO_FORMAT_xRGB;
    default:
      return GST_VIDEO_FORMAT_UNKNOWN;
  }
};

static void
gst_phoenixsrc_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_phoenixsrc_src_template));

  gst_element_class_set_details_simple (element_class,
      "Active Silicon Phoenix Video Source", "Source/Video",
      "Active Silicon Phoenix framegrabber video source",
      "Joshua M. Doe <oss@nvl.army.mil>");
}

static void
gst_phoenixsrc_class_init (GstPhoenixSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_phoenixsrc_set_property;
  gobject_class->get_property = gst_phoenixsrc_get_property;
  gobject_class->dispose = gst_phoenixsrc_dispose;
  gobject_class->finalize = gst_phoenixsrc_finalize;
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_phoenixsrc_get_caps);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_phoenixsrc_set_caps);
  base_src_class->newsegment = GST_DEBUG_FUNCPTR (gst_phoenixsrc_newsegment);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_phoenixsrc_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_phoenixsrc_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_phoenixsrc_get_times);
  base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_phoenixsrc_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_phoenixsrc_is_seekable);
  base_src_class->query = GST_DEBUG_FUNCPTR (gst_phoenixsrc_query);
  base_src_class->check_get_range =
      GST_DEBUG_FUNCPTR (gst_phoenixsrc_check_get_range);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_phoenixsrc_fixate);

  push_src_class->create = GST_DEBUG_FUNCPTR (gst_phoenixsrc_create);

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

}

static void
gst_phoenixsrc_init (GstPhoenixSrc * phoenixsrc,
    GstPhoenixSrcClass * phoenixsrc_class)
{
  phoenixsrc->srcpad =
      gst_pad_new_from_static_template (&gst_phoenixsrc_src_template, "src");

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

  phoenixsrc->mutex = g_mutex_new ();
  phoenixsrc->cond = g_cond_new ();
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

  G_OBJECT_CLASS (parent_class)->dispose (object);
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

  g_mutex_free (phoenixsrc->mutex);
  g_cond_free (phoenixsrc->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GstCaps *
gst_phoenixsrc_get_caps (GstBaseSrc * src)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);

  GST_DEBUG_OBJECT (phoenixsrc, "get_caps");

  /* return template caps if we don't know the actual camera caps */
  if (!phoenixsrc->caps) {
    return
        gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD
            (phoenixsrc)));
  }

  return gst_caps_copy (phoenixsrc->caps);

  return NULL;
}

static gboolean
gst_phoenixsrc_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);
  GstStructure *structure;
  gboolean ret;
  gint width, height;

  GST_DEBUG_OBJECT (phoenixsrc, "set_caps");

  if (phoenixsrc->caps) {
    gst_caps_unref (phoenixsrc->caps);
    phoenixsrc->caps = gst_caps_copy (caps);
  }

  /* parse caps */

  if (gst_caps_get_size (caps) < 1)
    return FALSE;

  structure = gst_caps_get_structure (caps, 0);

  ret = gst_structure_get (structure,
      "width", G_TYPE_INT, &width, "height", G_TYPE_INT, &height, NULL);

  if (!ret) {
    GST_DEBUG ("Failed to retrieve width and height");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_phoenixsrc_newsegment (GstBaseSrc * src)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);

  GST_DEBUG_OBJECT (phoenixsrc, "newsegment");

  return TRUE;
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

  g_mutex_lock (phoenixsrc->mutex);

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
    g_cond_signal (phoenixsrc->cond);
  g_mutex_unlock (phoenixsrc->mutex);
  /* after unlocking, _create will check for these errors and copy data */
}

static gboolean
gst_phoenixsrc_start (GstBaseSrc * src)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);
  etStat eStat = PHX_OK;        /* Status variable */
  etParamValue eParamValue = 0;
  ui32 dwParamValue = 0;
  guint32 phx_format, phx_endian, width, height;
  GstVideoFormat videoFormat;
  ui32 dwBufferWidth, dwBufferHeight;

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

  /* Initialize board */
  /* TODO: this picks first digital board using default settings, parameterize this! */
  eStat =
      PHX_CameraConfigLoad (&phoenixsrc->hCamera, phoenixsrc->config_filepath,
      PHX_BOARD_AUTO | PHX_DIGITAL, PHX_ErrHandlerDefault);
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


  /* Get format (mono, Bayer, RBG, etc.) */
  eStat = PHX_ParameterGet (phoenixsrc->hCamera, PHX_DST_FORMAT, &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  phx_format = dwParamValue;

  /* Get endianness */
  eStat = PHX_ParameterGet (phoenixsrc->hCamera, PHX_DST_ENDIAN, &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  phx_endian = dwParamValue;

  /* get width */
  eStat =
      PHX_ParameterGet (phoenixsrc->hCamera, PHX_ROI_XLENGTH_SCALED,
      &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  width = dwParamValue;

  /* get height */
  eStat =
      PHX_ParameterGet (phoenixsrc->hCamera, PHX_ROI_YLENGTH_SCALED,
      &dwParamValue);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  height = dwParamValue;

  /* get buffer size; width (in bytes) and height (in lines) */
  eStat =
      PHX_ParameterGet (phoenixsrc->hCamera, PHX_BUF_DST_XLENGTH,
      &dwBufferWidth);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  eStat =
      PHX_ParameterGet (phoenixsrc->hCamera, PHX_BUF_DST_YLENGTH,
      &dwBufferHeight);
  if (PHX_OK != eStat)
    goto ResourceSettingsError;
  phoenixsrc->buffer_size = dwBufferHeight * dwBufferWidth;

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

  videoFormat =
      gst_phoenixsrc_color_format_to_video_format (phx_format, phx_endian);
  if (videoFormat == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ELEMENT_ERROR (phoenixsrc, STREAM, WRONG_TYPE,
        (_("Unknown or unsupported color format.")), (NULL));
    goto Error;
  }

  if (phoenixsrc->caps)
    gst_caps_unref (phoenixsrc->caps);
  phoenixsrc->caps =
      gst_video_format_new_caps (videoFormat, width, height, 30, 1, 1, 1);

  if (phoenixsrc->caps == NULL) {
    GST_ELEMENT_ERROR (phoenixsrc, STREAM, TOO_LAZY,
        (_("Failed to generate caps from video format.")), (NULL));
    goto Error;
  }

  return TRUE;

ResourceSettingsError:
  GST_ELEMENT_ERROR (phoenixsrc, RESOURCE, SETTINGS,
      (_("Failed to get Phoenix parameters.")), (NULL));

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

  gst_caps_unref (phoenixsrc->caps);
  phoenixsrc->caps = NULL;

  phoenixsrc->dropped_frame_count = 0;
  /*phoenixsrc->last_time_code = -1; */

  return TRUE;
}

static void
gst_phoenixsrc_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);

  GST_DEBUG_OBJECT (phoenixsrc, "get_times");
}

static gboolean
gst_phoenixsrc_get_size (GstBaseSrc * src, guint64 * size)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);

  GST_DEBUG_OBJECT (phoenixsrc, "get_size");

  return TRUE;
}

static gboolean
gst_phoenixsrc_is_seekable (GstBaseSrc * src)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);

  GST_DEBUG_OBJECT (phoenixsrc, "is_seekable");

  return FALSE;
}

static gboolean
gst_phoenixsrc_query (GstBaseSrc * src, GstQuery * query)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);

  GST_DEBUG_OBJECT (phoenixsrc, "query");

  return TRUE;
}

static gboolean
gst_phoenixsrc_check_get_range (GstBaseSrc * src)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);

  GST_DEBUG_OBJECT (phoenixsrc, "get_range");

  return FALSE;
}

static void
gst_phoenixsrc_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);

  GST_DEBUG_OBJECT (phoenixsrc, "fixate");
}

static GstFlowReturn
gst_phoenixsrc_create (GstPushSrc * src, GstBuffer ** buf)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);
  GstFlowReturn ret;
  etStat eStat = PHX_OK;        /* Phoenix status variable */
  ui32 dwParamValue = 0;        /* Phoenix Get/Set intermediate variable */
  stImageBuff phx_buffer;
  guint dropped_frame_count = 0;
  guint new_dropped_frames;
  guint n;

  /* Create and allocate the buffer */
  ret = gst_pad_alloc_buffer (GST_BASE_SRC_PAD (GST_BASE_SRC (src)),
      GST_BUFFER_OFFSET_NONE, phoenixsrc->buffer_size,
      GST_PAD_CAPS (GST_BASE_SRC_PAD (GST_BASE_SRC (src))), buf);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    return GST_FLOW_ERROR;
  }

  /* Start acquisition */
  if (!phoenixsrc->acq_started) {
    /* make class instance pointer available to the callback, and flush cache */
    PHX_ParameterSet (phoenixsrc->hCamera, PHX_EVENT_CONTEXT | PHX_CACHE_FLUSH,
        (void *) phoenixsrc);

    /* Now start our capture */
    eStat = PHX_Acquire (phoenixsrc->hCamera, PHX_START, (void *) phx_callback);
    if (PHX_OK != eStat) {
      GST_ELEMENT_ERROR (phoenixsrc, RESOURCE, FAILED,
          (_("Failed to start acquisition.")), (NULL));
      return GST_FLOW_ERROR;    /* TODO: make sure _stop is called if this happens to release resources */
    }
    phoenixsrc->acq_started = TRUE;
  }

  /* about to read/write variables modified by phx_callback */
  g_mutex_lock (phoenixsrc->mutex);

  /* wait for callback (we should always get at least a timeout( */
  g_cond_wait (phoenixsrc->cond, phoenixsrc->mutex);

  if (phoenixsrc->fifo_overflow_occurred) {
    /* TODO: we could offer to try and ABORT then re-START capture */
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        (_("Acquisition failure due to FIFO overflow.")), (NULL));
    g_mutex_unlock (phoenixsrc->mutex);
    return GST_FLOW_ERROR;
  }

  if (phoenixsrc->timeout_occurred) {
    /* TODO: we could offer to try and ABORT then re-START capture */
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        (_("Acquisition failure due to timeout.")), (NULL));
    g_mutex_unlock (phoenixsrc->mutex);
    return GST_FLOW_ERROR;
  }

  if (!phoenixsrc->buffer_ready) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        (_("You should not see this error, something very bad happened.")),
        (NULL));
    g_mutex_unlock (phoenixsrc->mutex);
    return GST_FLOW_ERROR;
  }

  GST_INFO_OBJECT (phoenixsrc,
      "Processing new buffer %d (Frame start: %d), ready-processed = %d",
      phoenixsrc->buffer_ready_count, phoenixsrc->frame_start_count,
      phoenixsrc->buffer_ready_count - phoenixsrc->buffer_processed_count);
  phoenixsrc->buffer_ready = FALSE;

  /* frame_start is always >= buffer_ready */
  dropped_frame_count =
      phoenixsrc->frame_start_count - phoenixsrc->buffer_ready_count;

  g_mutex_unlock (phoenixsrc->mutex);

  eStat = PHX_Acquire (phoenixsrc->hCamera, PHX_BUFFER_GET, &phx_buffer);
  if (PHX_OK != eStat) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        (_("Failed to get buffer.")), (NULL));
    return GST_FLOW_ERROR;
  }

  /* Copy image to buffer from surface TODO: use orc_memcpy */
  memcpy (GST_BUFFER_DATA (*buf), phx_buffer.pvAddress,
      phoenixsrc->buffer_size);

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

  GST_BUFFER_SIZE (*buf) = phoenixsrc->buffer_size;
  /* use time from capture board */
  n = (phoenixsrc->buffer_processed_count -
      1) % phoenixsrc->num_capture_buffers;
  GST_BUFFER_TIMESTAMP (*buf) = phoenixsrc->frame_start_times[n];
  GST_BUFFER_DURATION (*buf) = GST_CLOCK_DIFF (phoenixsrc->frame_start_times[n],
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
    "phoenixsrc",
    "Phoenix frame grabber source",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
