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
 * SECTION:element-gstpixcisrc
 *
 * The pixcisrc element is a source for EPIX PIXCI framegrabbers supported by EPIX XCLIB.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v pixcisrc ! ffmpegcolorspace ! autovideosink
 * ]|
 * Shows video from the default Pixci framegrabber
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#include "gstpixcisrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_pixcisrc_debug);
#define GST_CAT_DEFAULT gst_pixcisrc_debug

/* prototypes */
static void gst_pixcisrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_pixcisrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_pixcisrc_dispose (GObject * object);
static void gst_pixcisrc_finalize (GObject * object);

static gboolean gst_pixcisrc_start (GstBaseSrc * src);
static gboolean gst_pixcisrc_stop (GstBaseSrc * src);
static GstCaps *gst_pixcisrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_pixcisrc_set_caps (GstBaseSrc * src, GstCaps * caps);

static GstFlowReturn gst_pixcisrc_create (GstPushSrc * src, GstBuffer ** buf);

static GstCaps *gst_pixcisrc_create_caps (GstPixciSrc * src);
enum
{
  PROP_0,
  PROP_FORMAT_NAME,
  PROP_FORMAT_FILE,
  PROP_DRIVER_PARAMS,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_BOARD,
  PROP_CHANNEL,
  PROP_TIMEOUT
};

#define DEFAULT_PROP_FORMAT_NAME GST_PIXCI_VIDEO_FORMAT_RS_170
#define DEFAULT_PROP_FORMAT_FILE ""
#define DEFAULT_PROP_DRIVER_PARAMS ""
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 2
#define DEFAULT_PROP_BOARD 0
#define DEFAULT_PROP_CHANNEL 0
#define DEFAULT_PROP_TIMEOUT 0

/* pad templates */

static GstStaticPadTemplate gst_pixcisrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ GRAY8, GRAY16_LE, GRAY16_BE }"))
    );


#define GST_TYPE_PIXCI_VIDEO_FORMAT (gst_pixci_video_format_get_type())
static GType
gst_pixci_video_format_get_type (void)
{
  static GType pixci_video_format_type = 0;
  static const GEnumValue pixci_video_format[] = {
    {GST_PIXCI_VIDEO_FORMAT_CCIR, "CCIR",
        "CCIR composite video, capturing 720 pixels per line, 574 lines, monochrome grey level pixels."},
    {GST_PIXCI_VIDEO_FORMAT_CCIR_SQR, "CCIR(SQR)",
        "CCIR composite video, capturing 768 pixels per line, 576 lines, monochrome grey level pixels, yielding pixels with a 1:1 aspect ratio."},
    {GST_PIXCI_VIDEO_FORMAT_NTSC, "NTSC",
        "NTSC composite video, capturing 720 pixels per line, 480 lines, color YCrCb pixels."},
    {GST_PIXCI_VIDEO_FORMAT_NTSC_4_43, "NTSC(4.43)",
        "NTSC(4.43) composite video, capturing 720 pixels per line, 480 lines, color YCrCb pixels."},
    {GST_PIXCI_VIDEO_FORMAT_NTSC_J, "NTSC(J)",
        "NTSC(J) composite video, capturing 720 pixels per line, 480 lines, color YCrCb pixels."},
    {GST_PIXCI_VIDEO_FORMAT_NTSC_SQR, "NTSC(SQR)",
        "NTSC composite video, capturing 640 pixels per line, 480 lines, color YCrCb pixels using the High Density BNC connector closest to the S-Video connector, yielding pixels with a 1:1 aspect ratio."},
    {GST_PIXCI_VIDEO_FORMAT_NTSC_YC, "NTSC/YC",
        "NTSC S-Video, capturing 720 pixels per line, 480 lines, color YCrCb pixels."},
    {GST_PIXCI_VIDEO_FORMAT_NTSC_YC_SQR, "NTSC/YC(SQR)",
        "NTSC S-Video, capturing 640 pixels per line, 480 lines, color YCrCb pixels using the S-Video connector, yielding pixels with a 1:1 aspect ratio."},
    {GST_PIXCI_VIDEO_FORMAT_PAL, "PAL",
        "PAL (B,D,G,H,I) composite video, capturing 720 pixels per line, 576 lines, color YCrCb pixels."},
    {GST_PIXCI_VIDEO_FORMAT_PAL_60, "PAL(60)",
        "PAL(60) composite video, capturing 720 pixels per line, 576 lines, color YCrCb pixels."},
    {GST_PIXCI_VIDEO_FORMAT_PAL_M, "PAL(M)",
        "PAL(M) composite video, capturing 920 pixels per line, 576 lines, color YCrCb or color RGB."},
    {GST_PIXCI_VIDEO_FORMAT_PAL_M_YC, "PAL(M)/YC",
        "PAL(M) S-VIDEO (Super VHS), capturing 920 pixels per line, 576 lines, color YCrCb or color RGB."},
    {GST_PIXCI_VIDEO_FORMAT_PAL_N, "PAL(N)",
        "PAL(N) composite video, capturing 920 pixels per line, 576 lines, color YCrCb or color RGB."},
    {GST_PIXCI_VIDEO_FORMAT_PAL_N_YC, "PAL(N)/YC",
        "PAL(N) S-VIDEO (Super VHS), capturing 920 pixels per line, 576 lines, color YCrCb or color RGB."},
    {GST_PIXCI_VIDEO_FORMAT_PAL_SQR, "PAL(SQR)",
        "PAL (B,D,G,H,I) composite video, capturing 768 pixels per line, 576 lines, color YCrCb pixels, yielding pixels with a 1:1 aspect ratio."},
    {GST_PIXCI_VIDEO_FORMAT_PAL_YC, "PAL/YC",
        "PAL (B,D,G,H,I) S-Video, capturing 720 pixels per line, 576 lines, color YCrCb pixels."},
    {GST_PIXCI_VIDEO_FORMAT_PAL_YC_SQR, "PAL/YC(SQR)",
        "PAL (B,D,G,H,I) S-Video, capturing 768 pixels per line, 576 lines, color YCrCb pixels using the S-Video connector, yielding pixels with a 1:1 aspect ratio."},
    {GST_PIXCI_VIDEO_FORMAT_RS_170, "RS-170",
        "RS-170 composite video, capturing 720 pixels per line, 480 lines, monochrome grey level pixels."},
    {GST_PIXCI_VIDEO_FORMAT_RS_170_SQR, "RS-170(SQR)",
        "RS-170 composite video, capturing 640 pixels per line, 480 lines, monochrome grey level pixels using the High Density BNC connector closest to the S-Video connector, yielding pixels with a 1:1 aspect ratio."},
    {GST_PIXCI_VIDEO_FORMAT_RS343_875i_60Hz, "RS343 875i 60Hz",
        "Composite interlaced RS-343 video, 875 total lines per frame, 60 Hz, capturing 1332 pixels per line, 806 lines, monochrome grey level pixels."},
    {GST_PIXCI_VIDEO_FORMAT_RS343_875i_60Hz_RGB, "RS343 875i 60Hz RGB",
        "Interlaced RS-343 RGB component video, composite sync on green, 875 total lines per frame, 60 Hz, capturing 1332 pixels per line, 806 lines, RGB color pixels."},
    {GST_PIXCI_VIDEO_FORMAT_SECAM, "SECAM",
        "SECAM composite video, capturing 920 pixels per line, 576 lines, color YCrCb or color RGB."},
    {GST_PIXCI_VIDEO_FORMAT_SECAM_YC, "SECAM/YC",
        "SECAM S-VIDEO (Super VHS), capturing 920 pixels per line, 576 lines, color YCrCb or color RGB."},
    {GST_PIXCI_VIDEO_FORMAT_SVGA_800x600_60Hz_RGB, "SVGA 800x600 60Hz RGB",
        "Progressive RGB+HSYNC+VSYNC component SVGA video, 60.371 Hz, capturing 800 pixels per line, 600 lines, RGB color pixels."},
    {GST_PIXCI_VIDEO_FORMAT_SXGA_1280x1024_60Hz_RGB, "SXGA 1280x1024 60Hz RGB",
        "Progressive RGB+HSYNC+VSYNC component SXGA video, 60.02 Hz, capturing 1280 pixels per line, 1024 lines, RGB color pixels."},
    {GST_PIXCI_VIDEO_FORMAT_VGA_640x480_60Hz_RGB, "VGA 640x480 60Hz RGB",
        "Progressive RGB+HSYNC+VSYNC component VGA video, 59.94 Hz, capturing 640 pixels per line, 480 lines, RGB color pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_1280x720p_50Hz, "Video 1280x720p 50Hz",
        "Composite progressive video, 50 Hz, capturing 1280 pixels per line, 720 lines, monochrome grey level pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_1280x720p_50Hz_RGB,
          "Video 1280x720p 50Hz RGB",
        "Progressive RGB component video, composite sync on green, 50 Hz, capturing 1280 pixels per line, 720 lines, RGB color pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_1280x720p_60Hz, "Video 1280x720p 60Hz",
        "Composite progressive video, 60 Hz, capturing 1280 pixels per line, 720 lines, monochrome grey level pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_1280x720p_60Hz_RGB,
          "Video 1280x720p 60Hz RGB",
        "Progressive RGB component video, composite sync on green, 60 Hz, capturing 1280 pixels per line, 720 lines, RGB color pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_1920x1080i_50Hz, "Video 1920x1080i 50Hz",
        "Composite interlaced video, 50 Hz, capturing 1920 pixels per line, 1080 lines, monochrome grey level pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_1920x1080i_50Hz_RGB,
          "Video 1920x1080i 50Hz RGB",
        "Interlaced RGB component video, composite sync on green, 50 Hz, capturing 1920 pixels per line, 1080 lines, RGB color pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_1920x1080i_60Hz, "Video 1920x1080i 60Hz",
        "Composite interlaced video, 60 Hz, capturing 1920 pixels per line, 1080 lines, monochrome grey level pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_1920x1080i_60Hz_RGB,
          "Video 1920x1080i 60Hz RGB",
        "Interlaced RGB component video, composite sync on green, 60 Hz, capturing 1920 pixels per line, 1080 lines, RGB color pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_720x480i_60Hz, "Video 720x480i 60Hz",
        "Composite interlaced video, 525 total lines per frame, 60 Hz, capturing 720 pixels per line, 480 lines, monochrome grey level pixels. Same as 'RS-170'."},
    {GST_PIXCI_VIDEO_FORMAT_Video_720x480i_60Hz_Color,
          "Video 720x480i 60Hz Color",
        "Composite video, capturing 720 pixels per line, 480 lines, color YCrCb pixels. Same as 'NTSC'."},
    {GST_PIXCI_VIDEO_FORMAT_Video_720x480i_60Hz_RGB, "Video 720x480i 60Hz RGB",
        "Interlaced RGB component video, composite sync on green, 525 total lines per frame, 60 Hz, capturing 720 pixels per line, 480 lines, RGB color pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_720x480i_60Hz_Y_Color,
          "Video 720x480i 60Hz Y/Color",
        "NTSC S-Video, capturing 720 pixels per line, 480 lines, color YCrCb pixels. Same as 'NTSC/YC'."},
    {GST_PIXCI_VIDEO_FORMAT_Video_720x576i_50Hz, "Video 720x576i 50Hz",
        "Composite interlaced video, 625 total lines per frame, 50 Hz, capturing 720 pixels per line, 576 lines, monochrome grey level pixels. Same as 'CCIR'."},
    {GST_PIXCI_VIDEO_FORMAT_Video_720x576i_50Hz_Color,
          "Video 720x576i 50Hz Color",
        "Composite video, capturing 720 pixels per line, 576 lines, color YCrCb pixels. Same as 'PAL'."},
    {GST_PIXCI_VIDEO_FORMAT_Video_720x576i_50Hz_RGB, "Video 720x576i 50Hz RGB",
        "Interlaced RGB component video, composite sync on green, 625 total lines per frame, 50 Hz, capturing 720 pixels per line, 576 lines, RGB color pixels."},
    {GST_PIXCI_VIDEO_FORMAT_Video_720x576i_50Hz_Y_Color,
          "Video 720x576i 50Hz Y/Color",
        "PAL (B,D,G,H,I) S-Video, capturing 720 pixels per line, 576 lines, color YCrCb pixels. Same as 'PAL/YC'."},
    {GST_PIXCI_VIDEO_FORMAT_XGA_1024x768_60Hz_RGB, "XGA 1024x768 60Hz RGB",
        "Progressive RGB+HSYNC+VSYNC component XGA video, 60 Hz, capturing 1024 pixels per line, 768 lines, RGB color pixels."},
    {0, NULL, NULL},
  };

  if (!pixci_video_format_type) {
    pixci_video_format_type =
        g_enum_register_static ("GstPixciVideoFormat", pixci_video_format);
  }
  return pixci_video_format_type;
}


/* class initialization */

G_DEFINE_TYPE (GstPixciSrc, gst_pixcisrc, GST_TYPE_PUSH_SRC);

static void
gst_pixcisrc_class_init (GstPixciSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_pixcisrc_set_property;
  gobject_class->get_property = gst_pixcisrc_get_property;
  gobject_class->dispose = gst_pixcisrc_dispose;
  gobject_class->finalize = gst_pixcisrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_pixcisrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "EPIX PIXCI Video Source", "Source/Video",
      "EPIX PIXCI framegrabber video source",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_pixcisrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_pixcisrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_pixcisrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_pixcisrc_set_caps);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_pixcisrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_FORMAT_NAME,
      g_param_spec_enum ("format-name", "Format name",
          "Video format of the camera. If set, format-file will override this.",
          GST_TYPE_PIXCI_VIDEO_FORMAT, DEFAULT_PROP_FORMAT_NAME,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_READY));
  g_object_class_install_property (gobject_class, PROP_FORMAT_FILE,
      g_param_spec_string ("format-file", "Format file",
          "Filepath of the video file for the selected camera "
          "(if set this will override format-name)", DEFAULT_PROP_FORMAT_FILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_DRIVER_PARAMS,
      g_param_spec_string ("driver-params", "Driver parameters",
          "Driver parameters to use when initializing XCLIB",
          DEFAULT_PROP_DRIVER_PARAMS,
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
      g_param_spec_uint ("channel", "Channel", "Channel number (0 for auto)", 0,
          2, DEFAULT_PROP_CHANNEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_uint ("timeout", "Timeout",
          "Timeout in milliseconds (0 for default)", 0, G_MAXUINT,
          DEFAULT_PROP_TIMEOUT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_pixcisrc_init (GstPixciSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->format_name = DEFAULT_PROP_FORMAT_NAME;
  src->format_file = g_strdup (DEFAULT_PROP_FORMAT_FILE);
  src->driver_params = g_strdup (DEFAULT_PROP_DRIVER_PARAMS);
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;

  /* this selects the first unit, make this a property? */
  src->unitmap = 1;
  src->pixci_open = FALSE;

  src->first_pixci_ts = GST_CLOCK_TIME_NONE;
  src->frame_start_times = g_new (guint64, src->num_capture_buffers);
  src->frame_end_times = g_new (guint64, src->num_capture_buffers);
  src->buffer_ready = FALSE;
  src->timeout_occurred = FALSE;
  src->fifo_overflow_occurred = FALSE;

  src->buffer_ready_count = 0;
  src->buffer_processed_count = 0;
  src->frame_end_count = 0;
  src->frame_start_count = 0;
  /*pixcisrc->frame_count = 0; */

  g_mutex_init (&src->mutex);
  g_cond_init (&src->cond);
}

void
gst_pixcisrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPixciSrc *src;

  src = GST_PIXCI_SRC (object);

  switch (property_id) {
    case PROP_FORMAT_NAME:
      src->format_name = g_value_get_enum (value);
      break;
    case PROP_FORMAT_FILE:
      g_free (src->format_file);
      src->format_file = g_strdup (g_value_get_string (value));
      break;
    case PROP_DRIVER_PARAMS:
      g_free (src->driver_params);
      src->driver_params = g_strdup (g_value_get_string (value));
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      if (src->acq_started) {
        GST_ELEMENT_WARNING (src, RESOURCE, SETTINGS,
            ("Number of capture buffers cannot be changed after acquisition has started."),
            (NULL));
      } else {
        src->num_capture_buffers = g_value_get_uint (value);

        g_free (src->frame_start_times);
        src->frame_start_times = g_new (guint64, src->num_capture_buffers);

        g_free (src->frame_end_times);
        src->frame_end_times = g_new (guint64, src->num_capture_buffers);
      }
      break;
    case PROP_BOARD:
      src->board = g_value_get_uint (value);
      break;
    case PROP_CHANNEL:
      src->channel = g_value_get_uint (value);
      break;
    case PROP_TIMEOUT:
      src->timeout = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_pixcisrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstPixciSrc *src;

  g_return_if_fail (GST_IS_PIXCI_SRC (object));
  src = GST_PIXCI_SRC (object);

  switch (property_id) {
    case PROP_FORMAT_NAME:
      g_value_set_enum (value, src->format_name);
      break;
    case PROP_FORMAT_FILE:
      g_value_set_string (value, src->format_file);
      break;
    case PROP_DRIVER_PARAMS:
      g_value_set_string (value, src->driver_params);
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
      g_value_set_uint (value, src->timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_pixcisrc_dispose (GObject * object)
{
  GstPixciSrc *src;

  g_return_if_fail (GST_IS_PIXCI_SRC (object));
  src = GST_PIXCI_SRC (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_pixcisrc_parent_class)->dispose (object);
}

void
gst_pixcisrc_finalize (GObject * object)
{
  GstPixciSrc *src;

  g_return_if_fail (GST_IS_PIXCI_SRC (object));
  src = GST_PIXCI_SRC (object);

  /* clean up object here */
  g_free (src->format_file);
  g_free (src->driver_params);

  g_free (src->frame_start_times);
  g_free (src->frame_end_times);

  G_OBJECT_CLASS (gst_pixcisrc_parent_class)->finalize (object);
}

//static inline GstClockTime
//gst_pixci_get_timestamp (GstPixciSrc * pixcisrc)
//{
//  ui32 dwParam;
//  guint64 timestamp;
//
//  /* get time in microseconds from start of acquisition */
//  /* TODO: check for rollover */
//  PHX_ParameterGet (pixcisrc->hCamera, PHX_EVENTCOUNT, &dwParam);
//  timestamp = (guint64) 1000 *dwParam;
//
//  if (pixcisrc->first_pixci_ts == GST_CLOCK_TIME_NONE) {
//    pixcisrc->first_pixci_ts = timestamp;
//  }
//  return timestamp - pixcisrc->first_pixci_ts;
//}

/* Callback function to handle image capture events. */
//void
//phx_callback (tHandle hCamera, ui32 dwMask, void *pvParams)
//{
//  GstPixciSrc *pixcisrc = GST_PIXCI_SRC (pvParams);
//  GstClockTime ct = gst_pixci_get_timestamp (pixcisrc);
//  gboolean signal_create_func = FALSE;
//  guint n;
//
//  g_mutex_lock (&pixcisrc->mutex);
//
//  /* Note that more than one interrupt can be sent, so no "else if" */
//
//  /* called when frame valid signal goes high */
//  if (PHX_INTRPT_FRAME_START & dwMask) {
//    /* FIXME: this will work until frames are dropped */
//    n = pixcisrc->frame_start_count % pixcisrc->num_capture_buffers;
//    pixcisrc->frame_start_times[n] = ct;
//
//    pixcisrc->frame_start_count++;
//  }
//
//  /* called when frame valid signal goes low */
//  if (PHX_INTRPT_FRAME_END & dwMask) {
//    /* FIXME: this will work until frames are dropped */
//    n = (pixcisrc->frame_end_count - 1) % pixcisrc->num_capture_buffers;
//    pixcisrc->frame_end_times[n] = ct;
//
//    pixcisrc->frame_end_count++;
//  }
//
//  if (PHX_INTRPT_BUFFER_READY & dwMask) {
//    /* we have a buffer */
//    pixcisrc->buffer_ready = TRUE;
//    pixcisrc->buffer_ready_count++;
//    signal_create_func = TRUE;
//  }
//
//  if (PHX_INTRPT_TIMEOUT & dwMask) {
//    /* TODO: we could offer to try and ABORT then re-START capture */
//    pixcisrc->timeout_occurred = TRUE;
//    signal_create_func = TRUE;
//  }
//
//  if (PHX_INTRPT_FIFO_OVERFLOW & dwMask) {
//    pixcisrc->fifo_overflow_occurred = TRUE;
//    signal_create_func = TRUE;
//  }
//
//
//
//  if (signal_create_func)
//    g_cond_signal (&pixcisrc->cond);
//  g_mutex_unlock (&pixcisrc->mutex);
//  /* after unlocking, _create will check for these errors and copy data */
//}

static gboolean
gst_pixcisrc_start (GstBaseSrc * bsrc)
{
  GstPixciSrc *src = GST_PIXCI_SRC (bsrc);
  int pxerr;
  GEnumClass *video_format_enum_class;
  GEnumValue *video_format_enum_value;

  GST_DEBUG_OBJECT (src, "start");

  if (strlen (src->format_file)
      && !g_file_test (src->format_file, G_FILE_TEST_EXISTS)) {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        ("Format file does not exist: %s", src->format_file), (NULL));
    return FALSE;
  }

  /* open XCLIB library and driver */
  video_format_enum_class = g_type_class_ref (GST_TYPE_PIXCI_VIDEO_FORMAT);
  video_format_enum_value =
      g_enum_get_value (video_format_enum_class, src->format_name);
  pxerr =
      pxd_PIXCIopen (src->driver_params, video_format_enum_value->value_name,
      src->format_file);
  g_type_class_unref (video_format_enum_class);
  if (pxerr) {
    char buf[1024];
    pxd_mesgFaultText (src->unitmap, buf, 1024);
    GST_ELEMENT_ERROR (src, LIBRARY, INIT,
        ("Failed to open XCLIB library and driver"), ("%s", buf));
    return FALSE;
  }
  src->pixci_open = TRUE;

  GST_DEBUG_OBJECT (src, "DriverId: %s", pxd_infoDriverId ());
  GST_DEBUG_OBJECT (src, "LibraryId: %s", pxd_infoLibraryId ());
  GST_DEBUG_OBJECT (src, "Frame buffer memory: %d",
      pxd_infoMemsize (src->unitmap));
  GST_DEBUG_OBJECT (src, "Model: %d", pxd_infoModel (src->unitmap));
  GST_DEBUG_OBJECT (src, "SubModel: %d", pxd_infoSubmodel (src->unitmap));
  GST_DEBUG_OBJECT (src, "Units: %d", pxd_infoUnits ());

  return TRUE;
}

static gboolean
gst_pixcisrc_stop (GstBaseSrc * bsrc)
{
  GstPixciSrc *src = GST_PIXCI_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "stop");

  pxd_PIXCIclose ();
  src->pixci_open = FALSE;

  /* TODO: stop acq/release cam? */

  src->dropped_frame_count = 0;
  /*pixcisrc->last_time_code = -1; */

  return TRUE;
}

static GstCaps *
gst_pixcisrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstPixciSrc *src = GST_PIXCI_SRC (bsrc);
  GstCaps *caps;
  gint width, height;

  if (!src->pixci_open) {
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  } else {
    gint components, bits_per_component;
    gdouble par;
    GstVideoInfo vinfo;

    /* Create video info */
    gst_video_info_init (&vinfo);

    width = pxd_imageXdim ();
    height = pxd_imageYdim ();

    par = pxd_imageAspectRatio ();
    if (par != 0) {
      vinfo.par_d = 10000;
      vinfo.par_n = (gint) (par * vinfo.par_d);
    }

    bits_per_component = pxd_imageBdim ();
    components = pxd_imageCdim ();

    if (components == 1 && bits_per_component <= 8) {
      gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_GRAY8, width, height);
      caps = gst_video_info_to_caps (&vinfo);
    } else if (components == 1 &&
        bits_per_component > 8 && bits_per_component <= 16) {
      GValue val = G_VALUE_INIT;
      GstStructure *s;

      if (G_BYTE_ORDER == G_LITTLE_ENDIAN) {
        gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_GRAY16_LE, width,
            height);
      } else if (G_BYTE_ORDER == G_BIG_ENDIAN) {
        gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_GRAY16_BE, width,
            height);
      }
      caps = gst_video_info_to_caps (&vinfo);

      /* set bpp, extra info for GRAY16 so elements can scale properly */
      s = gst_caps_get_structure (caps, 0);
      g_value_init (&val, G_TYPE_INT);
      g_value_set_int (&val, bits_per_component);
      gst_structure_set_value (s, "bpp", &val);
      g_value_unset (&val);
    } else {
      GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
          (("Unknown or unsupported color format.")), (NULL));
      goto Error;
    }

  }

  GST_DEBUG_OBJECT (src, "The caps before filtering are %" GST_PTR_FORMAT,
      caps);

  if (filter) {
    GstCaps *tmp = gst_caps_intersect (caps, filter);
    gst_caps_unref (caps);
    caps = tmp;
  }

  GST_DEBUG_OBJECT (src, "The caps after filtering are %" GST_PTR_FORMAT, caps);

  return caps;

Error:
  return NULL;
}

static gboolean
gst_pixcisrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstPixciSrc *src = GST_PIXCI_SRC (bsrc);
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

static GstFlowReturn
gst_pixcisrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstPixciSrc *src = GST_PIXCI_SRC (psrc);
  guint dropped_frame_count = 0;
  guint new_dropped_frames;
  gint i;
  guint n;
  GstMapInfo minfo;
  pxbuffer_t buffer = 1;
  int pxerr;

  /* Start acquisition */
  if (!src->acq_started) {
    /* TODO: start  capture, goLive? */
    src->acq_started = TRUE;
  }

  /* about to read/write variables modified by phx_callback */
  //g_mutex_lock (&src->mutex);

  /* wait for callback (we should always get at least a timeout( */
  /*g_cond_wait (&src->cond, &src->mutex); */

  //if (src->fifo_overflow_occurred) {
  //  /* TODO: we could offer to try and ABORT then re-START capture */
  //  GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
  //      (("Acquisition failure due to FIFO overflow.")), (NULL));
  //  g_mutex_unlock (&src->mutex);
  //  return GST_FLOW_ERROR;
  //}

  //if (src->timeout_occurred) {
  //  /* TODO: we could offer to try and ABORT then re-START capture */
  //  GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
  //      (("Acquisition failure due to timeout.")), (NULL));
  //  g_mutex_unlock (&src->mutex);
  //  return GST_FLOW_ERROR;
  //}

  //if (!src->buffer_ready) {
  //  GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
  //      (("You should not see this error, something very bad happened.")),
  //      (NULL));
  //  g_mutex_unlock (&src->mutex);
  //  return GST_FLOW_ERROR;
  //}

  //GST_LOG_OBJECT (src,
  //    "Processing new buffer %d (Frame start: %d), ready-processed = %d",
  //    src->buffer_ready_count, src->frame_start_count,
  //    src->buffer_ready_count - src->buffer_processed_count);
  //src->buffer_ready = FALSE;

  ///* frame_start is always >= buffer_ready */
  //dropped_frame_count =
  //    src->frame_start_count - src->buffer_ready_count;

  //g_mutex_unlock (&src->mutex);
  pxerr = pxd_doSnap (src->unitmap, buffer, src->timeout);
  if (pxerr) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        (("Failed to get buffer.")), (NULL));
    return GST_FLOW_ERROR;
  }

  /* TODO: use allocator or use from Pixci pool */
  *buf = gst_buffer_new_and_alloc (src->height * src->gst_stride);

  /* Copy image to buffer from surface TODO: use orc_memcpy */
  gst_buffer_map (*buf, &minfo, GST_MAP_WRITE);
  GST_LOG_OBJECT (src,
      "GstBuffer size=%d, gst_stride=%d, phx_stride=%d", minfo.size,
      src->gst_stride, src->px_stride);
  /* TODO: must use readuchar for 8-bit buffers */
  pxd_readushort (src->unitmap, buffer, 0, 0, -1, -1, minfo.data, minfo.size,
      "Grey");
  //for (i = 0; i < src->height; i++) {
  //  memcpy (minfo.data + i * src->gst_stride,
  //      ((guint8 *) buffer.pvAddress) + i * src->px_stride,
  //      src->gst_stride);
  //}
  gst_buffer_unmap (*buf, &minfo);

  /* Having processed the data, release the buffer ready for further image data */
  //src->buffer_processed_count++;

  /* check for dropped frames (can only detect more than one) */
  //new_dropped_frames = dropped_frame_count - src->dropped_frame_count;
  //if (new_dropped_frames > 0) {
  //  src->dropped_frame_count = dropped_frame_count;
  //  GST_WARNING ("Dropped %d frames (%d total)", new_dropped_frames,
  //      src->dropped_frame_count);
  //  /* TODO: emit message here about dropped frames */
  //}

  /* use time from capture board */
  //n = (src->buffer_processed_count -
  //    1) % src->num_capture_buffers;
  //GST_BUFFER_TIMESTAMP (*buf) = src->frame_start_times[n];
  //GST_BUFFER_DURATION (*buf) =
  //    GST_CLOCK_DIFF (src->frame_start_times[n],
  //    src->frame_end_times[n]);
  //GST_BUFFER_OFFSET (*buf) = src->buffer_processed_count - 1;
  //GST_BUFFER_OFFSET_END (*buf) = GST_BUFFER_OFFSET (*buf);

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_pixcisrc_debug, "pixcisrc", 0,
      "debug category for pixcisrc element");
  gst_element_register (plugin, "pixcisrc", GST_RANK_NONE,
      gst_pixcisrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    pixci,
    "Pixci frame grabber source",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
