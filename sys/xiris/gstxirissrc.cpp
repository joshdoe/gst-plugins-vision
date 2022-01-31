/* GStreamer
 * Copyright (C) 2022 MinhQuan Tran <minhquan.tran@adlinktech.com>
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
 * SECTION:element-gstxirissrc
 *
 * The xirissrc element uses Xiris' WedlSDK API to get video from Xiris welding cameras.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v xirissrc ! videoconvert ! autovideosink
 * ]|
 * Outputs camera output to screen.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <iostream>
#include <gst/gst.h>
#include <glib.h>
#include "gstxirissrc.h"
#include <string>

#include "common/genicampixelformat.h"

/* prototypes */
static void gst_xirissrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_xirissrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_xirissrc_dispose (GObject * object);
static void gst_xirissrc_finalize (GObject * object);

static gboolean gst_xirissrc_start (GstBaseSrc * bsrc);
static gboolean gst_xirissrc_stop (GstBaseSrc * bsrc);
static GstCaps *gst_xirissrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter);
static gboolean gst_xirissrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps);

static GstFlowReturn gst_xirissrc_create (GstPushSrc * bsrc, GstBuffer ** buf);

/* parameters */
typedef enum GST_XIRISSRC_PROP
{
  PROP_0,
  PROP_SHUTTER_MODE,
  PROP_GS_EXPOSURE,
  PROP_GS_FRAME_RATE_LIMIT,
  PROP_GS_FRAME_RATE_LIMIT_ENABLED,
  PROP_RS_FRAME_RATE,
  PROP_PIXEL_DEPTH,
} GST_XIRISSRC_PROP;

#define DEFAULT_SHUTTER_MODE                        "global"
#define DEFAULT_PROP_GS_EXPOSURE                    15998.674805
#define DEFAULT_PROP_GS_FRAME_RATE_LIMIT            30
#define DEFAULT_PROP_GS_FRAME_RATE_LIMIT_ENABLED    false
#define DEFAULT_PROP_RS_FRAME_RATE                  55
#define DEFAULT_PROP_PIXEL_DEPTH                    "Bpp8"
#define orc_memcpy(a,b,c) memcpy(a,b,c)

/* pad templates */
static GstStaticPadTemplate gst_xirissrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS_ANY
  );

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstXirisSrc, gst_xirissrc, GST_TYPE_PUSH_SRC,
  GST_DEBUG_CATEGORY_INIT (gst_xirissrc_debug_category, "xirissrc", 0,
  "debug category for xirissrc element"));

static void
gst_xirissrc_class_init (GstXirisSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
    &gst_xirissrc_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Xiris WeldSDK Video Source", "Source/Video/Device",
      "Xiris WeldSDK video source",
      "MinhQuan Tran <minhquan.tran@adlinktech.com");

  gobject_class->set_property = gst_xirissrc_set_property;
  gobject_class->get_property = gst_xirissrc_get_property;
  gobject_class->dispose = gst_xirissrc_dispose;
  gobject_class->finalize = gst_xirissrc_finalize;

  base_src_class->start = GST_DEBUG_FUNCPTR (gst_xirissrc_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_xirissrc_stop);
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_xirissrc_get_caps);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_xirissrc_set_caps);

  push_src_class->create = GST_DEBUG_FUNCPTR (gst_xirissrc_create);

  /* Install GOject properties */
  g_object_class_install_property (gobject_class, PROP_SHUTTER_MODE,
    g_param_spec_string ("shutter-mode", "Shutter mode",
      "(global/rolling) Specifies the shutter mode of the camera. Default to 'Global'.",
      DEFAULT_SHUTTER_MODE,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GS_EXPOSURE,
    g_param_spec_float ("global-exposure", "Global shutter exposure time",
      "(Microseconds) Specifies the exposure time in micro-seconds. Only applicable in shutter mode 'Global'.", 0.0, 1000000.0,
      DEFAULT_PROP_GS_EXPOSURE,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GS_FRAME_RATE_LIMIT,
    g_param_spec_float ("global-frame-rate-limit", "Global shutter frame rate limit",
      "(Frames per second) Specifies the frame rate limit. Only applicable in shutter mode 'Global'. Default to 'false'.", 0.0, 1000.0,
      DEFAULT_PROP_GS_FRAME_RATE_LIMIT,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GS_FRAME_RATE_LIMIT_ENABLED,
    g_param_spec_boolean ("global-frame-rate-limit-enabled", "Global shutter frame rate limit enabled",
      "(true/false) Enable the use of frame rate limit. Only applicable in shutter mode 'Global'.",
      DEFAULT_PROP_GS_FRAME_RATE_LIMIT_ENABLED,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_RS_FRAME_RATE,
    g_param_spec_double ("rolling-frame-rate", "Rolling shutter frame rate",
      "(Frames per second) Specifies the frame rate. Only applicable in shutter mode 'Rolling'.", 0.0, 1000.0,
      DEFAULT_PROP_RS_FRAME_RATE,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_PIXEL_DEPTH,
    g_param_spec_string ("pixel-depth", "Pixel depth",
      "(Bpp8/Bpp12/Bpp14/Bpp16) Specifies the pixel depth, in bits per pixel, of the outputs . Default to 'Bpp8'.",
      DEFAULT_PROP_PIXEL_DEPTH,
      (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_xirissrc_init (GstXirisSrc *src)
{
  GST_DEBUG_OBJECT (src, "Initializing defaults");

  src->detectorEvents = new DemoCameraDetectorEventSink();
  src->camera_connected = false;
  src->caps = NULL;

  // Default parameter values
  src->shutter_mode = g_strdup (DEFAULT_SHUTTER_MODE);
  src->global_exposure = DEFAULT_PROP_GS_EXPOSURE;
  src->global_frame_rate_limit = DEFAULT_PROP_GS_FRAME_RATE_LIMIT;
  src->global_frame_rate_limit_enabled = DEFAULT_PROP_GS_FRAME_RATE_LIMIT_ENABLED;
  src->rolling_frame_rate = DEFAULT_PROP_RS_FRAME_RATE;
  src->pixel_depth = g_strdup (DEFAULT_PROP_PIXEL_DEPTH);

  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (src), TRUE);
}

void
gst_xirissrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstXirisSrc *src = GST_XIRISSRC (object);

  GST_DEBUG_OBJECT (src, "Setting a property: %u", property_id);

  switch (property_id) {
    case PROP_SHUTTER_MODE:
      g_free (src->shutter_mode);
      src->shutter_mode = g_value_dup_string (value);
      break;
    case PROP_GS_EXPOSURE:
      src->global_exposure = g_value_get_float (value);
      break;
    case PROP_GS_FRAME_RATE_LIMIT:
      src->global_frame_rate_limit = g_value_get_float (value);
      break;
    case PROP_GS_FRAME_RATE_LIMIT_ENABLED:
      src->global_frame_rate_limit_enabled = g_value_get_boolean (value);
      break;
    case PROP_RS_FRAME_RATE:
      src->rolling_frame_rate = g_value_get_double (value);
      break;
    case PROP_PIXEL_DEPTH:
      g_free (src->pixel_depth);
      src->pixel_depth = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_xirissrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstXirisSrc *src = GST_XIRISSRC (object);

  GST_DEBUG_OBJECT (src, "Getting a property.");

  switch (property_id) {
    case PROP_SHUTTER_MODE:
      g_value_set_string (value, src->shutter_mode);
      break;
    case PROP_GS_EXPOSURE:
      g_value_set_float (value, src->global_exposure);
      break;
    case PROP_GS_FRAME_RATE_LIMIT:
      g_value_set_float (value, src->global_frame_rate_limit);
      break;
    case PROP_GS_FRAME_RATE_LIMIT_ENABLED:
      g_value_set_boolean (value, src->global_frame_rate_limit_enabled);
      break;
    case PROP_RS_FRAME_RATE:
      g_value_set_double (value, src->rolling_frame_rate);
      break;
    case PROP_PIXEL_DEPTH:
      g_value_set_string (value, src->pixel_depth);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static ShutterModes
gst_xirissrc_get_shutter_mode_enum (gchar *shutter_mode_str)
{
  if (!strcmp(shutter_mode_str, "global")) // Global shutter
  {
    return ShutterModes::Global;
  }
  else if (!strcmp(shutter_mode_str, "rolling")) // Rolling shutter
  {
    return ShutterModes::Rolling;
  }
}

static PixelDepths
gst_xirissrc_get_pixel_depth_enum (gchar *pixel_depth_str)
{
  if (!strcmp(pixel_depth_str, "Bpp8")) // Bpp8
  {
    return PixelDepths::Bpp8;
  }
  else if (!strcmp(pixel_depth_str, "Bpp12")) // Bpp12
  {
    return PixelDepths::Bpp12;
  }
  else if (!strcmp(pixel_depth_str, "Bpp14")) // Bpp14
  {
    return PixelDepths::Bpp14;
  }
  else if (!strcmp(pixel_depth_str, "Bpp16")) // Bpp16
  {
    return PixelDepths::Bpp16;
  }
}

void
gst_xirissrc_dispose (GObject * object)
{
  GstXirisSrc *src = GST_XIRISSRC (object);
  GST_DEBUG_OBJECT (src, "dispose");
  G_OBJECT_CLASS (gst_xirissrc_parent_class)->dispose (object);
}

void
gst_xirissrc_finalize (GObject * object)
{
  GstXirisSrc *src = GST_XIRISSRC (object);
  GST_DEBUG_OBJECT (src, "finalize");

  g_free (src->sinkEvents);
  g_free (src->detectorEvents);
  g_free (src->caps);
  g_free (src->shutter_mode);
  g_free (src->pixel_depth);

  G_OBJECT_CLASS (gst_xirissrc_parent_class)->finalize (object);
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "xirissrc", GST_RANK_NONE,
      GST_TYPE_XIRISSRC);
}

static gboolean
gst_xirissrc_start (GstBaseSrc * bsrc)
{
  GstXirisSrc *src = GST_XIRISSRC (bsrc);

  try
  {
    GST_DEBUG_OBJECT (src, "Starting Camera Detector...");

    CameraDetector::GetInstance()->AttachEventSink(src->detectorEvents);

    while(!gCameraReady);
    gWeldCamera->mShutterMode = gst_xirissrc_get_shutter_mode_enum(src->shutter_mode);
    gWeldCamera->mGlobalExposure = src->global_exposure;
    gWeldCamera->mGlobalFrameRateLimit = src->global_frame_rate_limit;
    gWeldCamera->mGlobalFrameRateLimitEnabled = src->global_frame_rate_limit_enabled;
    gWeldCamera->mRollingFrameRate = src->rolling_frame_rate;
    gWeldCamera->mPixelDepth = gst_xirissrc_get_pixel_depth_enum(src->pixel_depth);

    while(!gBufferReady);
    src->camera_connected = gWeldCamera->IsConnected();
    src->pixel_type = gWeldCamera->mCapturedImage.pixelType;
  }
  catch (const std::exception& e)
  {
    GST_DEBUG_OBJECT (src, "An exception occurred: %s", e.what());
    gWeldCamera->Disconnect();
  }
  catch (...)
  {
    GST_DEBUG_OBJECT (src, "Failed to start camera!");
  }

  return TRUE;
}

static gboolean
gst_xirissrc_stop (GstBaseSrc * bsrc)
{
  GstXirisSrc *src = GST_XIRISSRC (bsrc);
  GST_DEBUG_OBJECT (src, "stop");

  if (gWeldCamera)
  {
    gWeldCamera->BeginStop();
    if (gWeldCamera->WaitForStopComplete())
    {
        gWeldCamera->Disconnect();
    }
  }

  return TRUE;
}

static GstFlowReturn
gst_xirissrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstXirisSrc *src = GST_XIRISSRC (psrc);
  GstMapInfo mapInfo;

  try
  {
    gint sizeInBytes = gWeldCamera->mCapturedImage.widthStep * gWeldCamera->mCapturedImage.height;
    *buf = gst_buffer_new_and_alloc(sizeInBytes);
    gst_buffer_map(*buf, &mapInfo, GST_MAP_WRITE);
    orc_memcpy(mapInfo.data, gWeldCamera->mCapturedImage.data, mapInfo.size);
    gst_buffer_unmap(*buf, &mapInfo);
  }
  catch (const std::exception& e)
  {
    GST_DEBUG_OBJECT (src, "An exception occurred: %s", e.what());
    gWeldCamera->Disconnect();
  }

  return GST_FLOW_OK;
}

static gchar*
gst_xirissrc_get_pixel_type_str (XPixelType pixel_type_enum)
{
  gchar* pixel_type_str;
  switch (pixel_type_enum)
  {
    case XPixelType::Mono8: // 8-bit Mono
      pixel_type_str = (gchar*)"Mono8";
      break;
    case XPixelType::Mono12Packed:  // 12-bit Mono (Packed in 3 bytes per 2 pixels.)
      pixel_type_str = (gchar*)"Mono12Packed";
      break;
    case XPixelType::BayerGR8:  // 8-bit Bayer Encoded GR
      pixel_type_str = (gchar*)"BayerGR8";
      break;
    case XPixelType::BayerRG8:  // 8-bit Bayer Encoded RG
      pixel_type_str = (gchar*)"BayerRG8";
      break;
    case XPixelType::BayerGB8:  // 8-bit Bayer Encoded GB
      pixel_type_str = (gchar*)"BayerGB8";
      break;
    case XPixelType::BayerBG8:  // 8-bit Bayer Encoded BG
      pixel_type_str = (gchar*)"BayerBG8";
      break;
    case XPixelType::BayerRG12Packed: // 12-bit Bayer Encoded RG (Packed in 3 bytes per 2 pixels.)
      pixel_type_str = (gchar*)"BayerRG12Packed";
      break;
    case XPixelType::BayerGR16: // 16-bit Bayer Encoded GR
      pixel_type_str = (gchar*)"BayerGR16";
      break;
    case XPixelType::BayerRG16: // 16-bit Bayer Encoded RG
      pixel_type_str = (gchar*)"BayerRG16";
      break;
    case XPixelType::Mono16:  // 16-bit Mono
      pixel_type_str = (gchar*)"Mono16";
      break;
    case XPixelType::RGB8:  // 8-bit RGB
      pixel_type_str = (gchar*)"RGB8";
      break;
    case XPixelType::RGB16: // 16-bit RGB
      pixel_type_str = (gchar*)"RGB16";
      break;
    case XPixelType::RGBA8: // 8-bit RGBA
      pixel_type_str = (gchar*)"RGBA8";
      break;
    case XPixelType::RGBA16:  //16-bit RGBA
      pixel_type_str = (gchar*)"RGBA16";
      break;
    case XPixelType::Mono14:  //14-bit Mono (Unpacked)
      pixel_type_str = (gchar*)"Mono14";
      break;
    case XPixelType::RGB14: // 14-bit Color
      pixel_type_str = (gchar*)"RGB14";
      break;
    case XPixelType::Mono12:  // 12-bit Mono (Unpacked)
      pixel_type_str = (gchar*)"Mono12";
      break;
    case XPixelType::RGB12: // 12-bit Color
      pixel_type_str = (gchar*)"RGB12";
      break;
    case XPixelType::MonoSF:  // 32-bit Floating-Point Mono
      pixel_type_str = (gchar*)"MonoSF";
      break;
    case XPixelType::MonoDF:  // 64-bit Floating-Point Mono
      pixel_type_str = (gchar*)"MonoDF";
      break;
    case XPixelType::RGBSF: // 32-bit Floating-Point RGB
      pixel_type_str = (gchar*)"RGBSF";
      break;
    case XPixelType::RGBDF: // 64-bit Floating-Point RGB
      pixel_type_str = (gchar*)"RGBDF";
      break;
    case XPixelType::BayerRG12: // 12-bit Bayer Encoded RG
      pixel_type_str = (gchar*)"BayerRG12";
      break;
    default:
      pixel_type_str = (gchar*)"Unknown";
      break;
  }
  return pixel_type_str;
}

static GstCaps *
gst_xirissrc_get_supported_caps (GstXirisSrc * src)
{
  GstCaps *caps;
  int i;
  GString *format = g_string_new (NULL);
  caps = gst_caps_new_empty ();

  for (i = 0; i < G_N_ELEMENTS (gst_genicam_pixel_format_infos); i++) {
    const GstGenicamPixelFormatInfo *info = &gst_genicam_pixel_format_infos[i];

    if (g_ascii_strncasecmp (gst_xirissrc_get_pixel_type_str(src->pixel_type), info->pixel_format,
            -1) != 0) {
      continue;
    }

    g_string_printf (format, "%s", info->pixel_format);
    GST_DEBUG("PixelType: %s", format->str);

    if (CXImage::PixelTypeIsSupported(src->pixel_type))
    {
      GstCaps *format_caps;
      GST_DEBUG_OBJECT (src, "PixelType %s supported, adding to caps",
          info->pixel_format);

      format_caps =
          gst_genicam_pixel_format_caps_from_pixel_format_var
          (info->pixel_format, G_BYTE_ORDER, gWeldCamera->getMaximumImageWidth(), gWeldCamera->getMaximumImageHeight());

      if (format_caps)
      {
        gst_caps_append (caps, format_caps);
      }
    }
  }

  if (!gst_caps_is_empty(caps))
  {
    GST_DEBUG_OBJECT (src, "Supported caps: %" GST_PTR_FORMAT, caps);
  }

  g_string_free (format, TRUE);
  return caps;
}

static void
gst_xirissrc_update_caps (GstXirisSrc * src)
{
  if (src->caps != NULL) {
    gst_caps_unref (src->caps);
  }
  src->caps = gst_xirissrc_get_supported_caps (src);
}

/* caps negotiation */
static GstCaps *
gst_xirissrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstXirisSrc *src = GST_XIRISSRC (bsrc);

  GST_DEBUG_OBJECT (src, "Received request for caps. Filter:\n%" GST_PTR_FORMAT, filter);

  if (!src->camera_connected)
  {
    GST_DEBUG_OBJECT (src, "Camera not connected");
    return gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (bsrc));
  }
  else
  {
    gst_xirissrc_update_caps (src);
    GstCaps *result = gst_caps_copy (src->caps);
    if (!gst_caps_is_empty(result))
    {
      GST_DEBUG_OBJECT (src, "Return caps: %" GST_PTR_FORMAT, result);
      return result;
    }
    else
    {
      GST_ERROR_OBJECT (src, "PixelType %s has no supported caps", gst_xirissrc_get_pixel_type_str(src->pixel_type));
    }
  }
}

static gboolean
gst_xirissrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstXirisSrc *src = GST_XIRISSRC (bsrc);

  GST_DEBUG_OBJECT (src, "Setting caps to %" GST_PTR_FORMAT, caps);

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    xiris,
    "Xiris WeldSDK video elements",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
