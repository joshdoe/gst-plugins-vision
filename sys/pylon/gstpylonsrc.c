/* GStreamer
 * Copyright (C) 2016-2017 Ingmars Melkis <zingmars@playgineering.com>
 * Copyright (C) 2018 Ingmars Melkis <contact@zingmars.me>
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
 * SECTION:element-gstpylonsrc
 *
 * The pylonsrc element uses Basler's pylonc API to get video from Basler's USB3 Vision cameras.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v pylonsrc ! bayer2rgb ! videoconvert ! xvimagesink
 * ]|
 * Outputs camera output to screen.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstpylonsrc.h"
#include <gst/gst.h>
#include <glib.h>

#include "common/genicampixelformat.h"


/* PylonC */
_Bool pylonc_reset_camera (GstPylonSrc * src);
_Bool pylonc_connect_camera (GstPylonSrc * src);
void pylonc_disconnect_camera (GstPylonSrc * src);
void pylonc_print_camera_info (GstPylonSrc * src,
    PYLON_DEVICE_HANDLE deviceHandle, int deviceId);
void pylonc_terminate ();


/* debug category */
GST_DEBUG_CATEGORY_STATIC (gst_pylonsrc_debug_category);
#define GST_CAT_DEFAULT gst_pylonsrc_debug_category
#define PYLONC_CHECK_ERROR(obj, res) if (res != GENAPI_E_OK) { char* errMsg; size_t length; GenApiGetLastErrorMessage( NULL, &length ); errMsg = (char*) malloc( length ); GenApiGetLastErrorMessage( errMsg, &length ); GST_CAT_LEVEL_LOG(GST_CAT_DEFAULT, GST_LEVEL_NONE, obj, "PylonC error: %s (%#08x).\n", errMsg, (unsigned int) res); free(errMsg); GenApiGetLastErrorDetail( NULL, &length ); errMsg = (char*) malloc( length ); GenApiGetLastErrorDetail( errMsg, &length ); GST_CAT_LEVEL_LOG(GST_CAT_DEFAULT, GST_LEVEL_NONE, obj, "PylonC error: %s\n", errMsg); free(errMsg); goto error; }

/* prototypes */
static void gst_pylonsrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_pylonsrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_pylonsrc_dispose (GObject * object);
static void gst_pylonsrc_finalize (GObject * object);

static gboolean gst_pylonsrc_start (GstBaseSrc * bsrc);
static gboolean gst_pylonsrc_stop (GstBaseSrc * bsrc);
static GstCaps *gst_pylonsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter);
static gboolean gst_pylonsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps);

static GstFlowReturn gst_pylonsrc_create (GstPushSrc * bsrc, GstBuffer ** buf);

/* parameters */
enum
{
  PROP_0,
  PROP_CAMERA,
  PROP_HEIGHT,
  PROP_WIDTH,
  PROP_BINNINGH,
  PROP_BINNINGV,
  PROP_LIMITBANDWIDTH,
  PROP_MAXBANDWIDTH,
  PROP_SENSORREADOUTMODE,
  PROP_ACQUISITIONFRAMERATEENABLE,
  PROP_FPS,
  PROP_LIGHTSOURCE,
  PROP_AUTOEXPOSURE,
  PROP_EXPOSURE,
  PROP_AUTOWHITEBALANCE,
  PROP_BALANCERED,
  PROP_BALANCEGREEN,
  PROP_BALANCEBLUE,
  PROP_COLORREDHUE,
  PROP_COLORREDSATURATION,
  PROP_COLORYELLOWHUE,
  PROP_COLORYELLOWSATURATION,
  PROP_COLORGREENHUE,
  PROP_COLORGREENSATURATION,
  PROP_COLORCYANHUE,
  PROP_COLORCYANSATURATION,
  PROP_COLORBLUEHUE,
  PROP_COLORBLUESATURATION,
  PROP_COLORMAGENTAHUE,
  PROP_COLORMAGENTASATURATION,
  PROP_AUTOGAIN,
  PROP_GAIN,
  PROP_BLACKLEVEL,
  PROP_GAMMA,
  PROP_RESET,
  PROP_TESTIMAGE,
  PROP_CONTINUOUSMODE,
  PROP_PIXEL_FORMAT,
  PROP_USERID,
  PROP_BASLERDEMOSAICING,
  PROP_DEMOSAICINGNOISEREDUCTION,
  PROP_DEMOSAICINGSHARPNESSENHANCEMENT,
  PROP_OFFSETX,
  PROP_CENTERX,
  PROP_OFFSETY,
  PROP_CENTERY,
  PROP_FLIPX,
  PROP_FLIPY,
  PROP_AUTOEXPOSUREUPPERLIMIT,
  PROP_AUTOEXPOSURELOWERLIMIT,
  PROP_GAINUPPERLIMIT,
  PROP_GAINLOWERLIMIT,
  PROP_AUTOPROFILE,
  PROP_AUTOBRIGHTNESSTARGET,
  PROP_TRANSFORMATIONSELECTOR,
  PROP_TRANSFORMATION00,
  PROP_TRANSFORMATION01,
  PROP_TRANSFORMATION02,
  PROP_TRANSFORMATION10,
  PROP_TRANSFORMATION11,
  PROP_TRANSFORMATION12,
  PROP_TRANSFORMATION20,
  PROP_TRANSFORMATION21,
  PROP_TRANSFORMATION22,
  PROP_FAILRATE,
  PROP_GRABTIMEOUT,
  PROP_PACKETSIZE,
  PROP_INTERPACKETDELAY,
  PROP_FRAMETRANSDELAY,
  PROP_BANDWIDTHRESERVE,
  PROP_BANDWIDTHRESERVEACC
};

#define DEFAULT_PROP_PIXEL_FORMAT "auto"

/* pad templates */
static GstStaticPadTemplate gst_pylonsrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

/* class initialisation */
G_DEFINE_TYPE_WITH_CODE (GstPylonSrc, gst_pylonsrc, GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (gst_pylonsrc_debug_category, "pylonsrc", 0,
        "debug category for pylonsrc element"));

static void
gst_pylonsrc_class_init (GstPylonSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_pylonsrc_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Basler Pylon Video Source", "Source/Video/Device",
      "Baser Pylon video source",
      "Ingmars Melkis <zingmars@playgineering.com>");

  gobject_class->set_property = gst_pylonsrc_set_property;
  gobject_class->get_property = gst_pylonsrc_get_property;
  gobject_class->dispose = gst_pylonsrc_dispose;
  gobject_class->finalize = gst_pylonsrc_finalize;

  base_src_class->start = GST_DEBUG_FUNCPTR (gst_pylonsrc_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_pylonsrc_stop);
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_pylonsrc_get_caps);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_pylonsrc_set_caps);

  push_src_class->create = GST_DEBUG_FUNCPTR (gst_pylonsrc_create);

  g_object_class_install_property (gobject_class, PROP_CAMERA,
      g_param_spec_int ("camera", "camera",
          "(Number) Camera ID as defined by Basler's API. If only one camera is connected this parameter will be ignored and the lone camera will be used. If there are multiple cameras and this parameter isn't defined, the plugin will output a list of available cameras and their IDs. Note that if there are multiple cameras available to the API and the camera parameter isn't defined then this plugin will not run.",
          0, 100, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_int ("height", "height",
          "(Pixels) The height of the picture. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          0, 10000, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_int ("width", "width",
          "(Pixels) The width of the picture. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          0, 10000, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BINNINGH,
      g_param_spec_int ("binningh", "Horizontal binning",
          "(Pixels) The number of pixels to be binned in horizontal direction. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          1, 6, 1, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BINNINGV,
      g_param_spec_int ("binningv", "Vertical binning",
          "(Pixels) The number of pixels to be binned in vertical direction. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          1, 6, 1, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_LIMITBANDWIDTH,
      g_param_spec_boolean ("limitbandwidth", "Link Throughput limit mode",
          "(true/false) Bandwidth limit mode. Disabling this will potentially allow the camera to reach higher frames per second, but can potentially damage your camera. Use with caution. Running the plugin without specifying this parameter will reset the value stored on the camera to `true`.",
          TRUE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MAXBANDWIDTH,
      g_param_spec_int ("maxbandwidth", "Maximum bandwidth",
          "(Bytes per second) This property sets the maximum bandwidth the camera can use. The camera will only use as much as it needs for the specified resolution and framerate. This setting will have no effect if limitbandwidth is set to off. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          0, 999999999, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_SENSORREADOUTMODE,
      g_param_spec_string ("sensorreadoutmode", "Sensor readout mode",
          "(normal/fast) This property changes the sensor readout mode. Fast will allow for faster framerates, but might cause quality loss. It might be required to either increase max bandwidth or disabling bandwidth limiting for this to cause any noticeable change. Running the plugin without specifying this parameter will reset the value stored on the camera to \"normal\".",
          "Normal",
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class,
      PROP_ACQUISITIONFRAMERATEENABLE,
      g_param_spec_boolean ("acquisitionframerateenable", "Custom FPS mode",
          "(true/false) Enables the use of custom fps values. Will be set to true if the fps poperty is set. Running the plugin without specifying this parameter will reset the value stored on the camera to false.",
          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FPS,
      g_param_spec_double ("fps", "Framerate",
          "(Frames per second) Sets the framerate of the video coming from the camera. Setting the value too high might cause the plugin to crash. Note that if your pipeline proves to be too much for your computer then the resulting video won't be in the resolution you set. Setting this parameter will set acquisitionframerateenable to true. The value of this parameter will be saved to the camera, but it will have no effect unless either this or the acquisitionframerateenable parameters are set. Reconnect the camera or use the reset parameter to reset.",
          0.0, 1024.0, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_LIGHTSOURCE,
      g_param_spec_string ("lightsource", "Lightsource preset",
          "(off, 2800k, 5000k, 6500k) Changes the colour balance settings to ones defined by presests. Just pick one that's closest to your environment's lighting. Running the plugin without specifying this parameter will reset the value stored on the camera to \"5000k\"",
          "5000k", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOEXPOSURE,
      g_param_spec_string ("autoexposure", "Automatic exposure setting",
          "(off, once, continuous) Controls whether or not the camera will try to adjust the exposure settings. Setting this parameter to anything but \"off\" will override the exposure parameter. Running the plugin without specifying this parameter will reset the value stored on the camera to \"off\"",
          "off", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_EXPOSURE,
      g_param_spec_double ("exposure", "Exposure",
          "(Microseconds) Exposure time for the camera in microseconds. Will only have an effect if autoexposure is set to off (default). Higher numbers will cause lower frame rate. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          0.0, 1000000.0, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOWHITEBALANCE,
      g_param_spec_string ("autowhitebalance", "Automatic colour balancing",
          "(off, once, continuous) Controls whether or not the camera will try to adjust the white balance settings. Setting this parameter to anything but \"off\" will override the exposure parameter. Running the plugin without specifying this parameter will reset the value stored on the camera to \"off\"",
          "off", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BALANCERED,
      g_param_spec_double ("balancered", "Red balance",
          "Specifies the red colour balance. the autowhitebalance must be set to \"off\" for this property to have any effect. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 15.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BALANCEGREEN,
      g_param_spec_double ("balancegreen", "Green balance",
          "Specifies the green colour balance. the autowhitebalance must be set to \"off\" for this property to have any effect. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 15.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BALANCEBLUE,
      g_param_spec_double ("balanceblue", "Blue balance",
          "Specifies the blue colour balance. the autowhitebalance must be set to \"off\" for this property to have any effect. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 15.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORREDHUE,
      g_param_spec_double ("colorredhue", "Red's hue",
          "Specifies the red colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORREDSATURATION,
      g_param_spec_double ("colorredsaturation", "Red's saturation",
          "Specifies the red colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORYELLOWHUE,
      g_param_spec_double ("coloryellowhue", "Yellow's hue",
          "Specifies the yellow colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORYELLOWSATURATION,
      g_param_spec_double ("coloryellowsaturation", "Yellow's saturation",
          "Specifies the yellow colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORGREENHUE,
      g_param_spec_double ("colorgreenhue", "Green's hue",
          "Specifies the green colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORGREENSATURATION,
      g_param_spec_double ("colorgreensaturation", "Green's saturation",
          "Specifies the green colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORCYANHUE,
      g_param_spec_double ("colorcyanhue", "Cyan's hue",
          "Specifies the cyan colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORCYANSATURATION,
      g_param_spec_double ("colorcyansaturation", "Cyan's saturation",
          "Specifies the cyan colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORBLUEHUE,
      g_param_spec_double ("colorbluehue", "Blue's hue",
          "Specifies the blue colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORBLUESATURATION,
      g_param_spec_double ("colorbluesaturation", "Blue's saturation",
          "Specifies the blue colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORMAGENTAHUE,
      g_param_spec_double ("colormagentahue", "Magenta's hue",
          "Specifies the magenta colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORMAGENTASATURATION,
      g_param_spec_double ("colormagentasaturation", "Magenta's saturation",
          "Specifies the magenta colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOGAIN,
      g_param_spec_string ("autogain", "Automatic gain",
          "(off, once, continuous) Controls whether or not the camera will try to adjust the gain settings. Setting this parameter to anything but \"off\" will override the exposure parameter. Running the plugin without specifying this parameter will reset the value stored on the camera to \"off\"",
          "off", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GAIN,
      g_param_spec_double ("gain", "Gain",
          "(dB) Sets the gain added on the camera before sending the frame to the computer. The value of this parameter will be saved to the camera, but it will be set to 0 every time this plugin is launched without specifying gain or overriden if the autogain parameter is set to anything that's not \"off\". Reconnect the camera or use the reset parameter to reset the stored value.",
          0.0, 12.0, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BLACKLEVEL,
      g_param_spec_double ("blacklevel", "Black Level",
          "(DN) Sets stream's black level. This parameter is processed on the camera before the picture is sent to the computer. The value of this parameter will be saved to the camera, but it will be set to 0 every time this plugin is launched without specifying this parameter. Reconnect the camera or use the reset parameter to reset the stored value.",
          0.0, 63.75, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GAMMA,
      g_param_spec_double ("gamma", "Gamma",
          "Sets the gamma correction value. This parameter is processed on the camera before the picture is sent to the computer. The value of this parameter will be saved to the camera, but it will be set to 1.0 every time this plugin is launched without specifying this parameter. Reconnect the camera or use the reset parameter to reset the stored value.",
          0.0, 3.9, 1.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_RESET,
      g_param_spec_string ("reset", "Camera reset settings",
          "(off, before, after). Controls whether or when the camera's settings will be reset. Setting this to \"before\" will wipe the settings before the camera initialisation begins. Setting this to \"after\" will reset the device once the pipeline closes. This can be useful for debugging or when you want to use the camera with other software that doesn't reset the camera settings before use (such as PylonViewerApp).",
          "off", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TESTIMAGE,
      g_param_spec_int ("testimage", "Test image",
          "(1-6) Specifies a test image to show instead of a video stream. Useful for debugging. Will be disabled by default.",
          0, 6, 0, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CONTINUOUSMODE,
      g_param_spec_boolean ("continuous", "Continuous mode",
          "(true/false) Used to switch between triggered and continuous mode. To switch to triggered mode this parameter has to be switched to false.",
          TRUE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_PIXEL_FORMAT,
      g_param_spec_string ("pixel-format", "Pixel format",
          "Force the pixel format (e.g., Mono8). Default to 'auto', which will use GStreamer negotiation.",
          DEFAULT_PROP_PIXEL_FORMAT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_USERID,
      g_param_spec_string ("userid", "Custom Device User ID",
          "(<string>) Sets the device custom id so that it can be identified later.",
          "", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BASLERDEMOSAICING,
      g_param_spec_boolean ("demosaicing", "Basler's Demosaicing mode'",
          "(true/false) Switches between simple and Basler's Demosaicing (PGI) mode. Note that this will not work if bayer output is used.",
          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class,
      PROP_DEMOSAICINGNOISEREDUCTION, g_param_spec_double ("noisereduction",
          "Noise reduction",
          "Specifies the amount of noise reduction to apply. To use this Basler's demosaicing mode must be enabled. Setting this will enable demosaicing mode.",
          0.0, 2.0, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class,
      PROP_DEMOSAICINGSHARPNESSENHANCEMENT,
      g_param_spec_double ("sharpnessenhancement", "Sharpness enhancement",
          "Specifies the amount of sharpness enhancement to apply. To use this Basler's demosaicing mode must be enabled. Setting this will enable demosaicing mode.",
          1.0, 3.98, 1.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_OFFSETX,
      g_param_spec_int ("offsetx", "horizontal offset",
          "(0-10000) Determines the vertical offset. Note that the maximum offset value is calculated during initialisation, and will not be shown in this output.",
          0, 10000, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_OFFSETY,
      g_param_spec_int ("offsety", "vertical offset",
          "(0-10000) Determines the vertical offset. Note that the maximum offset value is calculated during initialisation, and will not be shown in this output.",
          0, 10000, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CENTERX,
      g_param_spec_boolean ("centerx", "center horizontally",
          "(true/false) Setting this will center the horizontal offset. Setting this to true this will cause the plugin to ignore offsetx value.",
          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CENTERY,
      g_param_spec_boolean ("centery", "center vertically",
          "(true/false) Setting this will center the vertical offset. Setting this to true this will cause the plugin to ignore offsetx value.",
          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FLIPX,
      g_param_spec_boolean ("flipx", "Flip horizontally",
          "(true/false) Setting this will flip the image horizontally.", FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FLIPY,
      g_param_spec_boolean ("flipy", "Flip vertically",
          "(true/false) Setting this will flip the image vertically.", FALSE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOEXPOSURELOWERLIMIT,
      g_param_spec_double ("exposurelowerlimit", "Auto exposure lower limit",
          "(105-1000000) Sets the lower limit for the auto exposure function.",
          105.0, 1000000.0, 105.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOEXPOSUREUPPERLIMIT,
      g_param_spec_double ("exposureupperlimit", "Auto exposure upper limit",
          "(105-1000000) Sets the upper limit for the auto exposure function.",
          105.0, 1000000.0, 1000000.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GAINUPPERLIMIT,
      g_param_spec_double ("gainupperlimit", "Auto exposure upper limit",
          "(0-12.00921) Sets the upper limit for the auto gain function.", 0.0,
          12.00921, 12.00921,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GAINLOWERLIMIT,
      g_param_spec_double ("gainlowerlimit", "Auto exposure lower limit",
          "(0-12.00921) Sets the lower limit for the auto gain function.", 0.0,
          12.00921, 0.0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOBRIGHTNESSTARGET,
      g_param_spec_double ("autobrightnesstarget", "Auto brightness target",
          "(0.19608-0.80392) Sets the brightness value the auto exposure function should strive for.",
          0.19608, 0.80392, 0.50196,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOPROFILE,
      g_param_spec_string ("autoprofile", "Auto function minimize profile",
          "(gain/exposure) When the auto functions are on, this determines whether to focus on minimising gain or exposure.",
          "gain", (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION00,
      g_param_spec_double ("transformation00",
          "Color Transformation selector 00", "Gain00 transformation selector.",
          -8.0, 7.96875, 1.4375,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION01,
      g_param_spec_double ("transformation01",
          "Color Transformation selector 01", "Gain01 transformation selector.",
          -8.0, 7.96875, -0.3125,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION02,
      g_param_spec_double ("transformation02",
          "Color Transformation selector 02", "Gain02 transformation selector.",
          -8.0, 7.96875, -0.125,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION10,
      g_param_spec_double ("transformation10",
          "Color Transformation selector 10", "Gain10 transformation selector.",
          -8.0, 7.96875, -0.28125,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION11,
      g_param_spec_double ("transformation11",
          "Color Transformation selector 11", "Gain11 transformation selector.",
          -8.0, 7.96875, 1.75,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION12,
      g_param_spec_double ("transformation12",
          "Color Transformation selector 12", "Gain12 transformation selector.",
          -8.0, 7.96875, -0.46875,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION20,
      g_param_spec_double ("transformation20",
          "Color Transformation selector 20", "Gain20 transformation selector.",
          -8.0, 7.96875, 0.0625,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION21,
      g_param_spec_double ("transformation21",
          "Color Transformation selector 21", "Gain21 transformation selector.",
          -8.0, 7.96875, -0.8125,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION22,
      g_param_spec_double ("transformation22",
          "Color Transformation selector 22", "Gain22 transformation selector.",
          -8.0, 7.96875, 1.75,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATIONSELECTOR,
      g_param_spec_string ("transformationselector",
          "Color Transformation Selector",
          "(RGBRGB, RGBYUV, YUVRGB) Sets the type of color transformation done by the color transformation selectors.",
          "RGBRGB",
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FAILRATE,
      g_param_spec_int ("failrate", "Failed frames",
          "Specifies the number of consecutive frames to fail before failing everything.",
          0, 1000, 10,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property (gobject_class, PROP_GRABTIMEOUT,
      g_param_spec_int ("grabtimeout", "Initial load timeout",
          "Specifies the number of miiliseconds to wait for frame to be grabed from the camera.",
          0, 60000, 1000,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property (gobject_class, PROP_PACKETSIZE,
      g_param_spec_int ("packetsize", "Maximum size of data packet",
          "The packetsize parameter specifies the maximum size of a data packet transmitted via Ethernet. The value is in bytes. Default value 0 -> Use camera defaults",
          0, 16404, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS))); //TODO: Limits may be co-dependent on other transport layer parameters.
      g_object_class_install_property (gobject_class, PROP_INTERPACKETDELAY,
      g_param_spec_int ("interpacketdelay", "Inter-Packet Delay between packet transmissions",
          "If your network hardware can't handle the incoming packet rate, it is useful to increase the delay between packet transmissions.  Default value -1 -> Use camera defaults",
          -1, 273331, -1,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS))); //TODO: Limits may be co-dependent on other transport layer parameters
      g_object_class_install_property (gobject_class, PROP_FRAMETRANSDELAY,
      g_param_spec_int ("frametransdelay", "Delay for begin transmitting frame.",
          "Sets a delay in ticks between when camera begisn transmitting frame afther acquiring it. By default, one tick equals 8 ns. With PTP enabled, one tick equals 1 ns.  Default value -1 -> Use camera defaults",
          -1, 50000000, -1,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS))); //TODO: Limits may be co-dependent on other transport layer parameters
      g_object_class_install_property (gobject_class, PROP_BANDWIDTHRESERVE,
      g_param_spec_int ("bandwidthreserve", "Portion of bandwidth reserved for packet resends.",
          "The setting is expressed as a percentage of the assigned bandwidth.",
          -1, 200, -1,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS))); //TODO: Limits may be co-dependent on other transport layer parameters
      g_object_class_install_property (gobject_class, PROP_BANDWIDTHRESERVEACC,
      g_param_spec_int ("bandwidthreserveacc", "Pool of resends for unusual situations",
          "For situations when the network connection becomes unstable. A larger number of packet resends may be needed to transmit an image.  Default value 0 -> Use camera defaults",
          0, 200, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));//TODO: Limits may be co-dependent on other transport layer parameters    
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "pylonsrc", GST_RANK_NONE,
      GST_TYPE_PYLONSRC);
}

static void
gst_pylonsrc_init (GstPylonSrc * src)
{
  GST_DEBUG_OBJECT (src, "Initialising defaults");

  src->deviceConnected = FALSE;
  src->acquisition_configured = FALSE;
  src->caps = NULL;

  // Default parameter values
  src->continuousMode = TRUE;
  src->limitBandwidth = TRUE;
  src->setFPS = FALSE;
  src->demosaicing = FALSE;
  src->binningh = 1;
  src->binningv = 1;
  src->centerx = FALSE;
  src->centery = FALSE;
  src->flipx = FALSE;
  src->flipy = FALSE;
  src->offsetx = 99999;
  src->offsety = 99999;
  src->cameraId = 9999;
  src->height = 0;
  src->width = 0;
  src->maxBandwidth = 0;
  src->testImage = 0;
  src->sensorMode = "normal\0";
  src->lightsource = "5000k\0";
  src->autoexposure = "off\0";
  src->autowhitebalance = "off\0";
  src->autogain = "off\0";
  src->reset = "off\0";
  src->pixel_format = g_strdup (DEFAULT_PROP_PIXEL_FORMAT);
  src->userid = "\0";
  src->autoprofile = "default\0";
  src->transformationselector = "default\0";
  src->fps = 0.0;
  src->exposure = 0.0;
  src->gain = 0.0;
  src->blacklevel = 0.0;
  src->gamma = 1.0;
  src->balancered = 999.0;
  src->balancegreen = 999.0;
  src->balanceblue = 999.0;
  src->redhue = 999.0;
  src->redsaturation = 999.0;
  src->yellowhue = 999.0;
  src->yellowsaturation = 999.0;
  src->greenhue = 999.0;
  src->greensaturation = 999.0;
  src->cyanhue = 999.0;
  src->cyansaturation = 999.0;
  src->bluehue = 999.0;
  src->bluesaturation = 999.0;
  src->magentahue = 999.0;
  src->magentasaturation = 999.0;
  src->sharpnessenhancement = 999.0;
  src->noisereduction = 999.0;
  src->autoexposureupperlimit = 9999999.0;
  src->autoexposurelowerlimit = 9999999.0;
  src->gainupperlimit = 999.0;
  src->gainlowerlimit = 999.0;
  src->brightnesstarget = 999.0;
  src->transformation00 = 999.0;
  src->transformation01 = 999.0;
  src->transformation02 = 999.0;
  src->transformation10 = 999.0;
  src->transformation11 = 999.0;
  src->transformation12 = 999.0;
  src->transformation20 = 999.0;
  src->transformation21 = 999.0;
  src->transformation22 = 999.0;
  src->failrate = 10;
  src->grabtimeout = 1000;
  src->packetSize = 0;
  src->interPacketDelay = -1;
  src->frameTransDelay = -1;

  // Mark this element as a live source (disable preroll)
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (src), TRUE);
}

/* plugin's parameters/properties */
void
gst_pylonsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPylonSrc *src = GST_PYLONSRC (object);

  GST_DEBUG_OBJECT (src, "Setting a property.");

  switch (property_id) {
    case PROP_CAMERA:
      src->cameraId = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      src->height = g_value_get_int (value);
      break;
    case PROP_WIDTH:
      src->width = g_value_get_int (value);
      break;
    case PROP_BINNINGH:
      src->binningh = g_value_get_int (value);
      break;
    case PROP_BINNINGV:
      src->binningv = g_value_get_int (value);
      break;
    case PROP_OFFSETX:
      src->offsetx = g_value_get_int (value);
      break;
    case PROP_OFFSETY:
      src->offsety = g_value_get_int (value);
      break;
    case PROP_TESTIMAGE:
      src->testImage = g_value_get_int (value);
      break;
    case PROP_SENSORREADOUTMODE:
      src->sensorMode = g_value_dup_string (value + '\0');
      break;
    case PROP_LIGHTSOURCE:
      src->lightsource = g_value_dup_string (value + '\0');
      break;
    case PROP_AUTOEXPOSURE:
      src->autoexposure = g_value_dup_string (value + '\0');
      break;
    case PROP_AUTOWHITEBALANCE:
      src->autowhitebalance = g_value_dup_string (value + '\0');
      break;
    case PROP_PIXEL_FORMAT:
      g_free (src->pixel_format);
      src->pixel_format = g_value_dup_string (value);
      break;
    case PROP_AUTOGAIN:
      src->autogain = g_value_dup_string (value + '\0');
      break;
    case PROP_RESET:
      src->reset = g_value_dup_string (value + '\0');
      break;
    case PROP_AUTOPROFILE:
      src->autoprofile = g_value_dup_string (value + '\0');
      break;
    case PROP_TRANSFORMATIONSELECTOR:
      src->transformationselector = g_value_dup_string (value + '\0');
      break;
    case PROP_USERID:
      src->userid = g_value_dup_string (value + '\0');
      break;
    case PROP_BALANCERED:
      src->balancered = g_value_get_double (value);
      break;
    case PROP_BALANCEGREEN:
      src->balancegreen = g_value_get_double (value);
      break;
    case PROP_BALANCEBLUE:
      src->balanceblue = g_value_get_double (value);
      break;
    case PROP_COLORREDHUE:
      src->redhue = g_value_get_double (value);
      break;
    case PROP_COLORREDSATURATION:
      src->redsaturation = g_value_get_double (value);
      break;
    case PROP_COLORYELLOWHUE:
      src->yellowhue = g_value_get_double (value);
      break;
    case PROP_COLORYELLOWSATURATION:
      src->yellowsaturation = g_value_get_double (value);
      break;
    case PROP_COLORGREENHUE:
      src->greenhue = g_value_get_double (value);
      break;
    case PROP_COLORGREENSATURATION:
      src->greensaturation = g_value_get_double (value);
      break;
    case PROP_COLORCYANHUE:
      src->cyanhue = g_value_get_double (value);
      break;
    case PROP_COLORCYANSATURATION:
      src->cyansaturation = g_value_get_double (value);
      break;
    case PROP_COLORBLUEHUE:
      src->bluehue = g_value_get_double (value);
      break;
    case PROP_COLORBLUESATURATION:
      src->bluesaturation = g_value_get_double (value);
      break;
    case PROP_COLORMAGENTAHUE:
      src->magentahue = g_value_get_double (value);
      break;
    case PROP_COLORMAGENTASATURATION:
      src->magentasaturation = g_value_get_double (value);
      break;
    case PROP_MAXBANDWIDTH:
      src->maxBandwidth = g_value_get_int (value);
      break;
    case PROP_FLIPX:
      src->flipx = g_value_get_boolean (value);
      break;
    case PROP_FLIPY:
      src->flipy = g_value_get_boolean (value);
      break;
    case PROP_CENTERX:
      src->centerx = g_value_get_boolean (value);
      break;
    case PROP_CENTERY:
      src->centery = g_value_get_boolean (value);
      break;
    case PROP_LIMITBANDWIDTH:
      src->limitBandwidth = g_value_get_boolean (value);
      break;
    case PROP_ACQUISITIONFRAMERATEENABLE:
      src->setFPS = g_value_get_boolean (value);
      break;
    case PROP_CONTINUOUSMODE:
      src->continuousMode = g_value_get_boolean (value);
      break;
    case PROP_BASLERDEMOSAICING:
      src->demosaicing = g_value_get_boolean (value);
      break;
    case PROP_FPS:
      src->fps = g_value_get_double (value);
      break;
    case PROP_EXPOSURE:
      src->exposure = g_value_get_double (value);
      break;
    case PROP_GAIN:
      src->gain = g_value_get_double (value);
      break;
    case PROP_BLACKLEVEL:
      src->blacklevel = g_value_get_double (value);
      break;
    case PROP_GAMMA:
      src->gamma = g_value_get_double (value);
      break;
    case PROP_DEMOSAICINGNOISEREDUCTION:
      src->noisereduction = g_value_get_double (value);
      break;
    case PROP_AUTOEXPOSUREUPPERLIMIT:
      src->autoexposureupperlimit = g_value_get_double (value);
      break;
    case PROP_AUTOEXPOSURELOWERLIMIT:
      src->autoexposurelowerlimit = g_value_get_double (value);
      break;
    case PROP_GAINLOWERLIMIT:
      src->gainlowerlimit = g_value_get_double (value);
      break;
    case PROP_GAINUPPERLIMIT:
      src->gainupperlimit = g_value_get_double (value);
      break;
    case PROP_AUTOBRIGHTNESSTARGET:
      src->brightnesstarget = g_value_get_double (value);
      break;
    case PROP_DEMOSAICINGSHARPNESSENHANCEMENT:
      src->sharpnessenhancement = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION00:
      src->transformation00 = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION01:
      src->transformation01 = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION02:
      src->transformation02 = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION10:
      src->transformation10 = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION11:
      src->transformation11 = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION12:
      src->transformation12 = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION20:
      src->transformation20 = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION21:
      src->transformation21 = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION22:
      src->transformation22 = g_value_get_double (value);
      break;
    case PROP_FAILRATE:
      src->failrate = g_value_get_int (value);
      break;
    case PROP_GRABTIMEOUT:
      src->grabtimeout = g_value_get_int (value);
      break;
    case PROP_PACKETSIZE:
      src->packetSize = g_value_get_int (value);
      break;
    case PROP_INTERPACKETDELAY:
      src->interPacketDelay = g_value_get_int (value);
      break;
    case PROP_FRAMETRANSDELAY:
      src->frameTransDelay = g_value_get_int (value);
      break;
    case PROP_BANDWIDTHRESERVE:
      src->bandwidthReserve = g_value_get_int (value);
      break;
    case PROP_BANDWIDTHRESERVEACC:
      src->bandwidthReserveAcc = g_value_get_int (value);
      break;  
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_pylonsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstPylonSrc *src = GST_PYLONSRC (object);

  GST_DEBUG_OBJECT (src, "Getting a property.");

  switch (property_id) {
    case PROP_CAMERA:
      g_value_set_int (value, src->cameraId);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, src->height);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, src->width);
      break;
    case PROP_BINNINGH:
      g_value_set_int (value, src->binningh);
      break;
    case PROP_BINNINGV:
      g_value_set_int (value, src->binningv);
      break;
    case PROP_OFFSETX:
      g_value_set_int (value, src->offsetx);
      break;
    case PROP_OFFSETY:
      g_value_set_int (value, src->offsety);
      break;
    case PROP_TESTIMAGE:
      g_value_set_int (value, src->testImage);
      break;
    case PROP_SENSORREADOUTMODE:
      g_value_set_string (value, src->sensorMode);
      break;
    case PROP_LIGHTSOURCE:
      g_value_set_string (value, src->lightsource);
      break;
    case PROP_AUTOEXPOSURE:
      g_value_set_string (value, src->autoexposure);
      break;
    case PROP_AUTOWHITEBALANCE:
      g_value_set_string (value, src->autowhitebalance);
      break;
    case PROP_PIXEL_FORMAT:
      g_value_set_string (value, src->pixel_format);
      break;
    case PROP_USERID:
      g_value_set_string (value, src->userid);
      break;
    case PROP_AUTOGAIN:
      g_value_set_string (value, src->autogain);
      break;
    case PROP_RESET:
      g_value_set_string (value, src->reset);
      break;
    case PROP_AUTOPROFILE:
      g_value_set_string (value, src->autoprofile);
    case PROP_TRANSFORMATIONSELECTOR:
      g_value_set_string (value, src->transformationselector);
      break;
    case PROP_BALANCERED:
      g_value_set_double (value, src->balancered);
      break;
    case PROP_BALANCEGREEN:
      g_value_set_double (value, src->balancegreen);
      break;
    case PROP_BALANCEBLUE:
      g_value_set_double (value, src->balanceblue);
      break;
    case PROP_COLORREDHUE:
      g_value_set_double (value, src->redhue);
      break;
    case PROP_COLORREDSATURATION:
      g_value_set_double (value, src->redsaturation);
      break;
    case PROP_COLORYELLOWHUE:
      g_value_set_double (value, src->yellowhue);
      break;
    case PROP_COLORYELLOWSATURATION:
      g_value_set_double (value, src->yellowsaturation);
      break;
    case PROP_COLORGREENHUE:
      g_value_set_double (value, src->greenhue);
      break;
    case PROP_COLORGREENSATURATION:
      g_value_set_double (value, src->greensaturation);
      break;
    case PROP_COLORCYANHUE:
      g_value_set_double (value, src->cyanhue);
      break;
    case PROP_COLORCYANSATURATION:
      g_value_set_double (value, src->cyansaturation);
      break;
    case PROP_COLORBLUEHUE:
      g_value_set_double (value, src->bluehue);
      break;
    case PROP_COLORBLUESATURATION:
      g_value_set_double (value, src->bluesaturation);
      break;
    case PROP_COLORMAGENTAHUE:
      g_value_set_double (value, src->magentahue);
      break;
    case PROP_COLORMAGENTASATURATION:
      g_value_set_double (value, src->magentasaturation);
      break;
    case PROP_MAXBANDWIDTH:
      g_value_set_int (value, src->maxBandwidth);
      break;
    case PROP_FLIPX:
      g_value_set_boolean (value, src->flipx);
      break;
    case PROP_FLIPY:
      g_value_set_boolean (value, src->flipy);
      break;
    case PROP_CENTERX:
      g_value_set_boolean (value, src->centerx);
      break;
    case PROP_CENTERY:
      g_value_set_boolean (value, src->centery);
      break;
    case PROP_LIMITBANDWIDTH:
      g_value_set_boolean (value, src->limitBandwidth);
      break;
    case PROP_ACQUISITIONFRAMERATEENABLE:
      g_value_set_boolean (value, src->setFPS);
      break;
    case PROP_CONTINUOUSMODE:
      g_value_set_boolean (value, src->continuousMode);
      break;
    case PROP_BASLERDEMOSAICING:
      g_value_set_boolean (value, src->demosaicing);
      break;
    case PROP_FPS:
      g_value_set_double (value, src->fps);
      break;
    case PROP_EXPOSURE:
      g_value_set_double (value, src->exposure);
      break;
    case PROP_GAIN:
      g_value_set_double (value, src->gain);
      break;
    case PROP_BLACKLEVEL:
      g_value_set_double (value, src->blacklevel);
      break;
    case PROP_GAMMA:
      g_value_set_double (value, src->gamma);
      break;
    case PROP_DEMOSAICINGNOISEREDUCTION:
      g_value_set_double (value, src->noisereduction);
      break;
    case PROP_DEMOSAICINGSHARPNESSENHANCEMENT:
      g_value_set_double (value, src->sharpnessenhancement);
      break;
    case PROP_AUTOEXPOSUREUPPERLIMIT:
      g_value_set_double (value, src->sharpnessenhancement);
      break;
    case PROP_AUTOEXPOSURELOWERLIMIT:
      g_value_set_double (value, src->sharpnessenhancement);
      break;
    case PROP_GAINLOWERLIMIT:
      g_value_set_double (value, src->sharpnessenhancement);
      break;
    case PROP_GAINUPPERLIMIT:
      g_value_set_double (value, src->sharpnessenhancement);
      break;
    case PROP_AUTOBRIGHTNESSTARGET:
      g_value_set_double (value, src->sharpnessenhancement);
      break;
    case PROP_TRANSFORMATION00:
      g_value_set_double (value, src->transformation00);
      break;
    case PROP_TRANSFORMATION01:
      g_value_set_double (value, src->transformation01);
      break;
    case PROP_TRANSFORMATION02:
      g_value_set_double (value, src->transformation02);
      break;
    case PROP_TRANSFORMATION10:
      g_value_set_double (value, src->transformation10);
      break;
    case PROP_TRANSFORMATION11:
      g_value_set_double (value, src->transformation11);
      break;
    case PROP_TRANSFORMATION12:
      g_value_set_double (value, src->transformation12);
      break;
    case PROP_TRANSFORMATION20:
      g_value_set_double (value, src->transformation20);
      break;
    case PROP_TRANSFORMATION21:
      g_value_set_double (value, src->transformation21);
      break;
    case PROP_TRANSFORMATION22:
      g_value_set_double (value, src->transformation22);
      break;
    case PROP_FAILRATE:
      g_value_set_int (value, src->failrate);
      break;
    case PROP_GRABTIMEOUT:
      g_value_set_int (value, src->grabtimeout);
      break;
    case PROP_PACKETSIZE:
      g_value_set_int (value, src->packetSize);
      break;
    case PROP_INTERPACKETDELAY:
      g_value_set_int (value, src->interPacketDelay);
      break;
    case PROP_FRAMETRANSDELAY:
      g_value_set_int (value, src->frameTransDelay);
      break;
    case PROP_BANDWIDTHRESERVE:
      g_value_set_int (value, src->bandwidthReserve);
      break;
    case PROP_BANDWIDTHRESERVEACC:
      g_value_set_int (value, src->bandwidthReserveAcc);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

/* caps negotiation */
static GstCaps *
gst_pylonsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstPylonSrc *src = GST_PYLONSRC (bsrc);

  GST_DEBUG_OBJECT (src, "Received a request for caps.");
  if (!src->deviceConnected) {
    GST_DEBUG_OBJECT (src, "Could not send caps - no camera connected.");
    return gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (bsrc));
  } else {
    return src->caps;
  }
}

static gboolean
gst_pylonsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstPylonSrc *src = GST_PYLONSRC (bsrc);
  gint i;
  GString *format = g_string_new (NULL);

  GST_DEBUG_OBJECT (src, "Setting caps to %" GST_PTR_FORMAT, caps);

  g_free (src->pixel_format);
  src->pixel_format = NULL;
  for (i = 0; i < G_N_ELEMENTS (gst_genicam_pixel_format_infos); i++) {
    GstCaps *super_caps;
    GstGenicamPixelFormatInfo *info = &gst_genicam_pixel_format_infos[i];
    super_caps = gst_caps_from_string (info->gst_caps_string);
    g_string_printf (format, "EnumEntry_PixelFormat_%s", info->pixel_format);
    if (gst_caps_is_subset (caps, super_caps)
        && PylonDeviceFeatureIsAvailable (src->deviceHandle, format->str)) {
      src->pixel_format = g_strdup (info->pixel_format);
      GST_DEBUG_OBJECT (src, "Set caps match PixelFormat '%s'",
          src->pixel_format);
      break;
    }
  }
  g_string_free (format, TRUE);

  if (src->pixel_format == NULL)
    goto unsupported_caps;

  return TRUE;

unsupported_caps:
  GST_ERROR_OBJECT (bsrc, "Unsupported caps: %" GST_PTR_FORMAT, caps);
  return FALSE;
}

#define FEATURE_SUPPORTED(feat) PylonDeviceFeatureIsImplemented(src->deviceHandle, feat)

static gboolean
gst_pylonsrc_set_trigger (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Set camera trigger mode
  const char *triggerSelectorValue = "FrameStart";
  _Bool isAvailAcquisitionStart =
      PylonDeviceFeatureIsAvailable (src->deviceHandle,
      "EnumEntry_TriggerSelector_AcquisitionStart");
  _Bool isAvailFrameStart = PylonDeviceFeatureIsAvailable (src->deviceHandle,
      "EnumEntry_TriggerSelector_FrameStart");
  const char *triggerMode = (src->continuousMode) ? "Off" : "On";

  GST_DEBUG_OBJECT (src, "Setting trigger mode.");

  // Check to see if the camera implements the acquisition start trigger mode only
  if (isAvailAcquisitionStart && !isAvailFrameStart) {
    // Select the software trigger as the trigger source
    res =
        PylonDeviceFeatureFromString (src->deviceHandle, "TriggerSelector",
        "AcquisitionStart");
    PYLONC_CHECK_ERROR (src, res);
    res =
        PylonDeviceFeatureFromString (src->deviceHandle, "TriggerMode",
        triggerMode);
    PYLONC_CHECK_ERROR (src, res);
    triggerSelectorValue = "AcquisitionStart";
  } else {
    // Camera may have the acquisition start trigger mode and the frame start trigger mode implemented.
    // In this case, the acquisition trigger mode must be switched off.
    if (isAvailAcquisitionStart) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "TriggerSelector",
          "AcquisitionStart");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "TriggerMode",
          "Off");
      PYLONC_CHECK_ERROR (src, res);
    }
    // Disable frame burst start trigger if available
    if (PylonDeviceFeatureIsAvailable (src->deviceHandle,
            "EnumEntry_TriggerSelector_FrameBurstStart")) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "TriggerSelector",
          "FrameBurstStart");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "TriggerMode",
          "Off");
      PYLONC_CHECK_ERROR (src, res);
    }
    // To trigger each single frame by software or external hardware trigger: Enable the frame start trigger mode
    res =
        PylonDeviceFeatureFromString (src->deviceHandle, "TriggerSelector",
        "FrameStart");
    PYLONC_CHECK_ERROR (src, res);
    res =
        PylonDeviceFeatureFromString (src->deviceHandle, "TriggerMode",
        triggerMode);
    PYLONC_CHECK_ERROR (src, res);
  }

  if (!src->continuousMode) {
    // Set the acquisiton selector to FrameTrigger in case it was changed by something else before launching the plugin so we don't request frames when they're still capturing or something.
    res =
        PylonDeviceFeatureFromString (src->deviceHandle,
        "AcquisitionStatusSelector", "FrameTriggerWait");
    PYLONC_CHECK_ERROR (src, res);
  }
  GST_DEBUG_OBJECT (src,
      "Using \"%s\" trigger selector. Software trigger mode is %s.",
      triggerSelectorValue, triggerMode);
  res =
      PylonDeviceFeatureFromString (src->deviceHandle, "TriggerSelector",
      triggerSelectorValue);
  PYLONC_CHECK_ERROR (src, res);
  res =
      PylonDeviceFeatureFromString (src->deviceHandle, "TriggerSource",
      "Software");
  PYLONC_CHECK_ERROR (src, res);
  res =
      PylonDeviceFeatureFromString (src->deviceHandle, "AcquisitionMode",
      "Continuous");
  PYLONC_CHECK_ERROR (src, res);

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_select_device (GstPylonSrc * src)
{
  int i;
  size_t numDevices;
  GENAPIC_RESULT res;

  res = PylonEnumerateDevices (&numDevices);
  PYLONC_CHECK_ERROR (src, res);
  GST_DEBUG_OBJECT (src, "src: found %i Basler device(s).", (int) numDevices);
  if (numDevices == 0) {
    GST_ERROR_OBJECT (src, "No devices connected, canceling initialisation.");
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to initialise the camera"), ("No camera connected"));
    goto error;
  } else if (numDevices == 1) {
    if (src->cameraId != 9999) {
      GST_DEBUG_OBJECT (src,
          "Camera id was set, but was ignored as only one camera was found.");
    }
    src->cameraId = 0;
  } else if (numDevices > 1 && src->cameraId == 9999) {
    GST_DEBUG_OBJECT (src,
        "Multiple cameras found, and the user didn't specify which camera to use.");
    GST_DEBUG_OBJECT (src,
        "Please specify the camera using the CAMERA property.");
    GST_DEBUG_OBJECT (src, "The camera IDs are as follows: ");

    for (i = 0; i < numDevices; i++) {
      PYLON_DEVICE_HANDLE deviceHandle;
      res = PylonCreateDeviceByIndex (i, &deviceHandle);

      if (res == GENAPI_E_OK) {
        res =
            PylonDeviceOpen (deviceHandle,
            PYLONC_ACCESS_MODE_CONTROL | PYLONC_ACCESS_MODE_STREAM);
        PYLONC_CHECK_ERROR (src, res);

        pylonc_print_camera_info (src, deviceHandle, i);
      } else {
        GST_DEBUG_OBJECT (src,
            "ID:%i, Name: Unavailable, Serial No: Unavailable, Status: In use?",
            i);
      }

      PylonDeviceClose (deviceHandle);
      PylonDestroyDevice (deviceHandle);
    }

    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to initialise the camera"), ("No camera selected"));
    goto error;
  } else if (src->cameraId != 9999 && src->cameraId > numDevices) {
    GST_DEBUG_OBJECT (src, "No camera found with id %i.", src->cameraId);
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to initialise the camera"), ("No camera connected"));
    goto error;
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_connect_device (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  if (!pylonc_connect_camera (src)) {
    GST_ERROR_OBJECT (src, "Couldn't initialise the camera");
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to initialise the camera"), ("No camera connected"));
    goto error;
  }

  if (strcmp (src->userid, "") != 0) {
    if (PylonDeviceFeatureIsWritable (src->deviceHandle, "DeviceUserID")) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "DeviceUserID",
          src->userid);
      PYLONC_CHECK_ERROR (src, res);
    }
  }
  // Print the name of the camera
  pylonc_print_camera_info (src, src->deviceHandle, src->cameraId);

  // Reset the camera if required.
  src->reset = g_ascii_strdown (src->reset, -1);
  if (strcmp (src->reset, "before") == 0) {
    if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "DeviceReset")) {
      size_t numDevices;
      pylonc_reset_camera (src);
      pylonc_disconnect_camera (src);
      pylonc_terminate ();

      GST_DEBUG_OBJECT (src,
          "Camera reset. Waiting 6 seconds for it to fully reboot.");
      g_usleep (6 * G_USEC_PER_SEC);

      PylonInitialize ();
      res = PylonEnumerateDevices (&numDevices);
      PYLONC_CHECK_ERROR (src, res);

      if (!pylonc_connect_camera (src)) {
        GST_ERROR_OBJECT (src,
            "Couldn't initialise the camera. It looks like the reset failed. Please manually reconnect the camera and try again.");
        GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
            ("Failed to initialise the camera"), ("No camera connected"));
        goto error;
      }
    } else {
      GST_ERROR_OBJECT (src,
          "Couldn't reset the device - feature not supported. Cancelling startup.");
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"),
          ("Camera couldn't be reset properly."));
      goto error;
    }
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_resolution (GstPylonSrc * src)
{
  GENAPIC_RESULT res;
  int64_t width = 0, height = 0;

  // set binning of camera
  if (FEATURE_SUPPORTED ("BinningHorizontal") &&
      FEATURE_SUPPORTED ("BinningVertical")) {
    GST_DEBUG_OBJECT (src, "Setting horizontal binning to %d", src->binningh);
    res =
        PylonDeviceSetIntegerFeature (src->deviceHandle, "BinningHorizontal",
        src->binningh);
    PYLONC_CHECK_ERROR (src, res);
    GST_DEBUG_OBJECT (src, "Setting vertical binning to %d", src->binningv);
    res =
        PylonDeviceSetIntegerFeature (src->deviceHandle, "BinningVertical",
        src->binningv);
    PYLONC_CHECK_ERROR (src, res);
  }
  // Get the camera's resolution
  if (!FEATURE_SUPPORTED ("Width") || !FEATURE_SUPPORTED ("Height")) {
    GST_ERROR_OBJECT (src,
        "The camera doesn't seem to be reporting it's resolution.");
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to initialise the camera"),
        ("Camera isn't reporting it's resolution. (Unsupported device?)"));
    goto error;
  }
  // Default height/width
  res = PylonDeviceGetIntegerFeature (src->deviceHandle, "Width", &width);
  PYLONC_CHECK_ERROR (src, res);
  res = PylonDeviceGetIntegerFeature (src->deviceHandle, "Height", &height);
  PYLONC_CHECK_ERROR (src, res);

  // Max Width and Height.
  if (FEATURE_SUPPORTED ("WidthMax") && FEATURE_SUPPORTED ("HeightMax")) {
    int64_t maxWidth, maxHeight;
    res =
        PylonDeviceGetIntegerFeature (src->deviceHandle, "WidthMax", &maxWidth);
    src->maxWidth = (gint) maxWidth;
    PYLONC_CHECK_ERROR (src, res);
    res =
        PylonDeviceGetIntegerFeature (src->deviceHandle, "HeightMax",
        &maxHeight);
    src->maxHeight = (gint) maxHeight;
    PYLONC_CHECK_ERROR (src, res);
  }
  GST_DEBUG_OBJECT (src, "Max resolution is %dx%d.", src->maxWidth,
      src->maxHeight);

  // If custom resolution is set, check if it's even possible and set it
  if (src->height != 0 || src->width != 0) {
    if (src->width > src->maxWidth) {
      GST_DEBUG_OBJECT (src, "Set width is above camera's capabilities.");
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Wrong width specified"));
      goto error;
    } else if (src->width == 0) {
      src->width = (gint) width;
    }

    if (src->height > src->maxHeight) {
      GST_DEBUG_OBJECT (src, "Set height is above camera's capabilities.");
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Wrong height specified"));
      goto error;
    } else if (src->height == 0) {
      src->height = (gint) height;
    }
  } else {
    src->height = (gint) height;
    src->width = (gint) width;
  }

  // Set the final resolution
  res = PylonDeviceSetIntegerFeature (src->deviceHandle, "Width", src->width);
  PYLONC_CHECK_ERROR (src, res);
  res = PylonDeviceSetIntegerFeature (src->deviceHandle, "Height", src->height);
  PYLONC_CHECK_ERROR (src, res);
  GST_DEBUG_OBJECT (src, "Setting resolution to %dx%d.", src->width,
      src->height);

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_offset (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Set the offset
  if (!FEATURE_SUPPORTED ("OffsetX") || !FEATURE_SUPPORTED ("OffsetY")) {
    GST_WARNING_OBJECT (src,
        "The camera doesn't seem to allow setting offsets. Skipping...");
  } else {
    // Check if the user wants to center image first
    _Bool cameraSupportsCenterX = FEATURE_SUPPORTED ("CenterX");
    _Bool cameraSupportsCenterY = FEATURE_SUPPORTED ("CenterY");
    if (!cameraSupportsCenterX || !cameraSupportsCenterY) {
      GST_WARNING_OBJECT (src,
          "The camera doesn't seem to allow offset centering. Skipping...");
    } else {
      res =
          PylonDeviceSetBooleanFeature (src->deviceHandle, "CenterX",
          src->centerx);
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetBooleanFeature (src->deviceHandle, "CenterY",
          src->centery);
      PYLONC_CHECK_ERROR (src, res);
      GST_DEBUG_OBJECT (src, "Centering X: %s, Centering Y: %s.",
          src->centerx ? "True" : "False", src->centery ? "True" : "False");

      if (!src->centerx && src->offsetx != 99999) {
        int64_t maxoffsetx = src->maxWidth - src->width;

        if (maxoffsetx >= src->offsetx) {
          res =
              PylonDeviceSetIntegerFeature (src->deviceHandle, "OffsetX",
              src->offsetx);
          PYLONC_CHECK_ERROR (src, res);
          GST_DEBUG_OBJECT (src, "Setting X offset to %d", src->offsetx);
        } else {
          GST_DEBUG_OBJECT (src,
              "Set X offset is above camera's capabilities. (%d > %d)",
              src->offsetx, maxoffsetx);
          GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
              ("Failed to initialise the camera"),
              ("Wrong offset for X axis specified"));
          goto error;
        }
      }

      if (!src->centery && src->offsety != 99999) {
        int64_t maxoffsety = src->maxHeight - src->height;
        if (maxoffsety >= src->offsety) {
          res =
              PylonDeviceSetIntegerFeature (src->deviceHandle, "OffsetY",
              src->offsety);
          PYLONC_CHECK_ERROR (src, res);
          GST_DEBUG_OBJECT (src, "Setting Y offset to %d", src->offsety);
        } else {
          GST_DEBUG_OBJECT (src,
              "Set Y offset is above camera's capabilities. (%d > %d)",
              src->offsety, maxoffsety);
          GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
              ("Failed to initialise the camera"),
              ("Wrong offset for Y axis specified"));
          goto error;
        }
      }
    }
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_reverse (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Flip the image
  if (!FEATURE_SUPPORTED ("ReverseX")) {
    src->flipx = FALSE;
    GST_WARNING_OBJECT (src,
        "Camera doesn't support reversing the X axis. Skipping...");
  } else {
    if (!FEATURE_SUPPORTED ("ReverseY")) {
      src->flipy = FALSE;
      GST_WARNING_OBJECT (src,
          "Camera doesn't support reversing the Y axis. Skipping...");
    } else {
      res =
          PylonDeviceSetBooleanFeature (src->deviceHandle, "ReverseX",
          src->flipx);
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetBooleanFeature (src->deviceHandle, "ReverseY",
          src->flipy);
      PYLONC_CHECK_ERROR (src, res);
      GST_DEBUG_OBJECT (src, "Flipping X: %s, Flipping Y: %s.",
          src->flipx ? "True" : "False", src->flipy ? "True" : "False");
    }
  }

  return TRUE;

error:
  return FALSE;
}

static GstCaps *
gst_pylonsrc_get_supported_caps (GstPylonSrc * src)
{
  GstCaps *caps;
  int i;
  GString *format = g_string_new (NULL);
  gboolean auto_format = FALSE;

  if (g_ascii_strncasecmp (src->pixel_format, "auto", -1) == 0) {
    auto_format = TRUE;
  }

  caps = gst_caps_new_empty ();

  /* check every pixel format GStreamer supports */
  for (i = 0; i < G_N_ELEMENTS (gst_genicam_pixel_format_infos); i++) {
    const GstGenicamPixelFormatInfo *info = &gst_genicam_pixel_format_infos[i];

    if (!auto_format
        && g_ascii_strncasecmp (src->pixel_format, info->pixel_format,
            -1) != 0) {
      continue;
    }

    g_string_printf (format, "EnumEntry_PixelFormat_%s", info->pixel_format);
    if (PylonDeviceFeatureIsAvailable (src->deviceHandle, format->str)) {
      GstCaps *format_caps;

      GST_DEBUG_OBJECT (src, "PixelFormat %s supported, adding to caps",
          info->pixel_format);

      // TODO: query FPS
      format_caps =
          gst_genicam_pixel_format_caps_from_pixel_format (info->pixel_format,
          G_BYTE_ORDER, src->width, src->height, 30, 1, 1, 1);

      if (format_caps)
        gst_caps_append (caps, format_caps);
    }
  }

  GST_DEBUG_OBJECT (src, "Supported caps are %" GST_PTR_FORMAT, caps);

  return caps;
}

static gboolean
gst_pylonsrc_set_pixel_format (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  GST_DEBUG_OBJECT (src, "Using %s PixelFormat.", src->pixel_format);
  res =
      PylonDeviceFeatureFromString (src->deviceHandle, "PixelFormat",
      src->pixel_format);
  PYLONC_CHECK_ERROR (src, res);

  // Output the size of a pixel
  if (PylonDeviceFeatureIsReadable (src->deviceHandle, "PixelSize")) {
    char pixelSize[10];
    size_t siz = sizeof (pixelSize);

    res =
        PylonDeviceFeatureToString (src->deviceHandle, "PixelSize", pixelSize,
        &siz);
    PYLONC_CHECK_ERROR (src, res);
    GST_DEBUG_OBJECT (src, "Pixel is %s bits large.", pixelSize + 3);
  } else {
    GST_WARNING_OBJECT (src, "Couldn't read pixel size from the camera");
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_test_image (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Set whether test image will be shown
  if (FEATURE_SUPPORTED ("TestImageSelector")) {
    if (src->testImage != 0) {
      char *ImageId;
      GST_DEBUG_OBJECT (src, "Test image mode enabled.");
      ImageId = g_strdup_printf ("Testimage%d", src->testImage);
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "TestImageSelector",
          ImageId);
      PYLONC_CHECK_ERROR (src, res);
    } else {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "TestImageSelector",
          "Off");
      PYLONC_CHECK_ERROR (src, res);
    }
  } else {
    GST_WARNING_OBJECT (src, "The camera doesn't support test image mode.");
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_readout (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Set sensor readout mode (default: Normal)
  if (FEATURE_SUPPORTED ("SensorReadoutMode")) {
    src->sensorMode = g_ascii_strdown (src->sensorMode, -1);

    if (strcmp (src->sensorMode, "normal") == 0) {
      GST_DEBUG_OBJECT (src, "Setting the sensor readout mode to normal.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "SensorReadoutMode",
          "Normal");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->sensorMode, "fast") == 0) {
      GST_DEBUG_OBJECT (src, "Setting the sensor readout mode to fast.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "SensorReadoutMode",
          "Fast");
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_ERROR_OBJECT (src,
          "Invalid parameter value for sensorreadoutmode. Available values are normal/fast, while the value provided was \"%s\".",
          src->sensorMode);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Invalid parameters provided"));
      goto error;
    }
  } else {
    GST_WARNING_OBJECT (src,
        "Camera does not support changing the readout mode.");
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_bandwidth (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Set bandwidth limit mode (default: on)  
  if (FEATURE_SUPPORTED ("DeviceLinkThroughputLimitMode")) {
    if (src->limitBandwidth) {
      GST_DEBUG_OBJECT (src, "Limiting camera's bandwidth.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "DeviceLinkThroughputLimitMode", "On");
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_DEBUG_OBJECT (src, "Unlocking camera's bandwidth.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "DeviceLinkThroughputLimitMode", "Off");
      PYLONC_CHECK_ERROR (src, res);
    }
  } else {
    GST_WARNING_OBJECT (src,
        "Camera does not support disabling the throughput limit.");
  }

  // Set bandwidth limit
  if (FEATURE_SUPPORTED ("DeviceLinkThroughputLimit")) {
    if (src->maxBandwidth != 0) {
      if (!src->limitBandwidth) {
        GST_DEBUG_OBJECT (src,
            "Saving bandwidth limits, but because throughput mode is disabled they will be ignored.");
      }

      GST_DEBUG_OBJECT (src, "Setting bandwidth limit to %d B/s.",
          src->maxBandwidth);
      res =
          PylonDeviceSetIntegerFeature (src->deviceHandle,
          "DeviceLinkThroughputLimit", src->maxBandwidth);
      PYLONC_CHECK_ERROR (src, res);
    }
  } else {
    GST_WARNING_OBJECT (src,
        "Camera does not support changing the throughput limit.");
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_framerate (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Set framerate
  if (src->setFPS || (src->fps != 0)) {
    if (PylonDeviceFeatureIsAvailable (src->deviceHandle,
            "AcquisitionFrameRateEnable")) {
      res =
          PylonDeviceSetBooleanFeature (src->deviceHandle,
          "AcquisitionFrameRateEnable", TRUE);
      PYLONC_CHECK_ERROR (src, res);

      if (src->fps != 0
          && PylonDeviceFeatureIsAvailable (src->deviceHandle,
              "AcquisitionFrameRate")) {
        GST_DEBUG_OBJECT (src, "Capping framerate to %0.2lf.", src->fps);
        res =
            PylonDeviceSetFloatFeature (src->deviceHandle,
            "AcquisitionFrameRate", src->fps);
        PYLONC_CHECK_ERROR (src, res);
      } else {
        GST_DEBUG_OBJECT (src,
            "Enabled custom framerate limiter. See below for current framerate.");
      }
    }
  } else {
    if (PylonDeviceFeatureIsAvailable (src->deviceHandle,
            "AcquisitionFrameRateEnable")) {
      res =
          PylonDeviceSetBooleanFeature (src->deviceHandle,
          "AcquisitionFrameRateEnable", FALSE);
      PYLONC_CHECK_ERROR (src, res);
      GST_DEBUG_OBJECT (src, "Disabled custom framerate limiter.");
    }
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_lightsource (GstPylonSrc * src)
{
  GENAPIC_RESULT res;
  // Set lightsource preset
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "LightSourcePreset")) {
    src->lightsource = g_ascii_strdown (src->lightsource, -1);

    if (strcmp (src->lightsource, "off") == 0) {
      GST_DEBUG_OBJECT (src, "Not using a lightsource preset.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "LightSourcePreset",
          "Off");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->lightsource, "2800k") == 0) {
      GST_DEBUG_OBJECT (src,
          "Setting light preset to Tungsten 2800k (Incandescen light).");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "LightSourcePreset",
          "Tungsten2800K");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->lightsource, "5000k") == 0) {
      GST_DEBUG_OBJECT (src,
          "Setting light preset to Daylight 5000k (Daylight).");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "LightSourcePreset",
          "Daylight5000K");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->lightsource, "6500k") == 0) {
      GST_DEBUG_OBJECT (src,
          "Setting light preset to Daylight 6500k (Very bright day).");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "LightSourcePreset",
          "Daylight6500K");
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_ERROR_OBJECT (src,
          "Invalid parameter value for lightsource. Available values are off/2800k/5000k/6500k, while the value provided was \"%s\".",
          src->lightsource);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Invalid parameters provided"));
      goto error;
    }
  } else {
    GST_WARNING_OBJECT (src,
        "This camera doesn't have any lightsource presets");
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_auto_exp_gain_wb (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Enable/disable automatic exposure
  src->autoexposure = g_ascii_strdown (src->autoexposure, -1);
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "ExposureAuto")) {
    if (strcmp (src->autoexposure, "off") == 0) {
      GST_DEBUG_OBJECT (src, "Disabling automatic exposure.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "ExposureAuto",
          "Off");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->autoexposure, "once") == 0) {
      GST_DEBUG_OBJECT (src, "Making the camera only calibrate exposure once.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "ExposureAuto",
          "Once");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->autoexposure, "continuous") == 0) {
      GST_DEBUG_OBJECT (src,
          "Making the camera calibrate exposure automatically all the time.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "ExposureAuto",
          "Continuous");
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_ERROR_OBJECT (src,
          "Invalid parameter value for autoexposure. Available values are off/once/continuous, while the value provided was \"%s\".",
          src->autoexposure);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Invalid parameters provided"));
      goto error;
    }
  } else {
    GST_WARNING_OBJECT (src, "This camera doesn't support automatic exposure.");
  }

  // Enable/disable automatic gain
  src->autogain = g_ascii_strdown (src->autogain, -1);
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "GainAuto")) {
    if (strcmp (src->autogain, "off") == 0) {
      GST_DEBUG_OBJECT (src, "Disabling automatic gain.");
      res = PylonDeviceFeatureFromString (src->deviceHandle, "GainAuto", "Off");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->autogain, "once") == 0) {
      GST_DEBUG_OBJECT (src,
          "Making the camera only calibrate it's gain once.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "GainAuto", "Once");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->autogain, "continuous") == 0) {
      GST_DEBUG_OBJECT (src,
          "Making the camera calibrate gain settings automatically.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "GainAuto",
          "Continuous");
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_ERROR_OBJECT (src,
          "Invalid parameter value for autogain. Available values are off/once/continuous, while the value provided was \"%s\".",
          src->autogain);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Invalid parameters provided"));
      goto error;
    }
  } else {
    GST_WARNING_OBJECT (src, "This camera doesn't support automatic gain.");
  }

  // Enable/disable automatic white balance
  src->autowhitebalance = g_ascii_strdown (src->autowhitebalance, -1);
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "BalanceWhiteAuto")) {
    if (strcmp (src->autowhitebalance, "off") == 0) {
      GST_DEBUG_OBJECT (src, "Disabling automatic white balance.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "BalanceWhiteAuto",
          "Off");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->autowhitebalance, "once") == 0) {
      GST_DEBUG_OBJECT (src,
          "Making the camera only calibrate it's colour balance once.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "BalanceWhiteAuto",
          "Once");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->autowhitebalance, "continuous") == 0) {
      GST_DEBUG_OBJECT (src,
          "Making the camera calibrate white balance settings automatically.");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle, "BalanceWhiteAuto",
          "Continuous");
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_ERROR_OBJECT (src,
          "Invalid parameter value for autowhitebalance. Available values are off/once/continuous, while the value provided was \"%s\".",
          src->autowhitebalance);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Invalid parameters provided"));
      goto error;
    }
  } else {
    GST_WARNING_OBJECT (src,
        "This camera doesn't support automatic white balance.");
  }

  // Configure automatic exposure and gain settings
  if (src->autoexposureupperlimit != 9999999.0) {
    if (PylonDeviceFeatureIsAvailable (src->deviceHandle,
            "AutoExposureTimeUpperLimit")) {
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "AutoExposureTimeUpperLimit", src->autoexposureupperlimit);
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_WARNING_OBJECT (src,
          "This camera doesn't support changing the auto exposure limits.");
    }
  }
  if (src->autoexposurelowerlimit != 9999999.0) {
    if (src->autoexposurelowerlimit >= src->autoexposureupperlimit) {
      GST_ERROR_OBJECT (src,
          "Invalid parameter value for autoexposurelowerlimit. It seems like you're trying to set a lower limit (%.2f) that's higher than the upper limit (%.2f).",
          src->autoexposurelowerlimit, src->autoexposureupperlimit);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Invalid parameters provided"));
      goto error;
    }

    if (PylonDeviceFeatureIsAvailable (src->deviceHandle,
            "AutoExposureTimeLowerLimit")) {
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "AutoExposureTimeLowerLimit", src->autoexposurelowerlimit);
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_WARNING_OBJECT (src,
          "This camera doesn't support changing the auto exposure limits.");
    }
  }
  if (src->gainlowerlimit != 999.0) {
    if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "AutoGainLowerLimit")) {
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle, "AutoGainLowerLimit",
          src->gainlowerlimit);
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_WARNING_OBJECT (src,
          "This camera doesn't support changing the auto gain limits.");
    }
  }
  if (src->gainupperlimit != 999.0) {
    if (src->gainlowerlimit >= src->gainupperlimit) {
      GST_ERROR_OBJECT (src,
          "Invalid parameter value for gainupperlimit. It seems like you're trying to set a lower limit (%.5f) that's higher than the upper limit (%.5f).",
          src->gainlowerlimit, src->gainupperlimit);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Invalid parameters provided"));
      goto error;
    }

    if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "AutoGainUpperLimit")) {
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle, "AutoGainUpperLimit",
          src->gainupperlimit);
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_WARNING_OBJECT (src,
          "This camera doesn't support changing the auto gain limits.");
    }
  }
  if (src->brightnesstarget != 999.0) {
    if (PylonDeviceFeatureIsAvailable (src->deviceHandle,
            "AutoTargetBrightness")) {
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle, "AutoTargetBrightness",
          src->brightnesstarget);
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_WARNING_OBJECT (src,
          "This camera doesn't support changing the brightness target.");
    }
  }
  src->autoprofile = g_ascii_strdown (src->autoprofile, -1);
  if (strcmp (src->autoprofile, "default") != 0) {
    GST_DEBUG_OBJECT (src, "Setting automatic profile to minimise %s.",
        src->autoprofile);
    if (strcmp (src->autoprofile, "gain") == 0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "AutoFunctionProfile", "MinimizeGain");
      PYLONC_CHECK_ERROR (src, res);
    } else if (strcmp (src->autoprofile, "exposure") == 0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "AutoFunctionProfile", "MinimizeExposureTime");
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_ERROR_OBJECT (src,
          "Invalid parameter value for autoprofile. Available values are gain/exposure, while the value provided was \"%s\".",
          src->autoprofile);
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Invalid parameters provided"));
      goto error;
    }
  } else {
    GST_DEBUG_OBJECT (src,
        "Using the auto profile currently saved on the device.");
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_color (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Configure colour balance
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "BalanceRatio")) {
    if (strcmp (src->autowhitebalance, "off") == 0) {
      if (src->balancered != 999.0) {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "BalanceRatioSelector", "Red");
        PYLONC_CHECK_ERROR (src, res);
        res =
            PylonDeviceSetFloatFeature (src->deviceHandle, "BalanceRatio",
            src->balancered);
        PYLONC_CHECK_ERROR (src, res);

        GST_DEBUG_OBJECT (src, "Red balance set to %.2lf", src->balancered);
      } else {
        GST_DEBUG_OBJECT (src, "Using current settings for the colour red.");
      }

      if (src->balancegreen != 999.0) {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "BalanceRatioSelector", "Green");
        PYLONC_CHECK_ERROR (src, res);
        res =
            PylonDeviceSetFloatFeature (src->deviceHandle, "BalanceRatio",
            src->balancegreen);
        PYLONC_CHECK_ERROR (src, res);

        GST_DEBUG_OBJECT (src, "Green balance set to %.2lf", src->balancegreen);
      } else {
        GST_DEBUG_OBJECT (src, "Using current settings for the colour green.");
      }

      if (src->balanceblue != 999.0) {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "BalanceRatioSelector", "Blue");
        PYLONC_CHECK_ERROR (src, res);
        res =
            PylonDeviceSetFloatFeature (src->deviceHandle, "BalanceRatio",
            src->balanceblue);
        PYLONC_CHECK_ERROR (src, res);

        GST_DEBUG_OBJECT (src, "Blue balance set to %.2lf", src->balanceblue);
      } else {
        GST_DEBUG_OBJECT (src, "Using current settings for the colour blue.");
      }
    } else {
      GST_DEBUG_OBJECT (src,
          "Auto White Balance is enabled. Not setting Balance Ratio.");
    }
  }
  // Configure colour adjustment
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle,
          "ColorAdjustmentSelector")) {
    if (src->redhue != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Red");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle, "ColorAdjustmentHue",
          src->redhue);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Red hue set to %.2lf", src->redhue);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour red's hue.");
    }
    if (src->redsaturation != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Red");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorAdjustmentSaturation", src->redsaturation);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Red saturation set to %.2lf", src->redsaturation);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour red's saturation.");
    }

    if (src->yellowhue != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Yellow");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle, "ColorAdjustmentHue",
          src->yellowhue);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Yellow hue set to %.2lf", src->yellowhue);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour yellow's hue.");
    }
    if (src->yellowsaturation != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Yellow");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorAdjustmentSaturation", src->yellowsaturation);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Yellow saturation set to %.2lf",
          src->yellowsaturation);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour yellow's saturation.");
    }

    if (src->greenhue != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Green");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle, "ColorAdjustmentHue",
          src->greenhue);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Green hue set to %.2lf", src->greenhue);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour green's hue.");
    }
    if (src->greensaturation != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Green");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorAdjustmentSaturation", src->greensaturation);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Green saturation set to %.2lf",
          src->greensaturation);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour green's saturation.");
    }

    if (src->cyanhue != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Cyan");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle, "ColorAdjustmentHue",
          src->cyanhue);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Cyan hue set to %.2lf", src->cyanhue);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour cyan's hue.");
    }
    if (src->cyansaturation != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Cyan");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorAdjustmentSaturation", src->cyansaturation);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Cyan saturation set to %.2lf",
          src->cyansaturation);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour cyan's saturation.");
    }

    if (src->bluehue != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Blue");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle, "ColorAdjustmentHue",
          src->bluehue);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Blue hue set to %.2lf", src->bluehue);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour blue's hue.");
    }
    if (src->bluesaturation != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Blue");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorAdjustmentSaturation", src->bluesaturation);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Blue saturation set to %.2lf",
          src->bluesaturation);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour blue's saturation.");
    }

    if (src->magentahue != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Magenta");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle, "ColorAdjustmentHue",
          src->magentahue);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Magenta hue set to %.2lf", src->magentahue);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour magenta's hue.");
    }
    if (src->magentasaturation != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorAdjustmentSelector", "Magenta");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorAdjustmentSaturation", src->magentasaturation);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Magenta saturation set to %.2lf",
          src->magentasaturation);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved colour magenta's saturation.");
    }
  } else {
    GST_DEBUG_OBJECT (src,
        "This camera doesn't support adjusting colours. Skipping...");
  }

  // Configure colour transformation
  src->transformationselector =
      g_ascii_strdown (src->transformationselector, -1);
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle,
          "ColorTransformationSelector")) {
    if (strcmp (src->transformationselector, "default") != 0) {
      if (strcmp (src->transformationselector, "rgbrgb") == 0) {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "ColorTransformationSelector", "RGBtoRGB");
        PYLONC_CHECK_ERROR (src, res);
      } else if (strcmp (src->transformationselector, "rgbyuv") == 0) {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "ColorTransformationSelector", "RGBtoYUV");
        PYLONC_CHECK_ERROR (src, res);
      } else if (strcmp (src->transformationselector, "rgbyuv") == 0) {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "ColorTransformationSelector", "YUVtoRGB");
        PYLONC_CHECK_ERROR (src, res);
      } else {
        GST_ERROR_OBJECT (src,
            "Invalid parameter value for transformationselector. Available values are: RGBtoRGB, RGBtoYUV, YUVtoRGB. Value provided: \"%s\".",
            src->transformationselector);
        GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
            ("Failed to initialise the camera"),
            ("Invalid parameters provided"));
        goto error;
      }
    }

    if (src->transformation00 != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorTransformationSelector", "Gain00");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorTransformationValueSelector", src->transformation00);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Gain00 set to %.2lf", src->transformation00);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved Gain00 transformation value.");
    }

    if (src->transformation01 != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorTransformationValueSelector", "Gain01");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorTransformationValue", src->transformation01);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Gain01 set to %.2lf", src->transformation01);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved Gain01 transformation value.");
    }

    if (src->transformation02 != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorTransformationValueSelector", "Gain02");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorTransformationValue", src->transformation02);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Gain02 set to %.2lf", src->transformation02);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved Gain02 transformation value.");
    }

    if (src->transformation10 != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorTransformationValueSelector", "Gain10");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorTransformationValue", src->transformation10);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Gain10 set to %.2lf", src->transformation10);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved Gain10 transformation value.");
    }

    if (src->transformation11 != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorTransformationValueSelector", "Gain11");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorTransformationValue", src->transformation11);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Gain11 set to %.2lf", src->transformation11);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved Gain11 transformation value.");
    }

    if (src->transformation12 != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorTransformationValueSelector", "Gain12");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorTransformationValue", src->transformation12);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Gain12 set to %.2lf", src->transformation12);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved Gain12 transformation value.");
    }

    if (src->transformation20 != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorTransformationValueSelector", "Gain20");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorTransformationValue", src->transformation20);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Gain20 set to %.2lf", src->transformation20);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved Gain20 transformation value.");
    }

    if (src->transformation21 != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorTransformationValueSelector", "Gain21");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorTransformationValue", src->transformation21);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Gain21 set to %.2lf", src->transformation21);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved Gain21 transformation value.");
    }

    if (src->transformation22 != 999.0) {
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "ColorTransformationValueSelector", "Gain22");
      PYLONC_CHECK_ERROR (src, res);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle,
          "ColorTransformationValue", src->transformation22);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "Gain22 set to %.2lf", src->transformation22);
    } else {
      GST_DEBUG_OBJECT (src, "Using saved Gain22 transformation value.");
    }
  } else {
    GST_DEBUG_OBJECT (src,
        "This camera doesn't support transforming colours. Skipping...");
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_exposure_gain_level (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Configure exposure
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "ExposureTime")) {
    if (strcmp (src->autoexposure, "off") == 0) {
      if (src->exposure != 0.0) {
        GST_DEBUG_OBJECT (src, "Setting exposure to %0.2lf", src->exposure);
        res =
            PylonDeviceSetFloatFeature (src->deviceHandle, "ExposureTime",
            src->exposure);
        PYLONC_CHECK_ERROR (src, res);
      } else {
        GST_DEBUG_OBJECT (src,
            "Exposure property not set, using the saved exposure setting.");
      }
    } else {
      GST_WARNING_OBJECT (src,
          "Automatic exposure has been enabled, skipping setting manual exposure times.");
    }
  } else {
    GST_WARNING_OBJECT (src,
        "This camera doesn't support setting manual exposure.");
  }

  // Configure gain
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "Gain")) {
    if (strcmp (src->autogain, "off") == 0) {
      GST_DEBUG_OBJECT (src, "Setting gain to %0.2lf", src->gain);
      res = PylonDeviceSetFloatFeature (src->deviceHandle, "Gain", src->gain);
      PYLONC_CHECK_ERROR (src, res);
    } else {
      GST_WARNING_OBJECT (src,
          "Automatic gain has been enabled, skipping setting gain.");
    }
  } else {
    GST_WARNING_OBJECT (src,
        "This camera doesn't support setting manual gain.");
  }

  // Configure black level
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "BlackLevel")) {
    GST_DEBUG_OBJECT (src, "Setting black level to %0.2lf", src->blacklevel);
    res =
        PylonDeviceSetFloatFeature (src->deviceHandle, "BlackLevel",
        src->blacklevel);
    PYLONC_CHECK_ERROR (src, res);
  } else {
    GST_WARNING_OBJECT (src,
        "This camera doesn't support setting black level.");
  }

  // Configure gamma correction
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "Gamma")) {
    GST_DEBUG_OBJECT (src, "Setting gamma to %0.2lf", src->gamma);
    res = PylonDeviceSetFloatFeature (src->deviceHandle, "Gamma", src->gamma);
    PYLONC_CHECK_ERROR (src, res);
  } else {
    GST_WARNING_OBJECT (src,
        "This camera doesn't support setting gamma values.");
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_pgi (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  // Basler PGI
  if (FEATURE_SUPPORTED ("DemosaicingMode")) {
    if (src->demosaicing || src->sharpnessenhancement != 999.0
        || src->noisereduction != 999.0) {
      if (strncmp ("bayer", src->pixel_format, 5) != 0) {
        GST_DEBUG_OBJECT (src, "Enabling Basler's PGI.");
        res =
            PylonDeviceFeatureFromString (src->deviceHandle, "DemosaicingMode",
            "BaslerPGI");
        PYLONC_CHECK_ERROR (src, res);

        // PGI Modules (Noise reduction and Sharpness enhancement).
        if (src->noisereduction != 999.0) {
          if (PylonDeviceFeatureIsAvailable (src->deviceHandle,
                  "NoiseReduction")) {
            GST_DEBUG_OBJECT (src, "Setting PGI noise reduction to %0.2lf",
                src->noisereduction);
            res =
                PylonDeviceSetFloatFeature (src->deviceHandle, "NoiseReduction",
                src->noisereduction);
          } else {
            GST_ERROR_OBJECT (src,
                "This camera doesn't support noise reduction.");
          }
        } else {
          GST_DEBUG_OBJECT (src, "Using the stored value for noise reduction.");
        }
        if (src->sharpnessenhancement != 999.0) {
          if (PylonDeviceFeatureIsAvailable (src->deviceHandle,
                  "SharpnessEnhancement")) {
            GST_DEBUG_OBJECT (src,
                "Setting PGI sharpness enhancement to %0.2lf",
                src->sharpnessenhancement);
            res =
                PylonDeviceSetFloatFeature (src->deviceHandle,
                "SharpnessEnhancement", src->sharpnessenhancement);
          } else {
            GST_ERROR_OBJECT (src,
                "This camera doesn't support sharpness enhancement.");
          }
        } else {
          GST_DEBUG_OBJECT (src, "Using the stored value for noise reduction.");
        }
      } else {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle, "DemosaicingMode",
            "Simple");
        PYLONC_CHECK_ERROR (src, res);
      }
    } else {
      GST_DEBUG_OBJECT (src,
          "Usage of PGI is not permitted with bayer output. Skipping.");
    }
  } else {
    GST_DEBUG_OBJECT (src, "Basler's PGI is not supported. Skipping.");
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean gst_pylonsrc_set_packetsize (GstPylonSrc * src)
{
 GENAPIC_RESULT res;

  if (src->packetSize != 0) {
    if (FEATURE_SUPPORTED ("GevSCPSPacketSize")) {
      GST_DEBUG_OBJECT (src, "Setting packetsize to %d", src->packetSize);
      res = PylonDeviceSetIntegerFeature (src->deviceHandle,"GevSCPSPacketSize", src->packetSize);
      PYLONC_CHECK_ERROR (src, res);
    }
    else{
      GST_ERROR_OBJECT (src,"This camera doesn't support changin packetsize.");
      goto error;
    }
  } else {
      GST_DEBUG_OBJECT (src, "Using camera default packetSize");
  }
  return TRUE;

error:
  return FALSE;  
}

static gboolean gst_pylonsrc_set_interPacketDelay (GstPylonSrc * src)
{
 GENAPIC_RESULT res;

  if (src->interPacketDelay >= 0 ) {
    if (FEATURE_SUPPORTED ("GevSCPD")) {
      GST_DEBUG_OBJECT (src, "Setting interPacketDelay to %d", src->interPacketDelay);
      res = PylonDeviceSetIntegerFeature (src->deviceHandle,"GevSCPD", src->interPacketDelay);
      PYLONC_CHECK_ERROR (src, res);
    }
    else{
      GST_ERROR_OBJECT (src,"This camera doesn't support changin interPacketDelay.");
      goto error;
    }
  } else {
      GST_DEBUG_OBJECT (src, "Using camera default interPacketDelay");
  }
  return TRUE;

error:
  return FALSE;  
}

static gboolean gst_pylonsrc_set_frameTransDelay (GstPylonSrc * src)
{
 GENAPIC_RESULT res;

  if (src->frameTransDelay >= 0 ) {
    if (FEATURE_SUPPORTED ("GevSCFTD")) {
      GST_DEBUG_OBJECT (src, "Setting frameTransDelay to %d", src->frameTransDelay);
      res = PylonDeviceSetIntegerFeature (src->deviceHandle,"GevSCFTD", src->frameTransDelay);
      PYLONC_CHECK_ERROR (src, res);
    }
    else{
      GST_ERROR_OBJECT (src,"This camera doesn't support changin frameTransDelay.");
      goto error;
    }
  } else {
      GST_DEBUG_OBJECT (src, "Using camera default frameTransDelay");
  }
  return TRUE;

error:
  return FALSE;  
}

static gboolean gst_pylonsrc_set_bandwidthReserve (GstPylonSrc * src)
{
 GENAPIC_RESULT res;

  if (src->bandwidthReserve >= 0 ) {
    if (FEATURE_SUPPORTED ("GevSCBWR")) {
      GST_DEBUG_OBJECT (src, "Setting bandwidthReserve to %d", src->bandwidthReserve);
      res = PylonDeviceSetIntegerFeature (src->deviceHandle,"GevSCBWR", src->bandwidthReserve);
      PYLONC_CHECK_ERROR (src, res);
    }
    else{
      GST_ERROR_OBJECT (src,"This camera doesn't support changin bandwidthReserve.");
      goto error;
    }
  } else {
      GST_DEBUG_OBJECT (src, "Using camera default bandwidthReserve");
  }
  return TRUE;

error:
  return FALSE;  
}

static gboolean gst_pylonsrc_set_bandwidthReserveAcc (GstPylonSrc * src)
{
 GENAPIC_RESULT res;

  if (src->bandwidthReserveAcc > 0 ) {
    if (FEATURE_SUPPORTED ("GevSCBWR")) {
      GST_DEBUG_OBJECT (src, "Setting bandwidthReserveAcc to %d", src->bandwidthReserveAcc);
      res = PylonDeviceSetIntegerFeature (src->deviceHandle,"GevSCBWR", src->bandwidthReserveAcc);
      PYLONC_CHECK_ERROR (src, res);
    }
    else{
      GST_ERROR_OBJECT (src,"This camera doesn't support changin bandwidthReserveAcc.");
      goto error;
    }
  } else {
      GST_DEBUG_OBJECT (src, "Using camera default bandwidthReserveAcc");
  }
  return TRUE;

error:
  return FALSE;  
}
static gboolean
gst_pylonsrc_configure_start_acquisition (GstPylonSrc * src)
{
  GENAPIC_RESULT res;
  gint i;
  size_t num_streams;

  if (!gst_pylonsrc_set_offset (src) ||
      !gst_pylonsrc_set_reverse (src) ||
      !gst_pylonsrc_set_pixel_format (src) ||
      !gst_pylonsrc_set_test_image (src) ||
      !gst_pylonsrc_set_packetsize(src) ||
      !gst_pylonsrc_set_readout (src) ||
      !gst_pylonsrc_set_bandwidth (src) ||
      !gst_pylonsrc_set_framerate (src) ||
      !gst_pylonsrc_set_frameTransDelay(src) ||
      !gst_pylonsrc_set_bandwidthReserveAcc(src) ||
      !gst_pylonsrc_set_bandwidthReserve(src) ||
      !gst_pylonsrc_set_interPacketDelay(src) ||
      !gst_pylonsrc_set_lightsource (src) ||
      !gst_pylonsrc_set_auto_exp_gain_wb (src) ||
      !gst_pylonsrc_set_color (src) ||
      !gst_pylonsrc_set_exposure_gain_level (src) ||
      !gst_pylonsrc_set_pgi (src) || 
      !gst_pylonsrc_set_trigger (src))
    goto error;

  // Create a stream grabber
  res =
      PylonDeviceGetNumStreamGrabberChannels (src->deviceHandle, &num_streams);
  PYLONC_CHECK_ERROR (src, res);
  if (num_streams < 1) {
    GST_ERROR_OBJECT (src,
        "The transport layer doesn't support image streams.");
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Transport layer error"),
        ("The system does not support image streams."));
    goto error;
  }
  // Open the stream grabber for the first channel
  res = PylonDeviceGetStreamGrabber (src->deviceHandle, 0, &src->streamGrabber);
  PYLONC_CHECK_ERROR (src, res);
  res = PylonStreamGrabberOpen (src->streamGrabber);
  PYLONC_CHECK_ERROR (src, res);

  // Get the wait object
  res = PylonStreamGrabberGetWaitObject (src->streamGrabber, &src->waitObject);
  PYLONC_CHECK_ERROR (src, res);

  // Get the size of each frame
  res =
      PylonDeviceGetIntegerFeatureInt32 (src->deviceHandle, "PayloadSize",
      &src->payloadSize);
  PYLONC_CHECK_ERROR (src, res);

  // Allocate the memory for the frame payloads
  for (i = 0; i < NUM_CAPTURE_BUFFERS; ++i) {
    src->buffers[i] = (unsigned char *) malloc (src->payloadSize);
    if (NULL == src->buffers[i]) {
      GST_ERROR_OBJECT (src, "Memory allocation error.");
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Memory allocation error"),
          ("Couldn't allocate memory."));
      goto error;
    }
  }

  // Define buffers 
  res =
      PylonStreamGrabberSetMaxNumBuffer (src->streamGrabber,
      NUM_CAPTURE_BUFFERS);
  PYLONC_CHECK_ERROR (src, res);
  res =
      PylonStreamGrabberSetMaxBufferSize (src->streamGrabber, src->payloadSize);
  PYLONC_CHECK_ERROR (src, res);

  // Prepare the camera for grabbing
  res = PylonStreamGrabberPrepareGrab (src->streamGrabber);
  PYLONC_CHECK_ERROR (src, res);

  for (i = 0; i < NUM_CAPTURE_BUFFERS; ++i) {
    res =
        PylonStreamGrabberRegisterBuffer (src->streamGrabber, src->buffers[i],
        src->payloadSize, &src->bufferHandle[i]);
    PYLONC_CHECK_ERROR (src, res);
  }

  for (i = 0; i < NUM_CAPTURE_BUFFERS; ++i) {
    res =
        PylonStreamGrabberQueueBuffer (src->streamGrabber, src->bufferHandle[i],
        (void *) i);
    PYLONC_CHECK_ERROR (src, res);
  }

  // Output the bandwidth the camera will actually use [B/s]
  if (FEATURE_SUPPORTED ("DeviceLinkCurrentThroughput")
      && FEATURE_SUPPORTED ("DeviceLinkSpeed")) {
    int64_t throughput = 0, linkSpeed = 0;

    res =
        PylonDeviceGetIntegerFeature (src->deviceHandle,
        "DeviceLinkCurrentThroughput", &throughput);
    PYLONC_CHECK_ERROR (src, res);
    res =
        PylonDeviceGetIntegerFeature (src->deviceHandle, "DeviceLinkSpeed",
        &linkSpeed);
    PYLONC_CHECK_ERROR (src, res);

    if (throughput > linkSpeed) {
      GST_ERROR_OBJECT (src,
          "Not enough bandwidth for the specified parameters.");
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("USB3 error"),
          ("Not enough bandwidth."));
      goto error;
    }

    GST_DEBUG_OBJECT (src,
        "With current settings the camera requires %d/%d B/s (%.1lf out of %.1lf MB/s) of bandwidth.",
        (gint) throughput, (gint) linkSpeed, (double) throughput / 1000000,
        (double) linkSpeed / 1000000);
  } else {
    GST_WARNING_OBJECT (src, "Couldn't determine link speed.");
  }

  // Output sensor readout time [us]
  if (FEATURE_SUPPORTED ("SensorReadoutTime")) {
    double readoutTime = 0.0;

    res =
        PylonDeviceGetFloatFeature (src->deviceHandle, "SensorReadoutTime",
        &readoutTime);
    PYLONC_CHECK_ERROR (src, res);

    GST_DEBUG_OBJECT (src,
        "With these settings it will take approximately %.0lf microseconds to grab each frame.",
        readoutTime);
  } else {
    GST_WARNING_OBJECT (src, "Couldn't determine sensor readout time.");
  }

  // Output final frame rate [Hz]
  if (FEATURE_SUPPORTED ("ResultingFrameRate")) {
    double frameRate = 0.0;

    res =
        PylonDeviceGetFloatFeature (src->deviceHandle, "ResultingFrameRate",
        &frameRate);
    PYLONC_CHECK_ERROR (src, res);

    GST_DEBUG_OBJECT (src, "The resulting framerate is %.0lf fps.", frameRate);
    GST_DEBUG_OBJECT (src,
        "Each frame is %d bytes big (%.1lf MB). That's %.1lfMB/s.",
        src->payloadSize, (double) src->payloadSize / 1000000,
        (src->payloadSize * frameRate) / 1000000);
  } else {
    GST_WARNING_OBJECT (src, "Couldn't determine the resulting framerate.");
  }

  // Tell the camera to start recording
  res =
      PylonDeviceExecuteCommandFeature (src->deviceHandle, "AcquisitionStart");
  PYLONC_CHECK_ERROR (src, res);
  if (!src->continuousMode) {
    res =
        PylonDeviceExecuteCommandFeature (src->deviceHandle, "TriggerSoftware");
    PYLONC_CHECK_ERROR (src, res);
  }
  src->failedFrames = 0;
  src->frameNumber = 0;

  GST_DEBUG_OBJECT (src, "Initialised successfully.");
  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_start (GstBaseSrc * bsrc)
{
  GstPylonSrc *src = GST_PYLONSRC (bsrc);

  if (PylonInitialize () != 0) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to initialise the camera"),
        ("Pylon library initialization failed"));
    goto error;
  }

  if (!gst_pylonsrc_select_device (src) ||
      !gst_pylonsrc_connect_device (src) || !gst_pylonsrc_set_resolution (src))
    goto error;

  src->caps = gst_pylonsrc_get_supported_caps (src);

  return TRUE;

error:
  pylonc_disconnect_camera (src);
  return FALSE;
}

typedef struct
{
  GstPylonSrc *src;
  PYLON_STREAMBUFFER_HANDLE buffer_handle;
} VideoFrame;


static void
video_frame_free (void *data)
{
  VideoFrame *frame = (VideoFrame *) data;
  GstPylonSrc *src = frame->src;
  GENAPIC_RESULT res;

  // Release frame's memory
  res =
      PylonStreamGrabberQueueBuffer (src->streamGrabber, frame->buffer_handle,
      NULL);
  PYLONC_CHECK_ERROR (src, res);
  g_free (frame);

error:
  return;
}

static GstFlowReturn
gst_pylonsrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstPylonSrc *src = GST_PYLONSRC (psrc);
  GENAPIC_RESULT res;
  PylonGrabResult_t grabResult;
  _Bool bufferReady;

  if (!src->acquisition_configured) {
    if (!gst_pylonsrc_configure_start_acquisition (src))
      goto error;
    src->acquisition_configured = TRUE;
  }
  // Wait for the buffer to be filled  (up to n ms). Can fail on large frames if timeout set too low.
  res = PylonWaitObjectWait (src->waitObject, src->grabtimeout, &bufferReady);
  PYLONC_CHECK_ERROR (src, res);
  if (!bufferReady) {
    GST_ERROR_OBJECT (src,
        "Camera couldn't prepare the buffer in time. Probably dead.");
    goto error;
  }

  res =
      PylonStreamGrabberRetrieveResult (src->streamGrabber, &grabResult,
      &bufferReady);
  PYLONC_CHECK_ERROR (src, res);
  if (!bufferReady) {
    GST_ERROR_OBJECT (src,
        "Couldn't get a buffer from the camera. Basler said this should be impossible. You just proved them wrong. Congratulations!");
    goto error;
  }

  if (!src->continuousMode) {
    // Trigger the next picture while we process this one
    if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "AcquisitionStatus")) {
      _Bool isReady = FALSE;
      do {
        res =
            PylonDeviceGetBooleanFeature (src->deviceHandle,
            "AcquisitionStatus", &isReady);
        PYLONC_CHECK_ERROR (src, res);
      } while (!isReady);
    }
    res =
        PylonDeviceExecuteCommandFeature (src->deviceHandle, "TriggerSoftware");
    PYLONC_CHECK_ERROR (src, res);
  }
  // Process the current buffer
  if (grabResult.Status == Grabbed || src->failedFrames < src->failrate) {
    VideoFrame *vf = (VideoFrame *) g_malloc0 (sizeof (VideoFrame));

    *buf =
        gst_buffer_new_wrapped_full ((GstMemoryFlags) GST_MEMORY_FLAG_READONLY,
        (gpointer) grabResult.pBuffer, src->payloadSize, 0, src->payloadSize,
        vf, (GDestroyNotify) video_frame_free);

    vf->buffer_handle = grabResult.hBuffer;
    vf->src = src;

    if (grabResult.Status != Grabbed ){
      src->failedFrames += 1;
      GST_WARNING_OBJECT (src,"Failed capture count=%d. Status=%d, ErrorCode=%d",src->failedFrames,grabResult.Status, grabResult.ErrorCode);      
    }
    else src->failedFrames = 0;
  } else {
      GST_ERROR_OBJECT (src, "Error in the image processing loop. Status=%d, ErrorCode=%d", grabResult.Status, grabResult.ErrorCode);
      goto error;
  }

  // Set frame offset
  GST_BUFFER_OFFSET (*buf) = src->frameNumber;
  src->frameNumber += 1;
  GST_BUFFER_OFFSET_END (*buf) = src->frameNumber;
  
  return GST_FLOW_OK;
error:
  return GST_FLOW_ERROR;
}

static gboolean
gst_pylonsrc_stop (GstBaseSrc * bsrc)
{
  GstPylonSrc *src = GST_PYLONSRC (bsrc);
  GST_DEBUG_OBJECT (src, "stop");

  pylonc_disconnect_camera (src);

  return TRUE;
}

void
gst_pylonsrc_dispose (GObject * object)
{
  GstPylonSrc *src = GST_PYLONSRC (object);
  GST_DEBUG_OBJECT (src, "dispose");
  G_OBJECT_CLASS (gst_pylonsrc_parent_class)->dispose (object);
}

void
gst_pylonsrc_finalize (GObject * object)
{
  GstPylonSrc *src = GST_PYLONSRC (object);
  GST_DEBUG_OBJECT (src, "finalize");

  pylonc_terminate ();

  G_OBJECT_CLASS (gst_pylonsrc_parent_class)->finalize (object);
}

/* PylonC functions */
void
pylonc_terminate ()
{
  PylonTerminate ();
}

void
pylonc_disconnect_camera (GstPylonSrc * src)
{
  if (src->deviceConnected) {
    if (strcmp (src->reset, "after") == 0) {
      pylonc_reset_camera (src);
    }

    PylonDeviceClose (src->deviceHandle);
    PylonDestroyDevice (src->deviceHandle);
    src->deviceConnected = FALSE;
    GST_DEBUG_OBJECT (src, "Camera disconnected.");
  }
}

_Bool
pylonc_reset_camera (GstPylonSrc * src)
{
  GENAPIC_RESULT res;
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "DeviceReset")) {
    GST_DEBUG_OBJECT (src, "Resetting device...");
    res = PylonDeviceExecuteCommandFeature (src->deviceHandle, "DeviceReset");
    PYLONC_CHECK_ERROR (src, res);
    return TRUE;
  }

error:
  GST_ERROR_OBJECT (src, "ERROR: COULDN'T RESET THE DEVICE.");
  return FALSE;
}

_Bool
pylonc_connect_camera (GstPylonSrc * src)
{
  GENAPIC_RESULT res;
  GST_DEBUG_OBJECT (src, "Connecting to the camera (index=%d)...",
      src->cameraId);

  res = PylonCreateDeviceByIndex (src->cameraId, &src->deviceHandle);
  PYLONC_CHECK_ERROR (src, res);

  res =
      PylonDeviceOpen (src->deviceHandle,
      PYLONC_ACCESS_MODE_CONTROL | PYLONC_ACCESS_MODE_STREAM);
  PYLONC_CHECK_ERROR (src, res);

  src->deviceConnected = TRUE;
  return TRUE;

error:
  return FALSE;
}

void
pylonc_print_camera_info (GstPylonSrc * src, PYLON_DEVICE_HANDLE deviceHandle,
    int deviceId)
{
  char name[256];
  char serial[256];
  char id[256];
  size_t siz = 0;
  GENAPIC_RESULT res;

  if (PylonDeviceFeatureIsReadable (deviceHandle, "DeviceModelName")
      && PylonDeviceFeatureIsReadable (deviceHandle, "DeviceSerialNumber")) {
    siz = sizeof (name);
    res =
        PylonDeviceFeatureToString (deviceHandle, "DeviceModelName", name,
        &siz);
    PYLONC_CHECK_ERROR (src, res);

    siz = sizeof (serial);
    res =
        PylonDeviceFeatureToString (deviceHandle, "DeviceSerialNumber", serial,
        &siz);
    PYLONC_CHECK_ERROR (src, res);

    if (PylonDeviceFeatureIsReadable (deviceHandle, "DeviceUserID")) {
      siz = sizeof (id);
      res = PylonDeviceFeatureToString (deviceHandle, "DeviceUserID", id, &siz);
      PYLONC_CHECK_ERROR (src, res);
    }

    if (id[0] == (char) 0) {
      g_strdup ("None");
    }

    if (src->cameraId != deviceId) {    // We're listing cameras
      GST_LOG_OBJECT (src,
          "ID:%i, Name:%s, Serial No:%s, Status: Available. Custom ID: %s",
          deviceId, name, serial, id);
    } else {                    // We've connected to a camera
      GST_LOG_OBJECT (src,
          "Status: Using camera \"%s\" (serial number: %s, id: %i). Custom ID: %s",
          name, serial, deviceId, id);
    }
  } else {
  error:
    GST_ERROR_OBJECT (src,
        "ID:%i, Status: Could not properly identify connected camera, the camera might not be compatible with this plugin.",
        deviceId);
  }
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    pylon,
    "Basler Pylon video elements",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
