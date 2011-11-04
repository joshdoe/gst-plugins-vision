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
    PROP_CAMERA_CONFIG_FILEPATH/*,
    PROP_BOARD_INDEX,
    PROP_CAMERA_TYPE,
    PROP_CONNECTOR*/
    /* FILL ME */
};

#define DEFAULT_PROP_CAMERA_CONFIG_FILEPATH NULL /* defaults to 640x480x8bpp */
/*#define DEFAULT_PROP_BOARD_INDEX 0
#define DEFAULT_PROP_CAMERA_TYPE  MC_Camera_CAMERA_NTSC
#define DEFAULT_PROP_CONNECTOR  MC_Connector_VID1*/

/* pad templates */

static GstStaticPadTemplate gst_phoenixsrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
				GST_VIDEO_CAPS_GRAY8 ";"
        GST_VIDEO_CAPS_GRAY16("BIG_ENDIAN") ";"
        GST_VIDEO_CAPS_GRAY16("LITTLE_ENDIAN") ";"
				GST_VIDEO_CAPS_RGB ";"
				GST_VIDEO_CAPS_ARGB ";"
				GST_VIDEO_CAPS_RGB_15 ";"
				GST_VIDEO_CAPS_RGB_16)
    );


/*#define GST_TYPE_PHOENIX_SRC_CONNECTOR (gst_phoenixsrc_connector_get_type())
static GType
gst_phoenixsrc_connector_get_type (void)
{
  static GType phoenixsrc_connector_type = 0;
  static const GEnumValue phoenixsrc_connector[] = {
    {MC_Connector_VID1, "VID1", "VID1 input"},
    {MC_Connector_VID2, "VID2", "VID2 input"},
    {MC_Connector_VID3, "VID3", "VID3 input"},
    {MC_Connector_VID4, "VID4", "VID4 input"},
    {MC_Connector_VID5, "VID5", "VID5 input"},
    {MC_Connector_VID6, "VID6", "VID6 input"},
    {MC_Connector_VID7, "VID7", "VID7 input"},
    {MC_Connector_VID8, "VID8", "VID8 input"},
    {MC_Connector_VID9, "VID9", "VID9 input"},
    {MC_Connector_VID10, "VID10", "VID10 input"},
    {MC_Connector_VID11, "VID11", "VID11 input"},
    {MC_Connector_VID12, "VID12", "VID12 input"},
    {MC_Connector_VID13, "VID13", "VID13 input"},
    {MC_Connector_VID14, "VID14", "VID14 input"},
    {MC_Connector_VID15, "VID15", "VID15 input"},
    {MC_Connector_VID16, "VID16", "VID16 input"},
    {MC_Connector_YC, "YC", "YC input using the MiniDIN4 or DB9 connector"},
    {MC_Connector_YC1, "YC1", "YC1 input using the HD44 connector"},
    {MC_Connector_YC2, "YC2", "YC2 input using the HD44 connector"},
    {MC_Connector_YC3, "YC3", "YC3 input using the HD44 connector"},
    {MC_Connector_YC4, "YC4", "YC4 input using the HD44 connector"},
    {MC_Connector_X, "X", "X input"},
    {MC_Connector_Y, "Y", "Y input"},
    {MC_Connector_XBIS, "XBIS", "XBIS input using the secondary lane"},
    {MC_Connector_YBIS, "YBIS", "YBIS input using the secondary lane"},
    {MC_Connector_X1, "X1", "X1 input"},
    {MC_Connector_X2, "X2", "X2 input"},
    {MC_Connector_Y1, "Y1", "Y1 input"},
    {MC_Connector_Y2, "Y2", "Y2 input"},
    {MC_Connector_A, "A", "A input (Grablink Expert 2 DuoCam mode, connector A)"},
    {MC_Connector_B, "B", "B input (Grablink Expert 2 DuoCam mode, connector B)"},
    {MC_Connector_M, "M", "M input (Grablink in MonoCam mode)"},
    {0, NULL, NULL},
  };

  if (!phoenixsrc_connector_type) {
    phoenixsrc_connector_type =
      g_enum_register_static ("GstPhoenixSrcConnector", phoenixsrc_connector);
  }
  return phoenixsrc_connector_type;
}*/

/*#define GST_TYPE_PHOENIX_SRC_CAMERA (gst_phoenixsrc_camera_get_type())
static GType
gst_phoenixsrc_camera_get_type (void)
{
  static GType phoenixsrc_camera_type = 0;
  static const GEnumValue phoenixsrc_camera[] = {
    {MC_Camera_CAMERA_CCIR, "CCIR", "CCIR broadcasting standard"},
    {MC_Camera_CAMERA_EIA, "EIA", "EIA broadcasting standard"},
    {MC_Camera_CAMERA_PAL, "PAL", "PAL broadcasting standard"},
    {MC_Camera_CAMERA_NTSC, "NTSC", "NTSC broadcasting standard"},
    {0, NULL, NULL},
  };

  if (!phoenixsrc_camera_type) {
    phoenixsrc_camera_type =
      g_enum_register_static ("GstPhoenixSrcCamera", phoenixsrc_camera);
  }
  return phoenixsrc_camera_type;
}*/


/* class initialization */

GST_BOILERPLATE (GstPhoenixSrc, gst_phoenixsrc, GstPushSrc,
    GST_TYPE_PUSH_SRC);


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
      "Joshua Doe <joshua.m.doe2.civ@mail.mil>");
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
  base_src_class->check_get_range = GST_DEBUG_FUNCPTR (gst_phoenixsrc_check_get_range);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_phoenixsrc_fixate);

  push_src_class->create = GST_DEBUG_FUNCPTR (gst_phoenixsrc_create);


  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_CAMERA_CONFIG_FILEPATH,
      g_param_spec_string ("config-file", "Config file",
          "Camera configuration filepath", DEFAULT_PROP_CAMERA_CONFIG_FILEPATH,
          G_PARAM_READWRITE));
  //g_object_class_install_property (gobject_class, PROP_CAMERA_TYPE,
  //    g_param_spec_enum ("camera", "Camera",
  //        "Camera type", GST_TYPE_PHOENIX_SRC_CAMERA,
  //        DEFAULT_PROP_CAMERA_TYPE, G_PARAM_READWRITE));
  //g_object_class_install_property (gobject_class, PROP_CONNECTOR,
  //    g_param_spec_enum ("connector", "Connector",
  //        "Connector where camera is attached", GST_TYPE_PHOENIX_SRC_CONNECTOR,
  //        DEFAULT_PROP_CONNECTOR, G_PARAM_READWRITE));

}

static void
gst_phoenixsrc_init (GstPhoenixSrc * phoenixsrc, GstPhoenixSrcClass * phoenixsrc_class)
{
  phoenixsrc->srcpad = gst_pad_new_from_static_template (&gst_phoenixsrc_src_template
      , "src");

  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (phoenixsrc), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (phoenixsrc), GST_FORMAT_TIME);

  /* initialize member variables */
  phoenixsrc->config_filepath = g_strdup (DEFAULT_PROP_CAMERA_CONFIG_FILEPATH);
  phoenixsrc->buffer_ready = FALSE;
  phoenixsrc->buffer_ready_count = 0;
  phoenixsrc->timeout_occurred = FALSE;
  phoenixsrc->fifo_overflow_occurred = FALSE;

  phoenixsrc->mutex = g_mutex_new ();
  phoenixsrc->cond = g_cond_new ();

  // phoenixsrc->boardIdx = DEFAULT_PROP_BOARD_INDEX;
 // phoenixsrc->cameraType = DEFAULT_PROP_CAMERA_TYPE;
 // phoenixsrc->connector = DEFAULT_PROP_CONNECTOR;

 // phoenixsrc->hChannel = 0;
 // phoenixsrc->caps = NULL;

 // phoenixsrc->acq_started = FALSE;

	//phoenixsrc->last_time_code = -1;
	//phoenixsrc->dropped_frame_count = 0;

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
    return gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (phoenixsrc)));
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
    "width", G_TYPE_INT, &width,
    "height", G_TYPE_INT, &height,
    NULL);

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

/* Callback function to handle image capture events. */
void
phx_callback (tHandle hCamera, ui32 dwMask, void* pvParams )
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (pvParams);

  g_mutex_lock (phoenixsrc->mutex);

  if (PHX_INTRPT_BUFFER_READY & dwMask) {
    /* we have a buffer */
    phoenixsrc->buffer_ready = TRUE;
    phoenixsrc->buffer_ready_count++;
  }
  if (PHX_INTRPT_TIMEOUT & dwMask) {
    /* TODO: we could offer to try and ABORT then re-START capture */
    phoenixsrc->timeout_occurred = TRUE;
  }
  if (PHX_INTRPT_FIFO_OVERFLOW & dwMask) {
    phoenixsrc->fifo_overflow_occurred = TRUE;

  }

  g_cond_signal (phoenixsrc->cond);
  g_mutex_unlock (phoenixsrc->mutex);
}

static gboolean
gst_phoenixsrc_start (GstBaseSrc * src)
{
  GstPhoenixSrc *phoenixsrc = GST_PHOENIX_SRC (src);
  etStat       eStat              = PHX_OK; /* Status variable */
  etParamValue eParamValue        = 0;
  ui32         dwParamValue       = 0;
  guint32 phx_format, phx_endian, width, height;
  GstVideoFormat videoFormat;
  ui32 dwBufferWidth, dwBufferHeight;

  GST_DEBUG_OBJECT (phoenixsrc, "start");

  if (phoenixsrc->config_filepath == NULL) {
    GST_WARNING_OBJECT (phoenixsrc, "No config file set, using default 640x480x8bpp");
  }
  else if (!g_file_test (phoenixsrc->config_filepath, G_FILE_TEST_EXISTS)) {
    GST_ELEMENT_ERROR (phoenixsrc, RESOURCE, NOT_FOUND,
        ("Camera config file does not exist: %s", phoenixsrc->config_filepath),
        (NULL));
    goto Error;
  }

  /* Initialize board */
  /* TODO: this picks first digital board using default settings, parameterize this! */
  eStat = PHX_CameraConfigLoad (&phoenixsrc->hCamera, phoenixsrc->config_filepath,
      PHX_BOARD_AUTO | PHX_DIGITAL, PHX_ErrHandlerDefault);
  if (eStat != PHX_OK) {
    GST_ELEMENT_ERROR (phoenixsrc, LIBRARY, INIT, (NULL), (NULL));
    goto Error;
  }

  /* capture frames continuously */
  eParamValue = PHX_ENABLE;
  eStat = PHX_ParameterSet (phoenixsrc->hCamera, PHX_ACQ_CONTINUOUS, &eParamValue);
  if (PHX_OK != eStat) goto ResourceSettingsError;

  /* Get format (mono, Bayer, RBG, etc.) */
  eStat = PHX_ParameterGet (phoenixsrc->hCamera, PHX_DST_FORMAT, &dwParamValue);
  if (PHX_OK != eStat) goto ResourceSettingsError;
  phx_format = dwParamValue;
 
  /* Get endianness */
  eStat = PHX_ParameterGet (phoenixsrc->hCamera, PHX_DST_ENDIAN, &dwParamValue);
  if (PHX_OK != eStat) goto ResourceSettingsError;
  phx_endian = dwParamValue;

  /* get width */
  eStat = PHX_ParameterGet (phoenixsrc->hCamera, PHX_ROI_XLENGTH_SCALED, &dwParamValue);
  if (PHX_OK != eStat) goto ResourceSettingsError;
  width = dwParamValue;

  /* get height */
  eStat = PHX_ParameterGet (phoenixsrc->hCamera, PHX_ROI_YLENGTH_SCALED, &dwParamValue);
  if (PHX_OK != eStat) goto ResourceSettingsError;
  height = dwParamValue;

  /* get buffer size; width (in bytes) and height (in lines) */
  eStat = PHX_ParameterGet (phoenixsrc->hCamera, PHX_BUF_DST_XLENGTH, &dwBufferWidth);
  if (PHX_OK != eStat) goto ResourceSettingsError;
  eStat = PHX_ParameterGet (phoenixsrc->hCamera, PHX_BUF_DST_YLENGTH, &dwBufferHeight);
  if (PHX_OK != eStat) goto ResourceSettingsError;
  phoenixsrc->buffer_size = dwBufferHeight * dwBufferWidth;

  ///* Tell Phoenix we are using two buffers. */
  //eParamValue = 2;
  //PHX_ParameterSet (phoenixsrc->hCamera, PHX_ACQ_NUM_IMAGES, &eParamValue);

  /* Setup a one second timeout value (milliseconds) */
  dwParamValue = 1000;
  eStat = PHX_ParameterSet (phoenixsrc->hCamera, PHX_TIMEOUT_DMA, (void *) &dwParamValue);
  if (PHX_OK != eStat) goto ResourceSettingsError;

  /* The BUFFER_READY interrupt is already enabled by default,
  * but we must enable the TIMEOUT and FIFO_OVERFLOW interrupts here. */
  eParamValue = PHX_INTRPT_TIMEOUT | PHX_INTRPT_FIFO_OVERFLOW;
  eStat = PHX_ParameterSet (phoenixsrc->hCamera, PHX_INTRPT_SET, (void *) &eParamValue);
  if (PHX_OK != eStat) goto ResourceSettingsError;

  videoFormat = gst_phoenixsrc_color_format_to_video_format (phx_format, phx_endian);
  if (videoFormat == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ELEMENT_ERROR (phoenixsrc, STREAM, WRONG_TYPE,
      (_("Unknown or unsupported color format.")), (NULL));
    goto Error;
  }

  if (phoenixsrc->caps)
    gst_caps_unref (phoenixsrc->caps);
  phoenixsrc->caps = gst_video_format_new_caps (videoFormat, width, height, 30, 1, 1, 1);

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
  if (phoenixsrc->hCamera) PHX_Acquire(phoenixsrc->hCamera, PHX_ABORT, NULL );

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
  if (phoenixsrc->hCamera) PHX_CameraRelease(&phoenixsrc->hCamera);

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
	/*phoenixsrc->last_time_code = -1;*/

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
  etStat eStat = PHX_OK; /* Phoenix status variable */
  ui32 dwParamValue = 0; /* Phoenix Get/Set intermediate variable */
  stImageBuff stBuffer;
  GTimeVal timeVal;

  /* Start acquisition */
  if (!phoenixsrc->acq_started) {
    /* Setup our own event context */
    PHX_ParameterSet (phoenixsrc->hCamera, PHX_EVENT_CONTEXT, (void *) phoenixsrc);

    /* Now start our capture */
    eStat = PHX_Acquire (phoenixsrc->hCamera, PHX_START, (void*)phx_callback);
    if (PHX_OK != eStat) {
      GST_ELEMENT_ERROR (phoenixsrc, RESOURCE, FAILED,
          (_("Failed to start acquisition.")), (NULL));
      return GST_FLOW_ERROR; /* TODO: make sure _stop is called if this happens to release resources*/
    }
    phoenixsrc->acq_started = TRUE;
  }

  g_mutex_lock (phoenixsrc->mutex);

  /* wait up to 5 seconds for an interrupt */
  g_get_current_time (&timeVal);
  g_time_val_add (&timeVal, 5000000); /* TODO: don't hardcode this timeout */
  g_cond_timed_wait (phoenixsrc->cond, phoenixsrc->mutex, &timeVal);

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
  if (phoenixsrc->buffer_ready) {
    GST_INFO_OBJECT (phoenixsrc, "Buffer ready count: %d", phoenixsrc->buffer_ready_count);
    phoenixsrc->buffer_ready = FALSE;
  }
  else {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        (_("Timeout waiting for buffer ready or error.")), (NULL));
    g_mutex_unlock (phoenixsrc->mutex);
    return GST_FLOW_ERROR;
  }

  g_mutex_unlock (phoenixsrc->mutex);

  eStat = PHX_Acquire (phoenixsrc->hCamera, PHX_BUFFER_GET, &stBuffer);
  if (PHX_OK != eStat) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
      (_("Failed to get buffer.")), (NULL));
    return GST_FLOW_ERROR;
  }

  /* TODO: be sure to watch for dropped frames */
	//dropped_frame_count = timeCode - (phoenixsrc->last_time_code + 1);
	//if (dropped_frame_count != 0) {
	//	phoenixsrc->dropped_frame_count += dropped_frame_count;
	//	GST_WARNING ("Dropped %d frames (%d total)", dropped_frame_count, phoenixsrc->dropped_frame_count);
	//	/* TODO: emit message here about dropped frames */
	//}
	//phoenixsrc->last_time_code = timeCode;


  /* Create the buffer */
  ret = gst_pad_alloc_buffer (GST_BASE_SRC_PAD (GST_BASE_SRC (src)),
      GST_BUFFER_OFFSET_NONE, phoenixsrc->buffer_size,
      GST_PAD_CAPS (GST_BASE_SRC_PAD (GST_BASE_SRC (src))), buf);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    return GST_FLOW_ERROR;
  }

  /* Copy image to buffer from surface */
  memcpy (GST_BUFFER_DATA (*buf), stBuffer.pvAddress, phoenixsrc->buffer_size);
  GST_BUFFER_SIZE (*buf) = phoenixsrc->buffer_size;
  GST_BUFFER_TIMESTAMP (*buf) =
      gst_clock_get_time (GST_ELEMENT_CLOCK (src)) -
      GST_ELEMENT_CAST (src)->base_time;

  GST_INFO ("Buffer size: %d", sizeof(GstBuffer));
  /* Having processed the data, release the buffer ready for further image data */
  eStat = PHX_Acquire (phoenixsrc->hCamera, PHX_BUFFER_RELEASE, NULL);

  return GST_FLOW_OK;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_phoenixsrc_debug, "phoenixsrc", 0, \
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
