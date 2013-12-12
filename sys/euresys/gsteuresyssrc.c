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

/* TODO:
 * - add all caps supported by any Euresys framegrabber to src pad static caps
 *   - once specific framegrabber is known, set caps to available set
 *   - once caps are negotiated set to framegrabber
 *   - possibly use SurfaceColorFormat to determine the format of each surface
 * - apply surface timestamp to buffer
 * - issue warning and message if a frame has dropped
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>
#include "gsteuresyssrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_euresys_debug);
#define GST_CAT_DEFAULT gst_euresys_debug

/* prototypes */
static void gst_euresys_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_euresys_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_euresys_dispose (GObject * object);

static gboolean gst_euresys_start (GstBaseSrc * src);
static gboolean gst_euresys_stop (GstBaseSrc * src);
static GstCaps *gst_euresys_get_caps (GstBaseSrc * bsrc, GstCaps * filter);
static gboolean gst_euresys_set_caps (GstBaseSrc * bsrc, GstCaps * caps);

static GstFlowReturn gst_euresys_fill (GstPushSrc * src, GstBuffer * buf);

enum
{
  PROP_0,
  PROP_BOARD_INDEX,
  PROP_CAMERA_TYPE,
  PROP_CONNECTOR,
  PROP_COLOR_FORMAT
};

#define DEFAULT_PROP_BOARD_INDEX  0
#define DEFAULT_PROP_CAMERA_TYPE  GST_EURESYS_CAMERA_EIA
#define DEFAULT_PROP_CONNECTOR    GST_EURESYS_CONNECTOR_VID1
#define DEFAULT_PROP_COLOR_FORMAT GST_EURESYS_COLOR_FORMAT_Y8

/* pad templates */

static GstStaticPadTemplate gst_euresys_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, RGB, ARGB, RGB_16, RGB_16 }"))
    );

int gst_euresys_connector_map[] = {
  MC_Connector_VID1,
  MC_Connector_VID2,
  MC_Connector_VID3,
  MC_Connector_VID4
};

#define GST_TYPE_EURESYS_CONNECTOR (gst_euresys_connector_get_type())
static GType
gst_euresys_connector_get_type (void)
{
  static GType euresys_connector_type = 0;
  static const GEnumValue euresys_connector[] = {
    {GST_EURESYS_CONNECTOR_VID1, "VID1", "VID1 input"},
    {GST_EURESYS_CONNECTOR_VID2, "VID2", "VID2 input"},
    {GST_EURESYS_CONNECTOR_VID3, "VID3", "VID3 input"},
    {GST_EURESYS_CONNECTOR_VID4, "VID4", "VID4 input"},
    //{MC_Connector_VID5, "VID5", "VID5 input"},
    //{MC_Connector_VID6, "VID6", "VID6 input"},
    //{MC_Connector_VID7, "VID7", "VID7 input"},
    //{MC_Connector_VID8, "VID8", "VID8 input"},
    //{MC_Connector_VID9, "VID9", "VID9 input"},
    //{MC_Connector_VID10, "VID10", "VID10 input"},
    //{MC_Connector_VID11, "VID11", "VID11 input"},
    //{MC_Connector_VID12, "VID12", "VID12 input"},
    //{MC_Connector_VID13, "VID13", "VID13 input"},
    //{MC_Connector_VID14, "VID14", "VID14 input"},
    //{MC_Connector_VID15, "VID15", "VID15 input"},
    //{MC_Connector_VID16, "VID16", "VID16 input"},
    //{MC_Connector_YC, "YC", "YC input using the MiniDIN4 or DB9 connector"},
    //{MC_Connector_YC1, "YC1", "YC1 input using the HD44 connector"},
    //{MC_Connector_YC2, "YC2", "YC2 input using the HD44 connector"},
    //{MC_Connector_YC3, "YC3", "YC3 input using the HD44 connector"},
    //{MC_Connector_YC4, "YC4", "YC4 input using the HD44 connector"},
    //{MC_Connector_X, "X", "X input"},
    //{MC_Connector_Y, "Y", "Y input"},
    //{MC_Connector_XBIS, "XBIS", "XBIS input using the secondary lane"},
    //{MC_Connector_YBIS, "YBIS", "YBIS input using the secondary lane"},
    //{MC_Connector_X1, "X1", "X1 input"},
    //{MC_Connector_X2, "X2", "X2 input"},
    //{MC_Connector_Y1, "Y1", "Y1 input"},
    //{MC_Connector_Y2, "Y2", "Y2 input"},
    //{MC_Connector_A, "A",
    //    "A input (Grablink Expert 2 DuoCam mode, connector A)"},
    //{MC_Connector_B, "B",
    //    "B input (Grablink Expert 2 DuoCam mode, connector B)"},
    //{MC_Connector_M, "M", "M input (Grablink in MonoCam mode)"},
    {0, NULL, NULL},
  };

  if (!euresys_connector_type) {
    euresys_connector_type =
        g_enum_register_static ("GstEuresysConnector", euresys_connector);
  }
  return euresys_connector_type;
}

int gst_euresys_color_format_map[] = {
  MC_ColorFormat_Y8,
  MC_ColorFormat_RGB24,
  MC_ColorFormat_RGB32,
  MC_ColorFormat_ARGB32
};

#define GST_TYPE_EURESYS_COLOR_FORMAT (gst_euresys_color_format_get_type())
static GType
gst_euresys_color_format_get_type (void)
{
  static GType euresys_color_format_type = 0;
  static const GEnumValue euresys_color_format[] = {
    {GST_EURESYS_COLOR_FORMAT_Y8, "Y8", "Monochrome 8-bit"},
    {GST_EURESYS_COLOR_FORMAT_RGB24, "RGB24", "RGB24"},
    {GST_EURESYS_COLOR_FORMAT_RGB32, "RGB32", "RGB32"},
    {GST_EURESYS_COLOR_FORMAT_ARGB32, "ARGB32", "ARGB32"},
    {0, NULL, NULL},
  };

  if (!euresys_color_format_type) {
    euresys_color_format_type =
        g_enum_register_static ("GstEuresysColorFormat", euresys_color_format);
  }
  return euresys_color_format_type;
}

int gst_euresys_camera_map[] = {
  MC_Camera_CAMERA_EIA,
  MC_Camera_CAMERA_NTSC,
  MC_Camera_CAMERA_CCIR,
  MC_Camera_CAMERA_PAL
};

#define GST_TYPE_EURESYS_CAMERA (gst_euresys_camera_get_type())
static GType
gst_euresys_camera_get_type (void)
{
  static GType euresys_camera_type = 0;
  static const GEnumValue euresys_camera[] = {
    {GST_EURESYS_CAMERA_EIA, "EIA", "EIA broadcasting standard"},
    {GST_EURESYS_CAMERA_NTSC, "NTSC", "NTSC broadcasting standard"},
    {GST_EURESYS_CAMERA_CCIR, "CCIR", "CCIR broadcasting standard"},
    {GST_EURESYS_CAMERA_PAL, "PAL", "PAL broadcasting standard"},
    {0, NULL, NULL},
  };

  if (!euresys_camera_type) {
    euresys_camera_type =
        g_enum_register_static ("GstEuresysCamera", euresys_camera);
  }
  return euresys_camera_type;
}

/* class initialization */
G_DEFINE_TYPE (GstEuresys, gst_euresys, GST_TYPE_PUSH_SRC);

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
      return gst_video_format_from_fourcc (GST_MAKE_FOURCC ('Y', '4', '1',
              'P'));
    case MC_ColorFormat_YUV422:
    case MC_ColorFormat_Y42P:
      return gst_video_format_from_fourcc (GST_MAKE_FOURCC ('Y', '4', '2',
              'P'));
    case MC_ColorFormat_YUV444:
    case MC_ColorFormat_IYU2:
      return gst_video_format_from_fourcc (GST_MAKE_FOURCC ('I', 'Y', 'U',
              '2'));
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
      return gst_video_format_from_fourcc (GST_MAKE_FOURCC ('Y', 'V', 'U',
              '9'));
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
gst_euresys_class_init (GstEuresysClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_euresys_set_property;
  gobject_class->get_property = gst_euresys_get_property;
  gobject_class->dispose = gst_euresys_dispose;

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_BOARD_INDEX,
      g_param_spec_int ("board", "Board", "Index of board connected to camera",
          0, 15, DEFAULT_PROP_BOARD_INDEX,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_CAMERA_TYPE,
      g_param_spec_enum ("camera", "Camera", "Camera type",
          GST_TYPE_EURESYS_CAMERA, DEFAULT_PROP_CAMERA_TYPE,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_CONNECTOR,
      g_param_spec_enum ("connector", "Connector",
          "Connector where camera is attached", GST_TYPE_EURESYS_CONNECTOR,
          DEFAULT_PROP_CONNECTOR,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_COLOR_FORMAT,
      g_param_spec_enum ("color-format", "Color format",
          "Color format of the camera", GST_TYPE_EURESYS_COLOR_FORMAT,
          DEFAULT_PROP_COLOR_FORMAT,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_euresys_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Euresys MultiCam Video Source", "Source/Video",
      "Euresys MultiCam framegrabber video source",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_euresys_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_euresys_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_euresys_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_euresys_set_caps);

  gstpushsrc_class->fill = GST_DEBUG_FUNCPTR (gst_euresys_fill);
}

static void
gst_euresys_init (GstEuresys * euresys)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (euresys), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (euresys), GST_FORMAT_TIME);

  /* initialize member variables */
  euresys->boardIdx = DEFAULT_PROP_BOARD_INDEX;
  euresys->cameraType = DEFAULT_PROP_CAMERA_TYPE;
  euresys->connector = DEFAULT_PROP_CONNECTOR;
  euresys->colorFormat = DEFAULT_PROP_COLOR_FORMAT;

  euresys->hChannel = 0;

  euresys->acq_started = FALSE;

  euresys->last_time_code = -1;
  euresys->dropped_frame_count = 0;
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
    case PROP_COLOR_FORMAT:
      euresys->colorFormat = g_value_get_enum (value);
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
    case PROP_COLOR_FORMAT:
      g_value_set_enum (value, euresys->colorFormat);
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

  G_OBJECT_CLASS (gst_euresys_parent_class)->dispose (object);
}

static gboolean
gst_euresys_start (GstBaseSrc * bsrc)
{
  GstEuresys *euresys = GST_EURESYS (bsrc);
  MCSTATUS status = 0;

  GST_DEBUG_OBJECT (euresys, "start");

  /* Open MultiCam driver */
  status = McOpenDriver (NULL);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, LIBRARY, INIT, (NULL), (NULL));
    return FALSE;
  }

  status =
      McGetParamInt (MC_BOARD + euresys->boardIdx, MC_BoardType,
      &euresys->boardType);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (("Failed to get board type.")), (NULL));
    return FALSE;
  }

  /* Only Windows supports error message boxes */
  /* McSetParamInt (MC_CONFIGURATION, MC_ErrorHandling, MC_ErrorHandling_MSGBOX); */

  /* Set error log file */
  /* McSetParamStr (MC_CONFIGURATION, MC_ErrorLog, "mc_error.log"); */

  /* Create a channel */
  status = McCreate (MC_CHANNEL, &euresys->hChannel);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, FAILED,
        (("Failed to create channel.")), (NULL));
    return FALSE;
  }

  /* Link the channel to a board */
  status = McSetParamInt (euresys->hChannel, MC_DriverIndex, euresys->boardIdx);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (("Failed to link channel to board.")), (NULL));
    goto error;
  }

  /* Select the video connector */
  status =
      McSetParamInt (euresys->hChannel, MC_Connector,
      gst_euresys_connector_map[euresys->connector]);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (("Failed to set connector to channel.")), (NULL));
    goto error;
  }

  /* Select the video signal type */
  status =
      McSetParamInt (euresys->hChannel, MC_Camera,
      gst_euresys_camera_map[euresys->cameraType]);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (("Failed to set camera type = %d."), euresys->cameraType), (NULL));
    goto error;
  }

  /* Set the color format */
  status =
      McSetParamInt (euresys->hChannel, MC_ColorFormat,
      gst_euresys_color_format_map[euresys->colorFormat]);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (("Failed to set color format = %d."), MC_ColorFormat_Y8), (NULL));
    goto error;
  }

  /* Acquire images continuously */
  status = McSetParamInt (euresys->hChannel, MC_SeqLength_Fr, MC_INDETERMINATE);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (("Failed to set sequence length to indeterminate value.")), (NULL));
    goto error;
  }

  /* Enable signals */
  status =
      McSetParamInt (euresys->hChannel,
      MC_SignalEnable + MC_SIG_SURFACE_PROCESSING, MC_SignalEnable_ON);
  status |=
      McSetParamInt (euresys->hChannel,
      MC_SignalEnable + MC_SIG_ACQUISITION_FAILURE, MC_SignalEnable_ON);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, SETTINGS,
        (("Failed to enable signals.")), (NULL));
    goto error;
  }

  return TRUE;

error:
  if (euresys->hChannel) {
    McDelete (euresys->hChannel);
    euresys->hChannel = 0;
  }
  return FALSE;
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
  McCloseDriver ();

  /* Delete the channel */
  if (euresys->hChannel)
    McDelete (euresys->hChannel);
  euresys->hChannel = 0;

  euresys->dropped_frame_count = 0;
  euresys->last_time_code = -1;

  return TRUE;
}

static GstCaps *
gst_euresys_get_camera_caps (GstEuresys * src)
{
  INT32 colorFormat;
  GstVideoFormat videoFormat;
  GstCaps *caps;
  GstVideoInfo vinfo;
  gint32 width, height;
  int status;

  g_assert (src->hChannel != 0);

  status = McGetParamInt (src->hChannel, MC_ColorFormat, &colorFormat);
  status |= McGetParamInt (src->hChannel, MC_ImageSizeX, &width);
  status |= McGetParamInt (src->hChannel, MC_ImageSizeY, &height);
  if (status != MC_OK) {
    GST_ELEMENT_ERROR (src, RESOURCE, SETTINGS,
        (("Failed to get color format, width, and/or height.")), (NULL));
    return NULL;
  }

  videoFormat = gst_euresys_color_format_to_video_format (colorFormat);
  if (videoFormat == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
        (("Unknown or unsupported color format.")), (NULL));
    return NULL;
  }

  gst_video_info_init (&vinfo);

  vinfo.width = width;
  vinfo.height = height;
  vinfo.finfo = gst_video_format_get_info (videoFormat);

  caps = gst_video_info_to_caps (&vinfo);

  if (caps == NULL) {
    GST_ELEMENT_ERROR (src, STREAM, TOO_LAZY,
        (("Failed to generate caps from video format.")), (NULL));
    return NULL;
  }

  return caps;
}

static GstCaps *
gst_euresys_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstEuresys *src = GST_EURESYS (bsrc);
  GstCaps *caps;

  if (src->hChannel == 0)
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  else
    caps = gst_euresys_get_camera_caps (src);

  if (filter && caps) {
    GstCaps *tmp = gst_caps_intersect (caps, filter);
    gst_caps_unref (caps);
    caps = tmp;
  }

  return caps;
}

static gboolean
gst_euresys_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstVideoInfo vinfo;

  GST_DEBUG_OBJECT (bsrc, "set_caps with caps=%" GST_PTR_FORMAT, caps);

  gst_video_info_from_caps (&vinfo, caps);

  /* TODO: check stride alignment */
  gst_base_src_set_blocksize (bsrc, GST_VIDEO_INFO_SIZE (&vinfo));

  return TRUE;
}

GstFlowReturn
gst_euresys_fill (GstPushSrc * src, GstBuffer * buf)
{
  GstEuresys *euresys = GST_EURESYS (src);
  MCSTATUS status = 0;
  MCSIGNALINFO siginfo;
  MCHANDLE hSurface;
  int *pImage;
  INT32 timeCode;
  INT64 timeStamp;
  int newsize;
  int dropped_frame_count;
  GstMapInfo minfo;

  /* Start acquisition */
  if (!euresys->acq_started) {
    status =
        McSetParamInt (euresys->hChannel, MC_ChannelState,
        MC_ChannelState_ACTIVE);
    if (status != MC_OK) {
      GST_ELEMENT_ERROR (euresys, RESOURCE, FAILED,
          (("Failed to set channel state to ACTIVE.")), (NULL));
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
          (("Timeout waiting for signal.")), (("Timeout waiting for signal.")));
      return GST_FLOW_ERROR;
    } else if (siginfo.Signal == MC_SIG_ACQUISITION_FAILURE) {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          (("Acquisition failure due to timeout.")), (NULL));
      return GST_FLOW_ERROR;
    } else if (siginfo.Signal == MC_SIG_SURFACE_PROCESSING) {
      break;
    } else {
      continue;
    }
  }

  /* Get pointer to image data and other info */
  hSurface = (MCHANDLE) siginfo.SignalInfo;
  /* "number of bytes actually written into the surface" */
  status = McGetParamInt (hSurface, MC_FillCount, &newsize);
  /* "Internal numbering of surface during acquisition sequence" (zero-based) */
  status |= McGetParamInt (hSurface, MC_TimeCode, &timeCode);
  /* "number of microseconds elapsed since midnight (00:00:00), 
   * January 1, 1970, coordinated universal time (UTC), according
   * to the system clock when the surface is filled" */
  status |= McGetParamInt64 (hSurface, MC_TimeStamp_us, &timeStamp);
  status |= McGetParamPtr (hSurface, MC_SurfaceAddr, (PVOID *) & pImage);
  if (G_UNLIKELY (status != MC_OK)) {
    GST_ELEMENT_ERROR (euresys, RESOURCE, FAILED,
        (("Failed to read surface parameter.")), (NULL));
    return GST_FLOW_ERROR;
  }

  GST_INFO ("Got surface #%05d", timeCode);

  dropped_frame_count = timeCode - (euresys->last_time_code + 1);
  if (dropped_frame_count != 0) {
    euresys->dropped_frame_count += dropped_frame_count;
    GST_WARNING ("Dropped %d frames (%d total)", dropped_frame_count,
        euresys->dropped_frame_count);
    /* TODO: emit message here about dropped frames */
  }
  euresys->last_time_code = timeCode;

  /* Copy image to buffer from surface */
  gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
  /* TODO: fix strides? */
  g_assert (minfo.size == newsize);
  memcpy (minfo.data, pImage, newsize);
  gst_buffer_unmap (buf, &minfo);

  /* TODO: set buffer timestamp based on MC_TimeStamp_us */
  GST_BUFFER_TIMESTAMP (buf) =
      gst_clock_get_time (GST_ELEMENT_CLOCK (src)) -
      GST_ELEMENT_CAST (src)->base_time;

  /* Done processing surface, release control */
  McSetParamInt (hSurface, MC_SurfaceState, MC_SurfaceState_FREE);

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_euresys_debug, "euresys", 0,
      "debug category for euresys element");
  gst_element_register (plugin, "euresyssrc", GST_RANK_NONE,
      gst_euresys_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    euresys,
    "Euresys Multicam source",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
