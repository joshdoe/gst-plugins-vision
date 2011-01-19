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
 * SECTION:element-gsteuresys
 *
 * The euresys element is a source for framegrabbers supported by the Euresys Multicam driver.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v euresys ! ffmpegcolorspace ! autovideosink
 * ]|
 * Shows video from the default Euresys framegrabber
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>
#include <gst/gst-i18n-lib.h>
#include "gsteuresys.h"

GST_DEBUG_CATEGORY_STATIC (gst_euresys_debug);
#define GST_CAT_DEFAULT gst_euresys_debug

/* prototypes */


static void gst_euresys_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_euresys_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_euresys_dispose (GObject * object);
static void gst_euresys_finalize (GObject * object);

static GstCaps *gst_euresys_get_caps (GstBaseSrc * src);
static gboolean gst_euresys_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_euresys_negotiate (GstBaseSrc * src);
static gboolean gst_euresys_newsegment (GstBaseSrc * src);
static gboolean gst_euresys_start (GstBaseSrc * src);
static gboolean gst_euresys_stop (GstBaseSrc * src);
static void
gst_euresys_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_euresys_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_euresys_is_seekable (GstBaseSrc * src);
static gboolean gst_euresys_unlock (GstBaseSrc * src);
static gboolean gst_euresys_event (GstBaseSrc * src, GstEvent * event);
static gboolean gst_euresys_do_seek (GstBaseSrc * src, GstSegment * segment);
static gboolean gst_euresys_query (GstBaseSrc * src, GstQuery * query);
static gboolean gst_euresys_check_get_range (GstBaseSrc * src);
static void gst_euresys_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_euresys_unlock_stop (GstBaseSrc * src);
static gboolean
gst_euresys_prepare_seek_segment (GstBaseSrc * src, GstEvent * seek,
    GstSegment * segment);
static GstFlowReturn gst_euresys_create (GstPushSrc * src, GstBuffer ** buf);

enum
{
    PROP_0,
    PROP_BOARD_INDEX,
    PROP_CAMERA_TYPE,
    PROP_CONNECTOR
    /* FILL ME */
};

#define DEFAULT_PROP_BOARD_INDEX 0
#define DEFAULT_PROP_CAMERA_TYPE  MC_Camera_NTSC
#define DEFAULT_PROP_CONNECTOR  MC_Connector_VID1

/* pad templates */

static GstStaticPadTemplate gst_euresys_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/unknown")
    );


#define GST_TYPE_EURESYS_CONNECTOR (gst_euresys_connector_get_type())
static GType
gst_euresys_connector_get_type (void)
{
  static GType euresys_connector_type = 0;
  static const GEnumValue euresys_connector[] = {
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

  if (!euresys_connector_type) {
    euresys_connector_type =
      g_enum_register_static ("GstEuresysConnector", euresys_connector);
  }
  return euresys_connector_type;
}

#define GST_TYPE_EURESYS_CAMERA (gst_euresys_camera_get_type())
static GType
gst_euresys_camera_get_type (void)
{
  static GType euresys_camera_type = 0;
  static const GEnumValue euresys_camera[] = {
    {MC_Camera_CCIR, "CCIR", "CCIR broadcasting standard"},
    {MC_Camera_EIA, "EIA", "EIA broadcasting standard"},
    {MC_Camera_PAL, "PAL", "PAL broadcasting standard"},
    {MC_Camera_NTSC, "NTSC", "NTSC broadcasting standard"},
    {0, NULL, NULL},
  };

  if (!euresys_camera_type) {
    euresys_camera_type =
      g_enum_register_static ("GstEuresysCamera", euresys_camera);
  }
  return euresys_camera_type;
}


/* class initialization */

#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_euresys_debug, "euresys", 0, \
      "debug category for euresys element");

GST_BOILERPLATE_FULL (GstEuresys, gst_euresys, GstPushSrc,
    GST_TYPE_PUSH_SRC, DEBUG_INIT);


static GstVideoFormat
gst_euresys_color_format_to_video_format (INT32 color_format)
{
  switch (color_format) {
    case MC_ColorFormat_Y8:
    case MC_ColorFormat_RAW8:
       return GST_VIDEO_FORMAT_GRAY8;
    /* TODO: possibly use different formats for each of the following */
    case MC_ColorFormat_Y10:
    case MC_ColorFormat_Y12:
    case MC_ColorFormat_Y14:
    case MC_ColorFormat_Y16:
    case MC_ColorFormat_RAW10:
    case MC_ColorFormat_RAW12:
    case MC_ColorFormat_RAW14:
    case MC_ColorFormat_RAW16:
      return GST_VIDEO_FORMAT_GRAY16_LE;
    case MC_ColorFormat_Y41P:
    case MC_ColorFormat_YUV411:
      return gst_video_format_from_fourcc (GST_MAKE_FOURCC ('Y', '4', '1', 'P'));
    case MC_ColorFormat_YUV422:
    case MC_ColorFormat_Y42P:
      return gst_video_format_from_fourcc (GST_MAKE_FOURCC ('Y', '4', '2', 'P'));
    case MC_ColorFormat_YUV444:
    case MC_ColorFormat_IYU2:
      return gst_video_format_from_fourcc (GST_MAKE_FOURCC ('I', 'Y', 'U', '2'));
    case MC_ColorFormat_YUV411PL:
    case MC_ColorFormat_Y41B:
      return GST_VIDEO_FORMAT_Y41B;
    case MC_ColorFormat_YUV422PL:
    case MC_ColorFormat_Y42B:
      return GST_VIDEO_FORMAT_Y42B;
    case MC_ColorFormat_YUV444PL:
      return GST_VIDEO_FORMAT_Y444;
    case MC_ColorFormat_YUV422PL_DEC:
    case MC_ColorFormat_I420:
    case MC_ColorFormat_IYUV:
    case MC_ColorFormat_YV12:
      return GST_VIDEO_FORMAT_I420;
    case MC_ColorFormat_YUV411PL_DEC:
    case MC_ColorFormat_YUV9:
    case MC_ColorFormat_YVU9:
      return gst_video_format_from_fourcc (GST_MAKE_FOURCC ('Y', 'V', 'U', '9'));
    case MC_ColorFormat_RGB15:
      return GST_VIDEO_FORMAT_RGB15;
    case MC_ColorFormat_RGB16:
      return GST_VIDEO_FORMAT_RGB16;
    case MC_ColorFormat_RGB24:
      return GST_VIDEO_FORMAT_RGB;
    case MC_ColorFormat_RGB32:
    case MC_ColorFormat_ARGB32:
      return GST_VIDEO_FORMAT_ARGB;
    default:
      return GST_VIDEO_FORMAT_UNKNOWN;
  }
};

static void
gst_euresys_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_euresys_src_template));

  gst_element_class_set_details_simple (element_class,
      "Euresys MultiCam Video Source", "Source/Video",
      "Euresys MultiCam framegrabber video source",
      "Joshua Doe <oss@nvl.army.mil>");
}

static void
gst_euresys_class_init (GstEuresysClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_euresys_set_property;
  gobject_class->get_property = gst_euresys_get_property;
  gobject_class->dispose = gst_euresys_dispose;
  gobject_class->finalize = gst_euresys_finalize;
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_euresys_get_caps);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_euresys_set_caps);
  base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_euresys_negotiate);
  base_src_class->newsegment = GST_DEBUG_FUNCPTR (gst_euresys_newsegment);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_euresys_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_euresys_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_euresys_get_times);
  base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_euresys_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_euresys_is_seekable);
  base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_euresys_unlock);
  base_src_class->event = GST_DEBUG_FUNCPTR (gst_euresys_event);
  base_src_class->do_seek = GST_DEBUG_FUNCPTR (gst_euresys_do_seek);
  base_src_class->query = GST_DEBUG_FUNCPTR (gst_euresys_query);
  base_src_class->check_get_range = GST_DEBUG_FUNCPTR (gst_euresys_check_get_range);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_euresys_fixate);
  base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_euresys_unlock_stop);
  base_src_class->prepare_seek_segment = GST_DEBUG_FUNCPTR (gst_euresys_prepare_seek_segment);

  push_src_class->create = GST_DEBUG_FUNCPTR (gst_euresys_create);


  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_BOARD_INDEX,
      g_param_spec_int ("board", "Board", "Index of board connected to camera",
          0, 15, DEFAULT_PROP_BOARD_INDEX, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_CAMERA_TYPE,
      g_param_spec_enum ("camera", "Camera",
          "Camera type", GST_TYPE_EURESYS_CAMERA,
          DEFAULT_PROP_CAMERA_TYPE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_CONNECTOR,
      g_param_spec_enum ("connector", "Connector",
          "Connector where camera is attached", GST_TYPE_EURESYS_CONNECTOR,
          DEFAULT_PROP_CONNECTOR, G_PARAM_READWRITE));

}

static void
gst_euresys_init (GstEuresys * euresys, GstEuresysClass * euresys_class)
{
  euresys->srcpad = gst_pad_new_from_static_template (&gst_euresys_src_template
      , "src");

  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (euresys), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (euresys), GST_FORMAT_TIME);

  /* initialize member variables */
  euresys->boardIdx = DEFAULT_PROP_BOARD_INDEX;
  euresys->cameraType = DEFAULT_PROP_CAMERA_TYPE;
  euresys->connector = DEFAULT_PROP_CONNECTOR;

  euresys->hChannel = 0;
  euresys->caps = NULL;

  euresys->acq_started = FALSE;

}

void
gst_euresys_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstEuresys *euresys;

  g_return_if_fail (GST_IS_EURESYS (object));
  euresys = GST_EURESYS (object);

  switch (property_id) {
    case PROP_BOARD_INDEX:
      euresys->boardIdx = g_value_get_int (value);
      break;
    case PROP_CAMERA_TYPE:
      euresys->cameraType = g_value_get_enum (value);
      break;
    case PROP_CONNECTOR:
      euresys->connector = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_euresys_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstEuresys *euresys;

  g_return_if_fail (GST_IS_EURESYS (object));
  euresys = GST_EURESYS (object);

  switch (property_id) {
    case PROP_BOARD_INDEX:
      g_value_set_int (value, euresys->boardIdx);
      break;
    case PROP_CAMERA_TYPE:
      g_value_set_enum (value, euresys->cameraType);
      break;
    case PROP_CONNECTOR:
      g_value_set_enum (value, euresys->connector);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_euresys_dispose (GObject * object)
{
  GstEuresys *euresys;

  g_return_if_fail (GST_IS_EURESYS (object));
  euresys = GST_EURESYS (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

void
gst_euresys_finalize (GObject * object)
{
  GstEuresys *euresys;

  g_return_if_fail (GST_IS_EURESYS (object));
  euresys = GST_EURESYS (object);

  /* clean up object here */

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GstCaps *
gst_euresys_get_caps (GstBaseSrc * src)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "get_caps");

  /* return template caps if we don't know the actual camera caps */
  if (!euresys->caps) {
    return gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (euresys)));
  }

  return gst_caps_copy (euresys->caps);

  return NULL;
}

static gboolean
gst_euresys_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstEuresys *euresys = GST_EURESYS (src);
  GstStructure *structure;
  gboolean ret;
  gint width, height;

  GST_DEBUG_OBJECT (euresys, "set_caps");

  if (euresys->caps) {
    gst_caps_unref (euresys->caps);
    euresys->caps = gst_caps_copy (caps);
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
gst_euresys_negotiate (GstBaseSrc * src)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "negotiate");

  return TRUE;
}

static gboolean
gst_euresys_newsegment (GstBaseSrc * src)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "newsegment");

  return TRUE;
}

static gboolean
gst_euresys_start (GstBaseSrc * src)
{
  GstEuresys *euresys = GST_EURESYS (src);
  MCSTATUS status = 0;
  INT32 colorFormat;
  GstVideoFormat videoFormat;
  int width, height;

  GST_DEBUG_OBJECT (euresys, "start");

  /* Open MultiCam driver */
  status = McOpenDriver (NULL);
  if (status != MC_OK) {
      GST_ELEMENT_ERROR (euresys, LIBRARY, INIT, (NULL), (NULL));
      return FALSE;
  }

  /* Only Windows supports error message boxes */
  /* McSetParamInt (MC_CONFIGURATION, MC_ErrorHandling, MC_ErrorHandling_MSGBOX); */

  /* Set error log file */
  /* McSetParamStr (MC_CONFIGURATION, MC_ErrorLog, "mc_error.log"); */
 
  /* Create a channel */
  status = McCreate(MC_CHANNEL, &euresys->hChannel);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, FAILED,
        (_("Failed to create channel.")), (NULL));
    return FALSE;
  }

  /* Link the channel to a board */
  status = McSetParamInt(euresys->hChannel, MC_DriverIndex, euresys->boardIdx);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (_("Failed to link channel to board.")), (NULL));
    McDelete (euresys->hChannel);
    euresys->hChannel = 0;
    return FALSE;
  }

  /* Select the video connector */
  status = McSetParamInt(euresys->hChannel, MC_Connector, euresys->connector);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (_("Failed to set connector to channel.")), (NULL));
    McDelete (euresys->hChannel);
    euresys->hChannel = 0;
    return FALSE;
  }

  /* Select the video signal type */
  status = McSetParamInt(euresys->hChannel, MC_Camera, euresys->cameraType);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (_("Failed to set camera type.")), (NULL));
    McDelete (euresys->hChannel);
    euresys->hChannel = 0;
    return FALSE;
  }

  /* Set the color format */
  /*status = McSetParamInt(euresys->hChannel, MC_ColorFormat, MC_ColorFormat_RGB32);
  if (status != MC_OK) goto Finalize; */

  /* The number of images to acquire */
  status = McSetParamInt (euresys->hChannel, MC_SeqLength_Fr, MC_INDETERMINATE);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (_("Failed to set sequence length to indeterminate value.")), (NULL));
    McDelete (euresys->hChannel);
    euresys->hChannel = 0;
    return FALSE;
  }

  /* TODO create caps */
  status = McGetParamInt (euresys->hChannel, MC_ColorFormat, &colorFormat);
  status |= McGetParamInt (euresys->hChannel, MC_ImageSizeX, &width);
  status |= McGetParamInt (euresys->hChannel, MC_ImageSizeX, &height);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
      (_("Failed to get color format, width, and height.")), (NULL));
    McDelete (euresys->hChannel);
    euresys->hChannel = 0;
    return FALSE;
  }

  videoFormat = gst_euresys_color_format_to_video_format (colorFormat);
  if (videoFormat == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ELEMENT_ERROR (euresys, STREAM, WRONG_TYPE,
        (_("Unknown or unsupported color format.")), (NULL));
    McDelete (euresys->hChannel);
    euresys->hChannel = 0;
    return FALSE;
  }

  if (euresys->caps)
    gst_caps_unref (euresys->caps);
  euresys->caps = gst_video_format_new_caps (videoFormat, width, height, 30, 1, 1, 1);

  if (euresys->caps == NULL) {
    GST_ELEMENT_ERROR (euresys, STREAM, TOO_LAZY,
      (_("Failed to generate caps from video format.")), (NULL));
    McDelete (euresys->hChannel);
    euresys->hChannel = 0;
    return FALSE;
  }
  
  return TRUE;
}

static gboolean
gst_euresys_stop (GstBaseSrc * src)
{
  GstEuresys *euresys = GST_EURESYS (src);
  MCSTATUS status = 0;

  GST_DEBUG_OBJECT (euresys, "stop");

  /* Stop the acquisition */
  McSetParamInt (euresys->hChannel, MC_ChannelState, MC_ChannelState_IDLE);

  /* Close the MultiCam driver */
  McCloseDriver();

  /* Delete the channel */
  if (euresys->hChannel)
    McDelete(euresys->hChannel);
  euresys->hChannel = 0;

  gst_caps_unref (euresys->caps);
  euresys->caps = NULL;

  return TRUE;
}

static void
gst_euresys_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "get_times");
}

static gboolean
gst_euresys_get_size (GstBaseSrc * src, guint64 * size)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "get_size");

  return TRUE;
}

static gboolean
gst_euresys_is_seekable (GstBaseSrc * src)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "is_seekable");

  return FALSE;
}

static gboolean
gst_euresys_unlock (GstBaseSrc * src)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "unlock");

  return TRUE;
}

static gboolean
gst_euresys_event (GstBaseSrc * src, GstEvent * event)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "event");

  return TRUE;
}

static gboolean
gst_euresys_do_seek (GstBaseSrc * src, GstSegment * segment)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "do_seek");

  return FALSE;
}

static gboolean
gst_euresys_query (GstBaseSrc * src, GstQuery * query)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "query");

  return TRUE;
}

static gboolean
gst_euresys_check_get_range (GstBaseSrc * src)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "get_range");

  return FALSE;
}

static void
gst_euresys_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "fixate");
}

static gboolean
gst_euresys_unlock_stop (GstBaseSrc * src)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "stop");

  return TRUE;
}

static gboolean
gst_euresys_prepare_seek_segment (GstBaseSrc * src, GstEvent * seek,
    GstSegment * segment)
{
  GstEuresys *euresys = GST_EURESYS (src);

  GST_DEBUG_OBJECT (euresys, "seek_segment");

  return FALSE;
}

static GstFlowReturn
gst_euresys_create (GstPushSrc * src, GstBuffer ** buf)
{
  GstEuresys *euresys = GST_EURESYS (src);
  MCSTATUS status = 0;
  MCSIGNALINFO siginfo;
  MCHANDLE hSurface;
  int *pImage;
  INT32 timeCode;
  INT64 timeStamp;
  int newsize;
  GstFlowReturn ret;

  /* Start acquisition */
  if (!euresys->acq_started) {
    status = McSetParamInt(euresys->hChannel, MC_ChannelState, MC_ChannelState_ACTIVE);
    if (status != MC_OK) {
      GST_ELEMENT_ERROR (euresys, RESOURCE, FAILED,
          (_("Failed to set channel state to ACTIVE.")), (NULL));
      return GST_FLOW_ERROR;
    }
    euresys->acq_started = TRUE;
  }

  /* Wait for next surface (frame) */
  while (TRUE) {
    /* Wait up to 5000 msecs for a signal */
    status = McWaitSignal (euresys->hChannel, MC_SIG_ANY, 5000, &siginfo);
    if (status == MC_TIMEOUT) {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          (_("Timeout waiting for signal.")), (NULL));
      return GST_FLOW_ERROR;
    }
    else if (siginfo.Signal == MC_SIG_ACQUISITION_FAILURE) {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          (_("Acquisition failure due to timeout.")), (NULL));
      return GST_FLOW_ERROR;
    }
    else if (siginfo.Signal == MC_SIG_SURFACE_PROCESSING) {
      break;
    }
    else {
      continue;
    }
  }

  /* Get pointer to image data and other info*/
  hSurface = (MCHANDLE) siginfo.SignalInfo;
  status = McGetParamInt (hSurface, MC_FillCount, &newsize);
  status |= McGetParamInt (hSurface, MC_TimeCode, &timeCode);
  status |= McGetParamInt64 (hSurface, MC_TimeStamp_us, &timeStamp);
  status |= McGetParamPtr (hSurface, MC_SurfaceAddr, (PVOID*)&pImage);
  if (G_UNLIKELY (status != MC_OK)) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, FAILED,
      (_("Failed to read surface parameter.")), (NULL));
    return GST_FLOW_ERROR;
  }

  /* Create the buffer */
  ret = gst_pad_alloc_buffer (GST_BASE_SRC_PAD (GST_BASE_SRC (src)),
      GST_BUFFER_OFFSET_NONE, newsize,
      GST_PAD_CAPS (GST_BASE_SRC_PAD (GST_BASE_SRC (src))), buf);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    return GST_FLOW_ERROR;
  }

  /* Copy image to buffer from surface */
  memcpy (GST_BUFFER_DATA (*buf), pImage, newsize);
  GST_BUFFER_SIZE (*buf) = newsize;
  /* TODO: set buffer timestamp based on MC_TimeStamp_us */
  GST_BUFFER_TIMESTAMP (*buf) =
      gst_clock_get_time (GST_ELEMENT_CLOCK (src)) -
      GST_ELEMENT_CAST (src)->base_time;

  /* Done processing surface, release control */
  McSetParamInt (euresys->hChannel, MC_SurfaceState, MC_SurfaceState_FREE);

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

  gst_element_register (plugin, "euresys", GST_RANK_NONE,
      gst_euresys_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "euresys",
    "Euresys Multicam source",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
