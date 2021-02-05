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
#include <math.h>

#include "common/genicampixelformat.h"

static int plugin_counter = 0;

int
gst_pylonsrc_ref_pylon_environment ()
{
  if (plugin_counter == 0) {
    GST_DEBUG ("pylonsrc: Initializing Pylon environment");
    if (PylonInitialize () != GENAPI_E_OK) {
      return -1;
    }
  }
  return ++plugin_counter;
}

int
gst_pylonsrc_unref_pylon_environment ()
{
  if (plugin_counter == 1) {
    GST_DEBUG ("pylonsrc: Terminating Pylon environment");
    if (PylonTerminate () != GENAPI_E_OK) {
      return -1;
    }
  }

  if (plugin_counter > 0) {
    plugin_counter--;
  }

  return plugin_counter;
}

/* PylonC */
_Bool pylonc_reset_camera (GstPylonSrc * src);
_Bool pylonc_connect_camera (GstPylonSrc * src);
void pylonc_disconnect_camera (GstPylonSrc * src);
void pylonc_print_camera_info (GstPylonSrc * src,
    PYLON_DEVICE_HANDLE deviceHandle, int deviceId);

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
typedef enum _GST_PYLONSRC_PROP
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
  PROP_COLORADJUSTMENTENABLE,
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
  PROP_CONFIGFILE,
  PROP_IGNOREDEFAULTS,

  PROP_NUM_PROPERTIES           // Yes, there is PROP_0 that represent nothing, so actually there are (PROP_NUMPROPS - 1) properties.
      // But this way you can intuitively access propFlags[] by index
} GST_PYLONSRC_PROP;

G_STATIC_ASSERT ((int) PROP_NUM_PROPERTIES == GST_PYLONSRC_NUM_PROPS);

typedef enum _GST_PYLONSRC_AUTOFEATURE
{
  AUTOF_GAIN,
  AUTOF_EXPOSURE,
  AUTOF_WHITEBALANCE,

  AUTOF_NUM_FEATURES,

  AUTOF_NUM_LIMITED = 2
} GST_PYLONSRC_AUTOFEATURE;

G_STATIC_ASSERT ((int) AUTOF_NUM_FEATURES == GST_PYLONSRC_NUM_AUTO_FEATURES);
G_STATIC_ASSERT ((int) AUTOF_NUM_LIMITED == GST_PYLONSRC_NUM_LIMITED_FEATURES);

typedef struct _DemosaicingStrings {
  const char* name;
  const char* on;
  const char* off;
} DemosaicingStrings;

static const DemosaicingStrings featDemosaicing[2] = { 
  { "DemosaicingMode", "BaslerPGI", "Simple" },
  { "PgiMode", "On", "Off" }
};
static const char *const featAutoFeature[AUTOF_NUM_FEATURES] =
    { "GainAuto", "ExposureAuto", "BalanceWhiteAuto" };
static const GST_PYLONSRC_PROP propAutoFeature[AUTOF_NUM_FEATURES] =
    { PROP_AUTOGAIN, PROP_AUTOEXPOSURE, PROP_AUTOWHITEBALANCE };
// Yes,there is no "WhiteBalance" feature, it is only used for logging
static const char *const featManualFeature[AUTOF_NUM_FEATURES] =
    { "Gain", "ExposureTime", "WhiteBalance" };
static const char *const featManualFeatureAlias[AUTOF_NUM_LIMITED] =
    { "GainRaw", "ExposureTimeAbs" };
static const char *const featLimitedLower[AUTOF_NUM_LIMITED] =
    { "AutoGainLowerLimit", "AutoExposureTimeLowerLimit" };
static const char *const featLimitedUpper[AUTOF_NUM_LIMITED] =
    { "AutoGainUpperLimit", "AutoExposureTimeUpperLimit" };
static const char *const featLimitedLowerAlias[AUTOF_NUM_LIMITED] =
    { "AutoGainRawLowerLimit", "AutoExposureTimeAbsLowerLimit" };
static const char *const featLimitedUpperAlias[AUTOF_NUM_LIMITED] =
    { "AutoGainRawUpperLimit", "AutoExposureTimeAbsUpperLimit" };
static const GST_PYLONSRC_PROP propManualFeature[AUTOF_NUM_LIMITED] =
    { PROP_GAIN, PROP_EXPOSURE };
static const GST_PYLONSRC_PROP propLimitedLower[AUTOF_NUM_LIMITED] =
    { PROP_GAINLOWERLIMIT, PROP_AUTOEXPOSURELOWERLIMIT };
static const GST_PYLONSRC_PROP propLimitedUpper[AUTOF_NUM_LIMITED] =
    { PROP_GAINUPPERLIMIT, PROP_AUTOEXPOSUREUPPERLIMIT };

typedef enum _GST_PYLONSRC_AXIS
{
  AXIS_X,
  AXIS_Y
} GST_PYLONSRC_AXIS;

static const GST_PYLONSRC_AXIS otherAxis[2] = { AXIS_Y, AXIS_X };

static const char *const featOffset[2] = { "OffsetX", "OffsetY" };
static const char *const featCenter[2] = { "CenterX", "CenterY" };
static const char *const featCenterAlias[2] = { "BslCenterX", "BslCenterY" };
static const GST_PYLONSRC_PROP propOffset[2] = { PROP_OFFSETX, PROP_OFFSETY };
static const GST_PYLONSRC_PROP propCenter[2] = { PROP_CENTERX, PROP_CENTERY };

static const char *const featReverse[2] = { "ReverseX", "ReverseY" };
static const GST_PYLONSRC_PROP propReverse[2] = { PROP_FLIPX, PROP_FLIPY };

static const GST_PYLONSRC_PROP propBinning[2] =
    { PROP_BINNINGH, PROP_BINNINGV };
static const GST_PYLONSRC_PROP propSize[2] = { PROP_WIDTH, PROP_HEIGHT };
static const char *const featBinning[2] =
    { "BinningHorizontal", "BinningVertical" };
static const char *const featSize[2] = { "Width", "Height" };
static const char *const featMaxSize[2] = { "WidthMax", "HeightMax" };

static const char *const featTransform[3][3] = {
  {"Gain00", "Gain01", "Gain02"},
  {"Gain10", "Gain11", "Gain12"},
  {"Gain20", "Gain21", "Gain22"}
};

static const double defaultTransform[3][3] = {
  {1.4375, -0.3125, -0.125},
  {-0.28125, 1.75, -0.46875},
  {0.0625, -0.8125, 1.75}
};

static const GST_PYLONSRC_PROP propTransform[3][3] = {
  {PROP_TRANSFORMATION00, PROP_TRANSFORMATION01, PROP_TRANSFORMATION02},
  {PROP_TRANSFORMATION10, PROP_TRANSFORMATION11, PROP_TRANSFORMATION12},
  {PROP_TRANSFORMATION20, PROP_TRANSFORMATION21, PROP_TRANSFORMATION22}
};

typedef enum _GST_PYLONSRC_COLOUR
{
  COLOUR_RED,
  COLOUR_GREEN,
  COLOUR_BLUE,
  COLOUR_CYAN,
  COLOUR_MAGENTA,
  COLOUR_YELLOW
} GST_PYLONSRC_COLOUR;

static const char *const featColour[6] =
    { "Red", "Green", "Blue", "Cyan", "Magenta", "Yellow" };
static const GST_PYLONSRC_PROP propColourBalance[3] =
    { PROP_BALANCERED, PROP_BALANCEGREEN, PROP_BALANCEBLUE };
static const GST_PYLONSRC_PROP propColourHue[6] =
    { PROP_COLORREDHUE, PROP_COLORGREENHUE, PROP_COLORBLUEHUE,
  PROP_COLORCYANHUE, PROP_COLORMAGENTAHUE, PROP_COLORYELLOWHUE
};

static const GST_PYLONSRC_PROP propColourSaturation[6] =
    { PROP_COLORREDSATURATION, PROP_COLORGREENSATURATION,
  PROP_COLORBLUESATURATION,
  PROP_COLORCYANSATURATION, PROP_COLORMAGENTASATURATION,
  PROP_COLORYELLOWSATURATION
};

static inline const char *
boolalpha (_Bool arg)
{
  return arg ? "True" : "False";
}

static inline void
ascii_strdown (gchar * *str, gssize len)
{
  gchar *temp = g_ascii_strdown (*str, len);
  g_free (*str);
  *str = temp;
}

#define DEFAULT_PROP_CAMERA                           0
#define DEFAULT_PROP_SIZE                             0
#define DEFAULT_PROP_BINNING                          1
#define DEFAULT_PROP_LIMITBANDWIDTH                   TRUE
#define DEFAULT_PROP_MAXBANDWIDTH                     0
#define DEFAULT_PROP_SENSORREADOUTMODE                "normal"
#define DEFAULT_PROP_ACQUISITIONFRAMERATEENABLE       FALSE
#define DEFAULT_PROP_FPS                              0.0
#define DEFAULT_PROP_LIGHTSOURCE                      "5000k"
#define DEFAULT_PROP_AUTOFEATURE                      "off"
#define DEFAULT_PROP_LIMITED_MANUAL                   0.0
#define DEFAULT_PROP_BALANCE                          1.0
#define DEFAULT_PROP_HUE                              0.0
#define DEFAULT_PROP_SATURATION                       0.0
#define DEFAULT_PROP_BLACKLEVEL                       0.0
#define DEFAULT_PROP_GAMMA                            1.0
#define DEFAULT_PROP_RESET                            "off"
#define DEFAULT_PROP_TESTIMAGE                        0
#define DEFAULT_PROP_CONTINUOUSMODE                   TRUE
#define DEFAULT_PROP_PIXEL_FORMAT                     "auto"
#define DEFAULT_PROP_USERID                           ""
#define DEFAULT_PROP_BASLERDEMOSAICING                FALSE
#define DEFAULT_PROP_DEMOSAICINGNOISEREDUCTION        0.0
#define DEFAULT_PROP_DEMOSAICINGSHARPNESSENHANCEMENT  1.0
#define DEFAULT_PROP_OFFSET                           0
#define DEFAULT_PROP_CENTER                           FALSE
#define DEFAULT_PROP_FLIP                             FALSE
#define DEFAULT_PROP_AUTOEXPOSURELOWERLIMIT           105.0
#define DEFAULT_PROP_AUTOEXPOSUREUPPERLIMIT           1000000.0
#define DEFAULT_PROP_GAINLOWERLIMIT                   0.0
#define DEFAULT_PROP_GAINUPPERLIMIT                   12.00921
#define DEFAULT_PROP_AUTOBRIGHTNESSTARGET             0.50196
#define DEFAULT_PROP_AUTOPROFILE                      "gain"
#define DEFAULT_PROP_TRANSFORMATIONSELECTOR           "RGBRGB"
#define DEFAULT_PROP_CONFIGFILE                       ""
#define DEFAULT_PROP_IGNOREDEFAULTS                   FALSE
#define DEFAULT_PROP_COLORADJUSTMENTENABLE            TRUE

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
          0, 100, DEFAULT_PROP_CAMERA,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_int ("height", "height",
          "(Pixels) The height of the picture. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          0, 10000, DEFAULT_PROP_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_int ("width", "width",
          "(Pixels) The width of the picture. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          0, 10000, DEFAULT_PROP_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BINNINGH,
      g_param_spec_int ("binningh", "Horizontal binning",
          "(Pixels) The number of pixels to be binned in horizontal direction. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          1, 6, DEFAULT_PROP_BINNING,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BINNINGV,
      g_param_spec_int ("binningv", "Vertical binning",
          "(Pixels) The number of pixels to be binned in vertical direction. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          1, 6, DEFAULT_PROP_BINNING,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_LIMITBANDWIDTH,
      g_param_spec_boolean ("limitbandwidth", "Link Throughput limit mode",
          "(true/false) Bandwidth limit mode. Disabling this will potentially allow the camera to reach higher frames per second, but can potentially damage your camera. Use with caution. Running the plugin without specifying this parameter will reset the value stored on the camera to `true`.",
          DEFAULT_PROP_LIMITBANDWIDTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_MAXBANDWIDTH,
      g_param_spec_int ("maxbandwidth", "Maximum bandwidth",
          "(Bytes per second) This property sets the maximum bandwidth the camera can use. The camera will only use as much as it needs for the specified resolution and framerate. This setting will have no effect if limitbandwidth is set to off. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          0, 999999999, DEFAULT_PROP_MAXBANDWIDTH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_SENSORREADOUTMODE,
      g_param_spec_string ("sensorreadoutmode", "Sensor readout mode",
          "(normal/fast) This property changes the sensor readout mode. Fast will allow for faster framerates, but might cause quality loss. It might be required to either increase max bandwidth or disabling bandwidth limiting for this to cause any noticeable change. Running the plugin without specifying this parameter will reset the value stored on the camera to \"normal\".",
          DEFAULT_PROP_SENSORREADOUTMODE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class,
      PROP_ACQUISITIONFRAMERATEENABLE,
      g_param_spec_boolean ("acquisitionframerateenable", "Custom FPS mode",
          "(true/false) Enables the use of custom fps values. Will be set to true if the fps poperty is set. Running the plugin without specifying this parameter will reset the value stored on the camera to false.",
          DEFAULT_PROP_ACQUISITIONFRAMERATEENABLE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FPS,
      g_param_spec_double ("fps", "Framerate",
          "(Frames per second) Sets the framerate of the video coming from the camera. Setting the value too high might cause the plugin to crash. Note that if your pipeline proves to be too much for your computer then the resulting video won't be in the resolution you set. Setting this parameter will set acquisitionframerateenable to true. The value of this parameter will be saved to the camera, but it will have no effect unless either this or the acquisitionframerateenable parameters are set. Reconnect the camera or use the reset parameter to reset.",
          0.0, 1024.0, DEFAULT_PROP_FPS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_LIGHTSOURCE,
      g_param_spec_string ("lightsource", "Lightsource preset",
          "(off, 2800k, 5000k, 6500k) Changes the colour balance settings to ones defined by presests (if other values are supported by camera try setting them as specified in Basler documentation. E.g. \"Tungsten\" or \"Custom\"). Just pick one that's closest to your environment's lighting. Running the plugin without specifying this parameter will reset the value stored on the camera to \"5000k\"",
          DEFAULT_PROP_LIGHTSOURCE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOEXPOSURE,
      g_param_spec_string ("autoexposure", "Automatic exposure setting",
          "(off, once, continuous) Controls whether or not the camera will try to adjust the exposure settings. Setting this parameter to anything but \"off\" will override the exposure parameter. Running the plugin without specifying this parameter will reset the value stored on the camera to \"off\"",
          DEFAULT_PROP_AUTOFEATURE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_EXPOSURE,
      g_param_spec_double ("exposure", "Exposure",
          "(Microseconds) Exposure time for the camera in microseconds. Will only have an effect if autoexposure is set to off (default). Higher numbers will cause lower frame rate. Note that the camera will remember this setting, and will use values from the previous runs if you relaunch without specifying this parameter. Reconnect the camera or use the reset parameter to reset.",
          0.0, 1000000.0, DEFAULT_PROP_LIMITED_MANUAL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOWHITEBALANCE,
      g_param_spec_string ("autowhitebalance", "Automatic colour balancing",
          "(off, once, continuous) Controls whether or not the camera will try to adjust the white balance settings. Setting this parameter to anything but \"off\" will override the exposure parameter. Running the plugin without specifying this parameter will reset the value stored on the camera to \"off\"",
          DEFAULT_PROP_AUTOFEATURE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BALANCERED,
      g_param_spec_double ("balancered", "Red balance",
          "Specifies the red colour balance. the autowhitebalance must be set to \"off\" for this property to have any effect. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 15.9, DEFAULT_PROP_BALANCE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BALANCEGREEN,
      g_param_spec_double ("balancegreen", "Green balance",
          "Specifies the green colour balance. the autowhitebalance must be set to \"off\" for this property to have any effect. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 15.9, DEFAULT_PROP_BALANCE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BALANCEBLUE,
      g_param_spec_double ("balanceblue", "Blue balance",
          "Specifies the blue colour balance. the autowhitebalance must be set to \"off\" for this property to have any effect. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 15.9, DEFAULT_PROP_BALANCE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORREDHUE,
      g_param_spec_double ("colorredhue", "Red's hue",
          "Specifies the red colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, DEFAULT_PROP_HUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORREDSATURATION,
      g_param_spec_double ("colorredsaturation", "Red's saturation",
          "Specifies the red colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, DEFAULT_PROP_SATURATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORYELLOWHUE,
      g_param_spec_double ("coloryellowhue", "Yellow's hue",
          "Specifies the yellow colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, DEFAULT_PROP_HUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORYELLOWSATURATION,
      g_param_spec_double ("coloryellowsaturation", "Yellow's saturation",
          "Specifies the yellow colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, DEFAULT_PROP_SATURATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORGREENHUE,
      g_param_spec_double ("colorgreenhue", "Green's hue",
          "Specifies the green colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, DEFAULT_PROP_HUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORGREENSATURATION,
      g_param_spec_double ("colorgreensaturation", "Green's saturation",
          "Specifies the green colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, DEFAULT_PROP_SATURATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORCYANHUE,
      g_param_spec_double ("colorcyanhue", "Cyan's hue",
          "Specifies the cyan colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, DEFAULT_PROP_HUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORCYANSATURATION,
      g_param_spec_double ("colorcyansaturation", "Cyan's saturation",
          "Specifies the cyan colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, DEFAULT_PROP_SATURATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORBLUEHUE,
      g_param_spec_double ("colorbluehue", "Blue's hue",
          "Specifies the blue colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, DEFAULT_PROP_HUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORBLUESATURATION,
      g_param_spec_double ("colorbluesaturation", "Blue's saturation",
          "Specifies the blue colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, DEFAULT_PROP_SATURATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORMAGENTAHUE,
      g_param_spec_double ("colormagentahue", "Magenta's hue",
          "Specifies the magenta colour's hue. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          -4.0, 3.9, DEFAULT_PROP_HUE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORMAGENTASATURATION,
      g_param_spec_double ("colormagentasaturation", "Magenta's saturation",
          "Specifies the magenta colour's saturation. Note that the this value gets saved on the camera, and running this plugin again without specifying this value will cause the previous value being used. Use the reset parameter or reconnect the camera to reset.",
          0.0, 1.9, DEFAULT_PROP_SATURATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_COLORADJUSTMENTENABLE,
      g_param_spec_boolean ("coloradjustment", "Enable color adjustment",
          "(true/false) On ace classic/U/L GigE Cameras enables/disables hue and saturation adjustments. Enabled implicitly if any hue or saturation property is set",
          DEFAULT_PROP_COLORADJUSTMENTENABLE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOGAIN,
      g_param_spec_string ("autogain", "Automatic gain",
          "(off, once, continuous) Controls whether or not the camera will try to adjust the gain settings. Setting this parameter to anything but \"off\" will override the exposure parameter. Running the plugin without specifying this parameter will reset the value stored on the camera to \"off\"",
          DEFAULT_PROP_AUTOFEATURE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GAIN,
      g_param_spec_double ("gain", "Gain",
          "(dB or raw) Sets the gain added on the camera before sending the frame to the computer. The value of this parameter will be saved to the camera, but it will be set to 0 every time this plugin is launched without specifying gain or overriden if the autogain parameter is set to anything that's not \"off\". Reconnect the camera or use the reset parameter to reset the stored value.",
          0.0, 12.0, DEFAULT_PROP_LIMITED_MANUAL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BLACKLEVEL,
      g_param_spec_double ("blacklevel", "Black Level",
          "(DN) Sets stream's black level. This parameter is processed on the camera before the picture is sent to the computer. The value of this parameter will be saved to the camera, but it will be set to 0 every time this plugin is launched without specifying this parameter. Reconnect the camera or use the reset parameter to reset the stored value.",
          0.0, 63.75, DEFAULT_PROP_BLACKLEVEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GAMMA,
      g_param_spec_double ("gamma", "Gamma",
          "Sets the gamma correction value. This parameter is processed on the camera before the picture is sent to the computer. The value of this parameter will be saved to the camera, but it will be set to 1.0 every time this plugin is launched without specifying this parameter. Reconnect the camera or use the reset parameter to reset the stored value.",
          0.0, 3.9, DEFAULT_PROP_GAMMA,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_RESET,
      g_param_spec_string ("reset", "Camera reset settings",
          "(off, before, after). Controls whether or when the camera's settings will be reset. Setting this to \"before\" will wipe the settings before the camera initialisation begins. Setting this to \"after\" will reset the device once the pipeline closes. This can be useful for debugging or when you want to use the camera with other software that doesn't reset the camera settings before use (such as PylonViewerApp).",
          DEFAULT_PROP_RESET,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TESTIMAGE,
      g_param_spec_int ("testimage", "Test image",
          "(1-6) Specifies a test image to show instead of a video stream. Useful for debugging. Will be disabled by default.",
          0, 6, DEFAULT_PROP_TESTIMAGE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CONTINUOUSMODE,
      g_param_spec_boolean ("continuous", "Continuous mode",
          "(true/false) Used to switch between triggered and continuous mode. To switch to triggered mode this parameter has to be switched to false.",
          DEFAULT_PROP_CONTINUOUSMODE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_PIXEL_FORMAT,
      g_param_spec_string ("pixel-format", "Pixel format",
          "Force the pixel format (e.g., Mono8). Default to 'auto', which will use GStreamer negotiation.",
          DEFAULT_PROP_PIXEL_FORMAT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_USERID,
      g_param_spec_string ("userid", "Custom Device User ID",
          "(<string>) Sets the device custom id so that it can be identified later.",
          DEFAULT_PROP_USERID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BASLERDEMOSAICING,
      g_param_spec_boolean ("demosaicing", "Basler's Demosaicing mode'",
          "(true/false) Switches between simple and Basler's Demosaicing (PGI) mode. Note that this will not work if bayer output is used.",
          DEFAULT_PROP_BASLERDEMOSAICING,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class,
      PROP_DEMOSAICINGNOISEREDUCTION, g_param_spec_double ("noisereduction",
          "Noise reduction",
          "Specifies the amount of noise reduction to apply. To use this Basler's demosaicing mode must be enabled. Setting this will enable demosaicing mode.",
          0.0, 2.0, DEFAULT_PROP_DEMOSAICINGNOISEREDUCTION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class,
      PROP_DEMOSAICINGSHARPNESSENHANCEMENT,
      g_param_spec_double ("sharpnessenhancement", "Sharpness enhancement",
          "Specifies the amount of sharpness enhancement to apply. To use this Basler's demosaicing mode must be enabled. Setting this will enable demosaicing mode.",
          1.0, 3.98, DEFAULT_PROP_DEMOSAICINGSHARPNESSENHANCEMENT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_OFFSETX,
      g_param_spec_int ("offsetx", "horizontal offset",
          "(0-10000) Determines the vertical offset. Note that the maximum offset value is calculated during initialisation, and will not be shown in this output.",
          0, 10000, DEFAULT_PROP_OFFSET,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_OFFSETY,
      g_param_spec_int ("offsety", "vertical offset",
          "(0-10000) Determines the vertical offset. Note that the maximum offset value is calculated during initialisation, and will not be shown in this output.",
          0, 10000, DEFAULT_PROP_OFFSET,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CENTERX,
      g_param_spec_boolean ("centerx", "center horizontally",
          "(true/false) Setting this will center the horizontal offset. Setting this to true this will cause the plugin to ignore offsetx value.",
          DEFAULT_PROP_CENTER,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CENTERY,
      g_param_spec_boolean ("centery", "center vertically",
          "(true/false) Setting this will center the vertical offset. Setting this to true this will cause the plugin to ignore offsetx value.",
          DEFAULT_PROP_CENTER,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FLIPX,
      g_param_spec_boolean ("flipx", "Flip horizontally",
          "(true/false) Setting this will flip the image horizontally.",
          DEFAULT_PROP_FLIP,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_FLIPY,
      g_param_spec_boolean ("flipy", "Flip vertically",
          "(true/false) Setting this will flip the image vertically.",
          DEFAULT_PROP_FLIP,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOEXPOSURELOWERLIMIT,
      g_param_spec_double ("exposurelowerlimit", "Auto exposure lower limit",
          "(105-1000000) Sets the lower limit for the auto exposure function.",
          105.0, 1000000.0, DEFAULT_PROP_AUTOEXPOSURELOWERLIMIT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOEXPOSUREUPPERLIMIT,
      g_param_spec_double ("exposureupperlimit", "Auto exposure upper limit",
          "(105-1000000) Sets the upper limit for the auto exposure function.",
          105.0, 1000000.0, DEFAULT_PROP_AUTOEXPOSUREUPPERLIMIT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GAINUPPERLIMIT,
      g_param_spec_double ("gainupperlimit", "Auto gain upper limit",
          "(0-12.00921 dB or raw) Sets the upper limit for the auto gain function.",
          0.0, 12.00921, DEFAULT_PROP_GAINUPPERLIMIT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_GAINLOWERLIMIT,
      g_param_spec_double ("gainlowerlimit", "Auto gain lower limit",
          "(0-12.00921 dB or raw) Sets the lower limit for the auto gain function.",
          0.0, 12.00921, DEFAULT_PROP_GAINLOWERLIMIT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOBRIGHTNESSTARGET,
      g_param_spec_double ("autobrightnesstarget", "Auto brightness target",
          "(0.19608-0.80392 or 50-205) Sets the brightness value the auto exposure/gain function should strive for.",
          0.19608, 0.80392, DEFAULT_PROP_AUTOBRIGHTNESSTARGET,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_AUTOPROFILE,
      g_param_spec_string ("autoprofile", "Auto function minimize profile",
          "(gain/exposure) When the auto functions are on, this determines whether to focus on minimising gain or exposure.",
          DEFAULT_PROP_AUTOPROFILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION00,
      g_param_spec_double ("transformation00",
          "Color Transformation selector 00", "Gain00 transformation selector.",
          -8.0, 7.96875, defaultTransform[0][0],
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION01,
      g_param_spec_double ("transformation01",
          "Color Transformation selector 01", "Gain01 transformation selector.",
          -8.0, 7.96875, defaultTransform[0][1],
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION02,
      g_param_spec_double ("transformation02",
          "Color Transformation selector 02", "Gain02 transformation selector.",
          -8.0, 7.96875, defaultTransform[0][2],
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION10,
      g_param_spec_double ("transformation10",
          "Color Transformation selector 10", "Gain10 transformation selector.",
          -8.0, 7.96875, defaultTransform[1][0],
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION11,
      g_param_spec_double ("transformation11",
          "Color Transformation selector 11", "Gain11 transformation selector.",
          -8.0, 7.96875, defaultTransform[1][1],
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION12,
      g_param_spec_double ("transformation12",
          "Color Transformation selector 12", "Gain12 transformation selector.",
          -8.0, 7.96875, defaultTransform[1][2],
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION20,
      g_param_spec_double ("transformation20",
          "Color Transformation selector 20", "Gain20 transformation selector.",
          -8.0, 7.96875, defaultTransform[2][0],
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION21,
      g_param_spec_double ("transformation21",
          "Color Transformation selector 21", "Gain21 transformation selector.",
          -8.0, 7.96875, defaultTransform[2][1],
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATION22,
      g_param_spec_double ("transformation22",
          "Color Transformation selector 22", "Gain22 transformation selector.",
          -8.0, 7.96875, defaultTransform[2][2],
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_TRANSFORMATIONSELECTOR,
      g_param_spec_string ("transformationselector",
          "Color Transformation Selector",
          "(RGBRGB, RGBYUV, YUVRGB) Sets the type of color transformation done by the color transformation selectors.",
          DEFAULT_PROP_TRANSFORMATIONSELECTOR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_CONFIGFILE,
      g_param_spec_string ("config-file", "Pylon Feature Stream (.pfs) file",
          "Path to PFS file containing configurations for all features. PFS file can be obtained with PylonViewer or PylonFeaturePersistenceSave() function. Configuration file is applied prior to any other properties. Using configuration file implies setting \"ignore-defaults\"",
          DEFAULT_PROP_CONFIGFILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_IGNOREDEFAULTS,
      g_param_spec_boolean ("ignore-defaults",
          "Ignore default values to other properties",
          "(true/false) Apply features only if those are set explicitly. Can only be applied on plugin startup. This property is implicitly set to true if config-file is provided. Setting this to false while config-file is set will lead to strange result and is not recommended",
          DEFAULT_PROP_IGNOREDEFAULTS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
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
  src->continuousMode = DEFAULT_PROP_CONTINUOUSMODE;
  src->limitBandwidth = DEFAULT_PROP_LIMITBANDWIDTH;
  src->setFPS = DEFAULT_PROP_ACQUISITIONFRAMERATEENABLE;
  src->demosaicing = DEFAULT_PROP_BASLERDEMOSAICING;
  src->colorAdjustment = DEFAULT_PROP_COLORADJUSTMENTENABLE;

  for (int i = 0; i < 2; i++) {
    src->binning[i] = DEFAULT_PROP_BINNING;
    src->center[i] = DEFAULT_PROP_CENTER;
    src->flip[i] = DEFAULT_PROP_FLIP;
    src->offset[i] = DEFAULT_PROP_OFFSET;
    src->size[i] = DEFAULT_PROP_SIZE;
    src->maxSize[i] = G_MAXINT;
  }

  src->cameraId = DEFAULT_PROP_CAMERA;
  src->maxBandwidth = DEFAULT_PROP_MAXBANDWIDTH;
  src->testImage = DEFAULT_PROP_TESTIMAGE;
  src->sensorMode = g_strdup (DEFAULT_PROP_SENSORREADOUTMODE);
  src->lightsource = g_strdup (DEFAULT_PROP_LIGHTSOURCE);

  for (int i = 0; i < AUTOF_NUM_FEATURES; i++) {
    src->autoFeature[i] = g_strdup (DEFAULT_PROP_AUTOFEATURE);
  }

  src->reset = g_strdup (DEFAULT_PROP_RESET);
  src->pixel_format = g_strdup (DEFAULT_PROP_PIXEL_FORMAT);
  src->userid = g_strdup (DEFAULT_PROP_USERID);
  src->autoprofile = g_strdup (DEFAULT_PROP_AUTOPROFILE);
  src->transformationselector = g_strdup (DEFAULT_PROP_TRANSFORMATIONSELECTOR);
  src->fps = DEFAULT_PROP_FPS;

  src->blacklevel = DEFAULT_PROP_BLACKLEVEL;
  src->gamma = DEFAULT_PROP_GAMMA;

  for (int i = 0; i < 3; i++) {
    src->balance[i] = DEFAULT_PROP_BALANCE;
  }

  for (int i = 0; i < 6; i++) {
    src->hue[i] = DEFAULT_PROP_HUE;
    src->saturation[i] = DEFAULT_PROP_SATURATION;
  }

  src->sharpnessenhancement = DEFAULT_PROP_DEMOSAICINGSHARPNESSENHANCEMENT;
  src->noisereduction = DEFAULT_PROP_DEMOSAICINGNOISEREDUCTION;

  for (int i = 0; i < AUTOF_NUM_LIMITED; i++) {
    src->limitedFeature[i].manual = DEFAULT_PROP_LIMITED_MANUAL;
  }

  src->limitedFeature[AUTOF_EXPOSURE].lower =
      DEFAULT_PROP_AUTOEXPOSURELOWERLIMIT;
  src->limitedFeature[AUTOF_EXPOSURE].upper =
      DEFAULT_PROP_AUTOEXPOSUREUPPERLIMIT;

  src->limitedFeature[AUTOF_GAIN].lower = DEFAULT_PROP_GAINLOWERLIMIT;
  src->limitedFeature[AUTOF_GAIN].upper = DEFAULT_PROP_GAINUPPERLIMIT;

  src->brightnesstarget = DEFAULT_PROP_AUTOBRIGHTNESSTARGET;
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      src->transformation[j][i] = defaultTransform[j][i];
    }
  }

  src->configFile = g_strdup (DEFAULT_PROP_CONFIGFILE);
  src->ignoreDefaults = DEFAULT_PROP_IGNOREDEFAULTS;

  for (int i = 0; i < PROP_NUM_PROPERTIES; i++) {
    src->propFlags[i] = GST_PYLONSRC_PROPST_DEFAULT;
  }

  // Mark this element as a live source (disable preroll)
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (src), TRUE);
}

// check if property is set explicitly
static inline _Bool
is_prop_set (const GstPylonSrc * src, GST_PYLONSRC_PROP prop)
{
  return src->propFlags[prop] == GST_PYLONSRC_PROPST_SET;
}

// check if property is already processed (value is loaded to device)
static inline _Bool
is_prop_not_set (const GstPylonSrc * src, GST_PYLONSRC_PROP prop)
{
  return src->propFlags[prop] == GST_PYLONSRC_PROPST_NOT_SET;
}

// check if property is defaulted
static inline _Bool
is_prop_default (const GstPylonSrc * src, GST_PYLONSRC_PROP prop)
{
  return src->propFlags[prop] == GST_PYLONSRC_PROPST_DEFAULT;
}

// check if property is set implicitly, i.e. set explicitly or defaulted
static inline _Bool
is_prop_implicit (const GstPylonSrc * src, GST_PYLONSRC_PROP prop)
{
  return src->propFlags[prop] != GST_PYLONSRC_PROPST_NOT_SET;
}

// mark property as processed
static inline void
reset_prop (GstPylonSrc * src, GST_PYLONSRC_PROP prop)
{
  src->propFlags[prop] = GST_PYLONSRC_PROPST_NOT_SET;
}

// Use in gst_pylonsrc_set_property to set related boolean property
static inline void
set_prop_implicitly (GObject * object, GST_PYLONSRC_PROP prop,
    GParamSpec * pspec)
{
  GstPylonSrc *src = GST_PYLONSRC (object);
  if (!is_prop_set (src, prop)) {
    GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_BOOLEAN);
    g_value_set_boolean (&val, TRUE);
    gst_pylonsrc_set_property (object, prop, &val, pspec);
    g_value_unset (&val);
  }
}

// Use in gst_pylonsrc_set_property to set related string property
static inline void
set_string_prop_implicitly (GObject * object, GST_PYLONSRC_PROP prop,
    GParamSpec * pspec, const gchar * str_val)
{
  GstPylonSrc *src = GST_PYLONSRC (object);
  if (!is_prop_set (src, prop)) {
    GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_STRING);
    g_value_set_string (&val, str_val);
    gst_pylonsrc_set_property (object, prop, &val, pspec);
    g_value_unset (&val);
  }
}

/* plugin's parameters/properties */
void
gst_pylonsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPylonSrc *src = GST_PYLONSRC (object);

  GST_DEBUG_OBJECT (src, "Setting a property: %u", property_id);

  switch (property_id) {
    case PROP_CAMERA:
      src->cameraId = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      src->size[AXIS_Y] = g_value_get_int (value);
      break;
    case PROP_WIDTH:
      src->size[AXIS_X] = g_value_get_int (value);
      break;
    case PROP_BINNINGH:
      src->binning[AXIS_X] = g_value_get_int (value);
      break;
    case PROP_BINNINGV:
      src->binning[AXIS_Y] = g_value_get_int (value);
      break;
    case PROP_OFFSETX:
      src->offset[AXIS_X] = g_value_get_int (value);
      break;
    case PROP_OFFSETY:
      src->offset[AXIS_Y] = g_value_get_int (value);
      break;
    case PROP_TESTIMAGE:
      src->testImage = g_value_get_int (value);
      break;
    case PROP_SENSORREADOUTMODE:
      g_free (src->sensorMode);
      src->sensorMode = g_value_dup_string (value);
      break;
    case PROP_LIGHTSOURCE:
      g_free (src->lightsource);
      src->lightsource = g_value_dup_string (value);
      break;
    case PROP_AUTOEXPOSURE:
      g_free (src->autoFeature[AUTOF_EXPOSURE]);
      src->autoFeature[AUTOF_EXPOSURE] = g_value_dup_string (value);
      break;
    case PROP_AUTOWHITEBALANCE:
      g_free (src->autoFeature[AUTOF_WHITEBALANCE]);
      src->autoFeature[AUTOF_WHITEBALANCE] = g_value_dup_string (value);
      break;
    case PROP_PIXEL_FORMAT:
      g_free (src->pixel_format);
      src->pixel_format = g_value_dup_string (value);
      break;
    case PROP_AUTOGAIN:
      g_free (src->autoFeature[AUTOF_GAIN]);
      src->autoFeature[AUTOF_GAIN] = g_value_dup_string (value);
      break;
    case PROP_RESET:
      g_free (src->reset);
      src->reset = g_value_dup_string (value);
      break;
    case PROP_AUTOPROFILE:
      g_free (src->autoprofile);
      src->autoprofile = g_value_dup_string (value);
      break;
    case PROP_TRANSFORMATIONSELECTOR:
      g_free (src->transformationselector);
      src->transformationselector = g_value_dup_string (value);
      break;
    case PROP_USERID:
      g_free (src->userid);
      src->userid = g_value_dup_string (value);
      break;
    case PROP_BALANCERED:
      src->balance[COLOUR_RED] = g_value_get_double (value);
      break;
    case PROP_BALANCEGREEN:
      src->balance[COLOUR_GREEN] = g_value_get_double (value);
      break;
    case PROP_BALANCEBLUE:
      src->balance[COLOUR_BLUE] = g_value_get_double (value);
      break;
    case PROP_COLORREDHUE:
      src->hue[COLOUR_RED] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORREDSATURATION:
      src->saturation[COLOUR_RED] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORYELLOWHUE:
      src->hue[COLOUR_YELLOW] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORYELLOWSATURATION:
      src->saturation[COLOUR_YELLOW] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORGREENHUE:
      src->hue[COLOUR_GREEN] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORGREENSATURATION:
      src->saturation[COLOUR_GREEN] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORCYANHUE:
      src->hue[COLOUR_CYAN] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORCYANSATURATION:
      src->saturation[COLOUR_CYAN] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORBLUEHUE:
      src->hue[COLOUR_BLUE] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORBLUESATURATION:
      src->saturation[COLOUR_BLUE] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORMAGENTAHUE:
      src->hue[COLOUR_MAGENTA] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_COLORMAGENTASATURATION:
      src->saturation[COLOUR_MAGENTA] = g_value_get_double (value);
      set_prop_implicitly (object, PROP_COLORADJUSTMENTENABLE, pspec);
      break;
    case PROP_MAXBANDWIDTH:
      src->maxBandwidth = g_value_get_int (value);
      break;
    case PROP_FLIPX:
      src->flip[AXIS_X] = g_value_get_boolean (value);
      break;
    case PROP_FLIPY:
      src->flip[AXIS_Y] = g_value_get_boolean (value);
      break;
    case PROP_CENTERX:
      src->center[AXIS_X] = g_value_get_boolean (value);
      break;
    case PROP_CENTERY:
      src->center[AXIS_Y] = g_value_get_boolean (value);
      break;
    case PROP_LIMITBANDWIDTH:
      src->limitBandwidth = g_value_get_boolean (value);
      break;
    case PROP_ACQUISITIONFRAMERATEENABLE:
      // if FPS is set, then AcquisitionFrameRateEnable is set implicitly
      // This can be overriden if needed
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
      // Setting fps implies setting AcquisitionFrameRateEnable if the latter is not set to false explicitly
      set_prop_implicitly (object, PROP_ACQUISITIONFRAMERATEENABLE, pspec);
      break;
    case PROP_EXPOSURE:
      src->limitedFeature[AUTOF_EXPOSURE].manual = g_value_get_double (value);
      // disable autoexposure unless set explicitly
      set_string_prop_implicitly (object, PROP_AUTOEXPOSURE, pspec, "off");
      break;
    case PROP_GAIN:
      src->limitedFeature[AUTOF_GAIN].manual = g_value_get_double (value);
      // disable autoexposure unless set explicitly
      set_string_prop_implicitly (object, PROP_AUTOGAIN, pspec, "off");
      break;
    case PROP_BLACKLEVEL:
      src->blacklevel = g_value_get_double (value);
      break;
    case PROP_GAMMA:
      src->gamma = g_value_get_double (value);
      break;
    case PROP_DEMOSAICINGNOISEREDUCTION:
      src->noisereduction = g_value_get_double (value);
      // Setting NoiseReduction implies setting Basler Demosaising if the latter is not set to Simple explicitly
      set_prop_implicitly (object, PROP_BASLERDEMOSAICING, pspec);
      break;
    case PROP_AUTOEXPOSUREUPPERLIMIT:
      src->limitedFeature[AUTOF_EXPOSURE].upper = g_value_get_double (value);
      break;
    case PROP_AUTOEXPOSURELOWERLIMIT:
      src->limitedFeature[AUTOF_EXPOSURE].lower = g_value_get_double (value);
      break;
    case PROP_GAINLOWERLIMIT:
      src->limitedFeature[AUTOF_GAIN].lower = g_value_get_double (value);
      break;
    case PROP_GAINUPPERLIMIT:
      src->limitedFeature[AUTOF_GAIN].upper = g_value_get_double (value);
      break;
    case PROP_AUTOBRIGHTNESSTARGET:
      src->brightnesstarget = g_value_get_double (value);
      break;
    case PROP_DEMOSAICINGSHARPNESSENHANCEMENT:
      src->sharpnessenhancement = g_value_get_double (value);
      // Setting SharpnessEnhansement implies setting Basler Demosaising if the latter is not set to Simple explicitly
      set_prop_implicitly (object, PROP_BASLERDEMOSAICING, pspec);
      break;
    case PROP_TRANSFORMATION00:
      src->transformation[0][0] = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION01:
      src->transformation[0][1] = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION02:
      src->transformation[0][2] = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION10:
      src->transformation[1][0] = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION11:
      src->transformation[1][1] = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION12:
      src->transformation[1][2] = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION20:
      src->transformation[2][0] = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION21:
      src->transformation[2][1] = g_value_get_double (value);
      break;
    case PROP_TRANSFORMATION22:
      src->transformation[2][2] = g_value_get_double (value);
      break;
    case PROP_CONFIGFILE:
      g_free (src->configFile);
      src->configFile = g_value_dup_string (value);
      // Setting config-file implies setting ignore-defaults if the latter is not set to false explicitly
      set_prop_implicitly (object, PROP_IGNOREDEFAULTS, pspec);
      break;
    case PROP_IGNOREDEFAULTS:
      src->ignoreDefaults = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      return;
  }
  src->propFlags[property_id] = GST_PYLONSRC_PROPST_SET;
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
      g_value_set_int (value, src->size[AXIS_Y]);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, src->size[AXIS_X]);
      break;
    case PROP_BINNINGH:
      g_value_set_int (value, src->binning[AXIS_X]);
      break;
    case PROP_BINNINGV:
      g_value_set_int (value, src->binning[AXIS_Y]);
      break;
    case PROP_OFFSETX:
      g_value_set_int (value, src->offset[AXIS_X]);
      break;
    case PROP_OFFSETY:
      g_value_set_int (value, src->offset[AXIS_Y]);
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
      g_value_set_string (value, src->autoFeature[AUTOF_EXPOSURE]);
      break;
    case PROP_AUTOWHITEBALANCE:
      g_value_set_string (value, src->autoFeature[AUTOF_WHITEBALANCE]);
      break;
    case PROP_PIXEL_FORMAT:
      g_value_set_string (value, src->pixel_format);
      break;
    case PROP_USERID:
      g_value_set_string (value, src->userid);
      break;
    case PROP_AUTOGAIN:
      g_value_set_string (value, src->autoFeature[AUTOF_GAIN]);
      break;
    case PROP_RESET:
      g_value_set_string (value, src->reset);
      break;
    case PROP_AUTOPROFILE:
      g_value_set_string (value, src->autoprofile);
      break;
    case PROP_TRANSFORMATIONSELECTOR:
      g_value_set_string (value, src->transformationselector);
      break;
    case PROP_BALANCERED:
      g_value_set_double (value, src->balance[COLOUR_RED]);
      break;
    case PROP_BALANCEGREEN:
      g_value_set_double (value, src->balance[COLOUR_GREEN]);
      break;
    case PROP_BALANCEBLUE:
      g_value_set_double (value, src->balance[COLOUR_BLUE]);
      break;
    case PROP_COLORREDHUE:
      g_value_set_double (value, src->hue[COLOUR_RED]);
      break;
    case PROP_COLORREDSATURATION:
      g_value_set_double (value, src->saturation[COLOUR_RED]);
      break;
    case PROP_COLORYELLOWHUE:
      g_value_set_double (value, src->hue[COLOUR_YELLOW]);
      break;
    case PROP_COLORYELLOWSATURATION:
      g_value_set_double (value, src->saturation[COLOUR_YELLOW]);
      break;
    case PROP_COLORGREENHUE:
      g_value_set_double (value, src->hue[COLOUR_GREEN]);
      break;
    case PROP_COLORGREENSATURATION:
      g_value_set_double (value, src->saturation[COLOUR_GREEN]);
      break;
    case PROP_COLORCYANHUE:
      g_value_set_double (value, src->hue[COLOUR_CYAN]);
      break;
    case PROP_COLORCYANSATURATION:
      g_value_set_double (value, src->saturation[COLOUR_CYAN]);
      break;
    case PROP_COLORBLUEHUE:
      g_value_set_double (value, src->hue[COLOUR_BLUE]);
      break;
    case PROP_COLORBLUESATURATION:
      g_value_set_double (value, src->saturation[COLOUR_BLUE]);
      break;
    case PROP_COLORMAGENTAHUE:
      g_value_set_double (value, src->hue[COLOUR_MAGENTA]);
      break;
    case PROP_COLORMAGENTASATURATION:
      g_value_set_double (value, src->saturation[COLOUR_MAGENTA]);
      break;
    case PROP_MAXBANDWIDTH:
      g_value_set_int (value, src->maxBandwidth);
      break;
    case PROP_FLIPX:
      g_value_set_boolean (value, src->flip[AXIS_X]);
      break;
    case PROP_FLIPY:
      g_value_set_boolean (value, src->flip[AXIS_Y]);
      break;
    case PROP_CENTERX:
      g_value_set_boolean (value, src->center[AXIS_X]);
      break;
    case PROP_CENTERY:
      g_value_set_boolean (value, src->center[AXIS_Y]);
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
      g_value_set_double (value, src->limitedFeature[AUTOF_EXPOSURE].manual);
      break;
    case PROP_GAIN:
      g_value_set_double (value, src->limitedFeature[AUTOF_GAIN].manual);
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
      g_value_set_double (value, src->limitedFeature[AUTOF_EXPOSURE].upper);
      break;
    case PROP_AUTOEXPOSURELOWERLIMIT:
      g_value_set_double (value, src->limitedFeature[AUTOF_EXPOSURE].lower);
      break;
    case PROP_GAINLOWERLIMIT:
      g_value_set_double (value, src->limitedFeature[AUTOF_GAIN].lower);
      break;
    case PROP_GAINUPPERLIMIT:
      g_value_set_double (value, src->limitedFeature[AUTOF_GAIN].upper);
      break;
    case PROP_AUTOBRIGHTNESSTARGET:
      g_value_set_double (value, src->brightnesstarget);
      break;
    case PROP_TRANSFORMATION00:
      g_value_set_double (value, src->transformation[0][0]);
      break;
    case PROP_TRANSFORMATION01:
      g_value_set_double (value, src->transformation[0][1]);
      break;
    case PROP_TRANSFORMATION02:
      g_value_set_double (value, src->transformation[0][2]);
      break;
    case PROP_TRANSFORMATION10:
      g_value_set_double (value, src->transformation[1][0]);
      break;
    case PROP_TRANSFORMATION11:
      g_value_set_double (value, src->transformation[1][1]);
      break;
    case PROP_TRANSFORMATION12:
      g_value_set_double (value, src->transformation[1][2]);
      break;
    case PROP_TRANSFORMATION20:
      g_value_set_double (value, src->transformation[2][0]);
      break;
    case PROP_TRANSFORMATION21:
      g_value_set_double (value, src->transformation[2][1]);
      break;
    case PROP_TRANSFORMATION22:
      g_value_set_double (value, src->transformation[2][2]);
      break;
    case PROP_CONFIGFILE:
      g_value_set_string (value, src->configFile);
      break;
    case PROP_IGNOREDEFAULTS:
      g_value_set_boolean (value, src->ignoreDefaults);
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

  GST_DEBUG_OBJECT (src,
      "Received a request for caps. Filter:\n%" GST_PTR_FORMAT, filter);
  if (!src->deviceConnected) {
    GST_DEBUG_OBJECT (src, "Could not send caps - no camera connected.");
    return gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (bsrc));
  } else {
    GstCaps *result = gst_caps_copy (src->caps);
    GST_DEBUG_OBJECT (src, "Return caps:\n%" GST_PTR_FORMAT, result);
    return result;
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

static inline _Bool
feature_supported (const GstPylonSrc * src, const char *feature)
{
  if (PylonDeviceFeatureIsImplemented (src->deviceHandle, feature)) {
    return TRUE;
  } else {
    GST_WARNING_OBJECT (src, "Camera does not implement feature: %s", feature);
    return FALSE;
  }
}

static inline _Bool
feature_available (const GstPylonSrc * src, const char *feature)
{
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, feature)) {
    return TRUE;
  } else {
    GST_WARNING_OBJECT (src, "Feature is not available: %s", feature);
    return FALSE;
  }
}

static inline const char *
feature_alias_available (const GstPylonSrc * src, const char *feature,
    const char *alias)
{
  if (PylonDeviceFeatureIsAvailable (src->deviceHandle, feature)) {
    return feature;
  } else if (PylonDeviceFeatureIsAvailable (src->deviceHandle, alias)) {
    return alias;
  } else {
    GST_WARNING_OBJECT (src, "Feature is not available: %s or %s", feature,
        alias);
    return NULL;
  }
}

static inline _Bool
feature_readable (const GstPylonSrc * src, const char *feature)
{
  if (PylonDeviceFeatureIsReadable (src->deviceHandle, feature)) {
    return TRUE;
  } else {
    GST_WARNING_OBJECT (src, "Feature is not readable: %s", feature);
    return FALSE;
  }
}

static inline const char *
feature_alias_readable (const GstPylonSrc * src, const char *feature,
    const char *alias)
{
  if (PylonDeviceFeatureIsReadable (src->deviceHandle, feature)) {
    return feature;
  } else if (PylonDeviceFeatureIsReadable (src->deviceHandle, alias)) {
    return alias;
  } else {
    GST_WARNING_OBJECT (src, "Feature is not readable: %s or %s", feature,
        alias);
    return NULL;
  }
}

static gboolean
gst_pylonsrc_set_trigger (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  if (is_prop_implicit (src, PROP_CONTINUOUSMODE)) {
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

    reset_prop (src, PROP_CONTINUOUSMODE);
  }

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
    if (is_prop_set (src, PROP_CAMERA) && src->cameraId != 0) {
      GST_DEBUG_OBJECT (src,
          "Camera id was set, but was ignored as only one camera was found.");
      src->cameraId = 0;
    }
  } else if (numDevices > 1 && !is_prop_set (src, PROP_CAMERA)) {
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
  } else if (is_prop_set (src, PROP_CAMERA) && src->cameraId >= numDevices) {
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
  ascii_strdown (&src->reset, -1);
  if (strcmp (src->reset, "before") == 0) {
    if (PylonDeviceFeatureIsAvailable (src->deviceHandle, "DeviceReset")) {
      size_t numDevices;
      pylonc_reset_camera (src);
      pylonc_disconnect_camera (src);
      gst_pylonsrc_unref_pylon_environment ();

      GST_DEBUG_OBJECT (src,
          "Camera reset. Waiting 6 seconds for it to fully reboot.");
      g_usleep (6 * G_USEC_PER_SEC);

      gst_pylonsrc_ref_pylon_environment ();
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

static int64_t
gst_pylonsrc_read_max_size_axis (GstPylonSrc * src, GST_PYLONSRC_AXIS axis);

static _Bool
gst_pylonsrc_set_resolution_axis (GstPylonSrc * src, GST_PYLONSRC_AXIS axis)
{
  GENAPIC_RESULT res;

  // set binning of camera
  if (is_prop_implicit (src, propBinning[axis])) {
    if (feature_supported (src, featBinning[axis])) {
      GST_DEBUG_OBJECT (src, "Setting %s to %d", featBinning[axis],
          src->binning[axis]);
      res =
          PylonDeviceSetIntegerFeature (src->deviceHandle, featBinning[axis],
          src->binning[axis]);
      PYLONC_CHECK_ERROR (src, res);
    }
    reset_prop (src, propBinning[axis]);
  }

  if (is_prop_implicit (src, propSize[axis])) {
    if (!feature_supported (src, featSize[axis])) {
      GST_ERROR_OBJECT (src,
          "The camera doesn't seem to be reporting it's resolution.");
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"),
          ("Camera isn't reporting it's resolution. (Unsupported device?)"));
      goto error;
    }

    if (is_prop_set (src, propSize[axis])) {
      if (feature_supported (src, featMaxSize[axis])) {
        src->maxSize[axis] = (gint) gst_pylonsrc_read_max_size_axis (src, axis);
      }
      // Check if custom resolution is even possible and set it
      if (src->size[axis] > src->maxSize[axis]) {
        GST_ERROR_OBJECT (src, "Set %s is above camera's capabilities.",
            featSize[axis]);
        goto error;
      }
      // Set the final resolution
      res =
          PylonDeviceSetIntegerFeature (src->deviceHandle, featSize[axis],
          src->size[axis]);
      PYLONC_CHECK_ERROR (src, res);
    } else {
      // if defaulted, just read from camera
      int64_t size = 0;
      res =
          PylonDeviceGetIntegerFeature (src->deviceHandle, featSize[axis],
          &size);
      PYLONC_CHECK_ERROR (src, res);
      src->size[axis] = (gint) size;
    }

    if (is_prop_not_set (src, propSize[otherAxis[axis]])) {
      // if other axis is already configured
      GST_DEBUG_OBJECT (src, "Max resolution is %dx%d.", src->maxSize[AXIS_X],
          src->maxSize[AXIS_Y]);
      GST_DEBUG_OBJECT (src, "Setting resolution to %dx%d.", src->size[AXIS_X],
          src->size[AXIS_Y]);
    }

    reset_prop (src, propSize[axis]);
  }

  return TRUE;

error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_resolution (GstPylonSrc * src)
{
  return (gst_pylonsrc_set_resolution_axis (src, AXIS_X) &&
      gst_pylonsrc_set_resolution_axis (src, AXIS_Y));
}

static gboolean
gst_pylonsrc_set_offset_axis (GstPylonSrc * src, GST_PYLONSRC_AXIS axis)
{
  GENAPIC_RESULT res;

  if (is_prop_implicit (src, propCenter[axis])) {
    // Check if the user wants to center image first
    if (feature_supported (src, featCenter[axis])) {
      res =
          PylonDeviceSetBooleanFeature (src->deviceHandle, featCenter[axis],
          src->center[axis]);
      PYLONC_CHECK_ERROR (src, res);
    } else if (src->center[axis]
        && feature_supported (src, featCenterAlias[axis])) {
      res =
          PylonDeviceExecuteCommandFeature (src->deviceHandle,
          featCenterAlias[axis]);
      PYLONC_CHECK_ERROR (src, res);
    } else {
      src->center[axis] = FALSE;
    }

    GST_DEBUG_OBJECT (src, "%s: %s", featCenter[axis],
        boolalpha (src->center[axis]));

    reset_prop (src, propCenter[axis]);
  }

  if (is_prop_set (src, propOffset[axis])) {
    if (src->center[axis]) {
      GST_WARNING_OBJECT (src, "%s is ignored due to %s is set",
          featOffset[axis], featCenter[axis]);
    } else {
      if (feature_supported (src, featOffset[axis])) {
        const gint maxoffset = src->maxSize[axis] - src->size[axis];

        if (maxoffset >= src->offset[axis]) {
          res =
              PylonDeviceSetIntegerFeature (src->deviceHandle, featOffset[axis],
              src->offset[axis]);
          PYLONC_CHECK_ERROR (src, res);
          GST_DEBUG_OBJECT (src, "Setting %s to %d", featOffset[axis],
              src->offset[axis]);
        } else {
          GST_DEBUG_OBJECT (src,
              "Set %s is above camera's capabilities. (%d > %d)",
              featOffset[axis], src->offset[axis], maxoffset);
          GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
              ("Failed to initialise the camera"), ("Wrong %s specified",
                  featOffset[axis]));
          goto error;
        }
      }                         // if (feature_supported (src, featOffset[axis]))
    }                           // if(!src->center[axis]) 
    reset_prop (src, propOffset[axis]);
  }                             // if(!is_prop_not_set(src, propOffset[axis]))

  return TRUE;

error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_offset (GstPylonSrc * src)
{
  return gst_pylonsrc_set_offset_axis (src, AXIS_X) &&
      gst_pylonsrc_set_offset_axis (src, AXIS_Y);
}

static _Bool
gst_pylonsrc_set_reverse_axis (GstPylonSrc * src, GST_PYLONSRC_AXIS axis)
{
  GENAPIC_RESULT res;

  if (is_prop_implicit (src, propReverse[axis])) {
    // Flip the image
    if (feature_supported (src, featReverse[axis])) {
      res =
          PylonDeviceSetBooleanFeature (src->deviceHandle, featReverse[axis],
          src->flip[axis]);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src, "%s: %s", featReverse[axis],
          boolalpha (src->flip[axis]));
    } else {
      src->flip[axis] = FALSE;
    }
    reset_prop (src, propReverse[axis]);
  }

  return TRUE;

error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_reverse (GstPylonSrc * src)
{
  return gst_pylonsrc_set_reverse_axis (src, AXIS_X) &&
      gst_pylonsrc_set_reverse_axis (src, AXIS_Y);
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
          gst_genicam_pixel_format_caps_from_pixel_format_var
          (info->pixel_format, G_BYTE_ORDER, src->size[AXIS_X],
          src->size[AXIS_Y]);

      if (format_caps)
        gst_caps_append (caps, format_caps);
    }
  }

  GST_DEBUG_OBJECT (src, "Supported caps are %" GST_PTR_FORMAT, caps);

  g_string_free (format, TRUE);
  return caps;
}

static gboolean
gst_pylonsrc_set_pixel_format (GstPylonSrc * src)
{
  // TODO: handle PixelFormat change and caps renegotiation if possible
  if (is_prop_implicit (src, PROP_PIXEL_FORMAT)) {
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
    reset_prop (src, PROP_PIXEL_FORMAT);
  }
  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_test_image (GstPylonSrc * src)
{
  if (is_prop_implicit (src, PROP_TESTIMAGE)) {
    // Set whether test image will be shown
    if (feature_supported (src, "TestImageSelector")) {
      GENAPIC_RESULT res;
      if (src->testImage != 0) {
        char *ImageId;
        GST_DEBUG_OBJECT (src, "Test image mode enabled.");
        ImageId = g_strdup_printf ("Testimage%d", src->testImage);
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "TestImageSelector", ImageId);
        g_free (ImageId);
        PYLONC_CHECK_ERROR (src, res);
      } else {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "TestImageSelector", "Off");
        PYLONC_CHECK_ERROR (src, res);
      }
    }
    reset_prop (src, PROP_TESTIMAGE);
  }
  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_readout (GstPylonSrc * src)
{
  if (is_prop_implicit (src, PROP_SENSORREADOUTMODE)) {
    // Set sensor readout mode (default: Normal)
    if (feature_supported (src, "SensorReadoutMode")) {
      ascii_strdown (&src->sensorMode, -1);
      GENAPIC_RESULT res;
      if (strcmp (src->sensorMode, "normal") == 0) {

        GST_DEBUG_OBJECT (src, "Setting the sensor readout mode to normal.");
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "SensorReadoutMode", "Normal");
        PYLONC_CHECK_ERROR (src, res);
      } else if (strcmp (src->sensorMode, "fast") == 0) {
        GST_DEBUG_OBJECT (src, "Setting the sensor readout mode to fast.");
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "SensorReadoutMode", "Fast");
        PYLONC_CHECK_ERROR (src, res);
      } else {
        GST_ERROR_OBJECT (src,
            "Invalid parameter value for sensorreadoutmode. Available values are normal/fast, while the value provided was \"%s\".",
            src->sensorMode);
        GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
            ("Failed to initialise the camera"),
            ("Invalid parameters provided"));
        goto error;
      }
    }
    reset_prop (src, PROP_SENSORREADOUTMODE);
  }
  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_bandwidth (GstPylonSrc * src)
{
  GENAPIC_RESULT res;
  if (is_prop_implicit (src, PROP_LIMITBANDWIDTH)) {
    // Set bandwidth limit mode (default: on)  
    if (feature_supported (src, "DeviceLinkThroughputLimitMode")) {
      GST_DEBUG_OBJECT (src, "%s camera's bandwidth.",
          src->limitBandwidth ? "Limiting" : "Unlocking");
      res =
          PylonDeviceFeatureFromString (src->deviceHandle,
          "DeviceLinkThroughputLimitMode", src->limitBandwidth ? "On" : "Off");
      PYLONC_CHECK_ERROR (src, res);
    }
    reset_prop (src, PROP_LIMITBANDWIDTH);
  }

  if (is_prop_implicit (src, PROP_MAXBANDWIDTH)) {
    // Set bandwidth limit
    if (is_prop_set (src, PROP_MAXBANDWIDTH)) {
      if (feature_supported (src, "DeviceLinkThroughputLimit")) {
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
    }
    reset_prop (src, PROP_MAXBANDWIDTH);
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_framerate (GstPylonSrc * src)
{
  GENAPIC_RESULT res;
  if (is_prop_implicit (src, PROP_ACQUISITIONFRAMERATEENABLE)) {
    if (feature_available (src, "AcquisitionFrameRateEnable")) {
      res =
          PylonDeviceSetBooleanFeature (src->deviceHandle,
          "AcquisitionFrameRateEnable", src->setFPS);
      PYLONC_CHECK_ERROR (src, res);

      GST_DEBUG_OBJECT (src,
          "Limiting framerate: %s. See below for current framerate.",
          boolalpha (src->setFPS));
    }
    reset_prop (src, PROP_ACQUISITIONFRAMERATEENABLE);
  }

  if (is_prop_implicit (src, PROP_FPS)) {
    if (is_prop_set (src, PROP_FPS)) {
      // apply only if it is set explicitly (default is zero)
      const char *fps =
          feature_alias_available (src, "AcquisitionFrameRate",
          "AcquisitionFrameRateAbs");
      if (fps != NULL) {
        res = PylonDeviceSetFloatFeature (src->deviceHandle, fps, src->fps);
        PYLONC_CHECK_ERROR (src, res);
        if (src->setFPS) {
          GST_DEBUG_OBJECT (src, "Capping framerate to %0.2lf.", src->fps);
        }
      }
    }
    reset_prop (src, PROP_FPS);
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_lightsource (GstPylonSrc * src)
{
  GENAPIC_RESULT res;
  char *original = NULL;
  if (is_prop_implicit (src, PROP_LIGHTSOURCE)) {
    // Set lightsource preset
    const char *preset =
        feature_alias_available (src, "LightSourcePreset",
        "LightSourceSelector");
    if (preset == NULL && feature_available (src, "BslLightSourcePreset")) {
      preset = "BslLightSourcePreset";
    }

    if (preset != NULL) {
      original = g_strdup (src->lightsource);
      ascii_strdown (&src->lightsource, -1);

      if (strcmp (src->lightsource, "off") == 0) {
        GST_DEBUG_OBJECT (src, "Not using a lightsource preset.");
        res = PylonDeviceFeatureFromString (src->deviceHandle, preset, "Off");
        PYLONC_CHECK_ERROR (src, res);
      } else if (strcmp (src->lightsource, "2800k") == 0) {
        GST_DEBUG_OBJECT (src,
            "Setting light preset to Tungsten 2800k (Incandescen light).");
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            preset, "Tungsten2800K");
        PYLONC_CHECK_ERROR (src, res);
      } else if (strcmp (src->lightsource, "5000k") == 0) {
        GST_DEBUG_OBJECT (src,
            "Setting light preset to Daylight 5000k (Daylight).");
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            preset, "Daylight5000K");
        PYLONC_CHECK_ERROR (src, res);
      } else if (strcmp (src->lightsource, "6500k") == 0) {
        GST_DEBUG_OBJECT (src,
            "Setting light preset to Daylight 6500k (Very bright day).");
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            preset, "Daylight6500K");
        PYLONC_CHECK_ERROR (src, res);
      } else {
        // Lets try to set lightsourse as is
        GST_DEBUG_OBJECT (src, "Setting light preset to %s", original);
        res =
            PylonDeviceFeatureFromString (src->deviceHandle, preset, original);

        if (res != GENAPI_E_OK) {
          GST_ERROR_OBJECT (src,
              "Invalid parameter value for lightsource. Available values are off/2800k/5000k/6500k (if other values are supported by camera try setting them as specified in Basler documentation. E.g. \"Tungsten\" or \"Custom\"), while the value provided was \"%s\".",
              src->lightsource);
          GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
              ("Failed to initialise the camera"),
              ("Invalid parameters provided"));
          goto error;
        }
      }
    } else {
      GST_WARNING_OBJECT (src,
          "This camera doesn't have any lightsource presets");
    }
    reset_prop (src, PROP_LIGHTSOURCE);
  }
  g_free (original);
  return TRUE;

error:
  g_free (original);
  return FALSE;
}

static _Bool
gst_pylonsrc_set_auto_feature (GstPylonSrc * src,
    GST_PYLONSRC_AUTOFEATURE feature)
{
  if (is_prop_implicit (src, propAutoFeature[feature])) {
    // Enable/disable automatic feature
    if (feature_available (src, featAutoFeature[feature])) {
      GENAPIC_RESULT res;
      ascii_strdown (&src->autoFeature[feature], -1);
      if (strcmp (src->autoFeature[feature], "off") == 0) {
        GST_DEBUG_OBJECT (src, "Disabling %s.", featAutoFeature[feature]);
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            featAutoFeature[feature], "Off");
        PYLONC_CHECK_ERROR (src, res);
      } else if (strcmp (src->autoFeature[feature], "once") == 0) {
        GST_DEBUG_OBJECT (src, "Making the camera only calibrate %s once.",
            featManualFeature[feature]);
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            featAutoFeature[feature], "Once");
        PYLONC_CHECK_ERROR (src, res);
      } else if (strcmp (src->autoFeature[feature], "continuous") == 0) {
        GST_DEBUG_OBJECT (src,
            "Making the camera calibrate %s automatically all the time.",
            featManualFeature[feature]);
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            featAutoFeature[feature], "Continuous");
        PYLONC_CHECK_ERROR (src, res);
      } else {
        GST_ERROR_OBJECT (src,
            "Invalid parameter value for %s. Available values are off/once/continuous, while the value provided was \"%s\".",
            featAutoFeature[feature], src->autoFeature[feature]);
        GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
            ("Failed to initialise the camera"),
            ("Invalid parameters provided"));
        goto error;
      }
    }
    reset_prop (src, propAutoFeature[feature]);
  }

  return TRUE;
error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_limited_feature (GstPylonSrc * src,
    GST_PYLONSRC_AUTOFEATURE feature)
{
  if (feature >= AUTOF_NUM_LIMITED) {
    GST_WARNING_OBJECT (src,
        "Trying to set limits for unsupported autofeature: %d", (int) feature);
  } else {
    GENAPIC_RESULT res;

    // Configure automatic exposure and gain settings
    // Apply boundaries only if explicitly set
    if (is_prop_set (src, propLimitedUpper[feature])) {
      if (is_prop_default (src, propLimitedLower[feature])) {
        GST_WARNING_OBJECT (src, "Only the upper bound is set for %s",
            featManualFeature[feature]);
      }

      const char *upper =
          feature_alias_available (src, featLimitedUpper[feature],
          featLimitedUpperAlias[feature]);
      if (upper != NULL) {
        res =
            PylonDeviceSetFloatFeature (src->deviceHandle,
            upper, src->limitedFeature[feature].upper);
        PYLONC_CHECK_ERROR (src, res);
      }
      reset_prop (src, propLimitedUpper[feature]);
    }

    if (is_prop_set (src, propLimitedLower[feature])) {
      if (is_prop_default (src, propLimitedLower[feature])) {
        GST_WARNING_OBJECT (src, "Only the lower bound is set for %s",
            featManualFeature[feature]);
      } else {
        if (src->limitedFeature[feature].lower >=
            src->limitedFeature[feature].upper) {
          GST_ERROR_OBJECT (src,
              "Invalid parameter value for %s. It seems like you're trying to set a lower limit (%.2f) that's higher than the upper limit (%.2f).",
              featLimitedLower[feature], src->limitedFeature[feature].lower,
              src->limitedFeature[feature].upper);
          GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
              ("Failed to initialise the camera"),
              ("Invalid parameters provided"));
          goto error;
        }
      }

      const char *lower =
          feature_alias_available (src, featLimitedLower[feature],
          featLimitedLowerAlias[feature]);
      if (lower != NULL) {
        res =
            PylonDeviceSetFloatFeature (src->deviceHandle,
            lower, src->limitedFeature[feature].lower);
        PYLONC_CHECK_ERROR (src, res);
      }
      reset_prop (src, propLimitedLower[feature]);
    }
  }

  return TRUE;
error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_auto_exp_gain_wb (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  for (int i = 0; i < AUTOF_NUM_FEATURES; i++) {
    if (!gst_pylonsrc_set_auto_feature (src, (GST_PYLONSRC_AUTOFEATURE) i)) {
      goto error;
    }
  }

  for (int i = 0; i < AUTOF_NUM_LIMITED; i++) {
    if (!gst_pylonsrc_set_limited_feature (src, (GST_PYLONSRC_AUTOFEATURE) i)) {
      goto error;
    }
  }

  if (is_prop_implicit (src, PROP_AUTOBRIGHTNESSTARGET)) {
    if (is_prop_set (src, PROP_AUTOBRIGHTNESSTARGET)) {
      if (feature_available (src, "AutoTargetBrightness")) {
        res =
            PylonDeviceSetFloatFeature (src->deviceHandle,
            "AutoTargetBrightness", src->brightnesstarget);
        PYLONC_CHECK_ERROR (src, res);
      } else if (feature_available (src, "AutoTargetValue")) {
        res =
            PylonDeviceSetIntegerFeature (src->deviceHandle, "AutoTargetValue",
            (int64_t) src->brightnesstarget);
        PYLONC_CHECK_ERROR (src, res);
      }
    }
    reset_prop (src, PROP_AUTOBRIGHTNESSTARGET);
  }

  if (is_prop_implicit (src, PROP_AUTOPROFILE)) {
    if (is_prop_set (src, PROP_AUTOPROFILE)) {
      ascii_strdown (&src->autoprofile, -1);
      GST_DEBUG_OBJECT (src, "Setting automatic profile to minimise %s.",
          src->autoprofile);
      if (strcmp (src->autoprofile, "gain") == 0) {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "AutoFunctionProfile", "MinimizeGain");
        if (res != GENAPI_E_OK) {
          res =
              PylonDeviceFeatureFromString (src->deviceHandle,
              "AutoFunctionProfile", "GainMinimum");
          PYLONC_CHECK_ERROR (src, res);
        }
      } else if (strcmp (src->autoprofile, "exposure") == 0) {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "AutoFunctionProfile", "MinimizeExposureTime");
        if (res != GENAPI_E_OK) {
          res =
              PylonDeviceFeatureFromString (src->deviceHandle,
              "AutoFunctionProfile", "ExposureMinimum");
          PYLONC_CHECK_ERROR (src, res);
        }
      } else {
        GST_ERROR_OBJECT (src,
            "Invalid parameter value for autoprofile. Available values are gain/exposure, while the value provided was \"%s\".",
            src->autoprofile);
        GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
            ("Failed to initialise the camera"),
            ("Invalid parameters provided"));
        goto error;
      }
    } else {
      GST_DEBUG_OBJECT (src,
          "Using the auto profile currently saved on the device.");
    }
    reset_prop (src, PROP_AUTOPROFILE);
  }

  return TRUE;

error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_colour_balance (GstPylonSrc * src, GST_PYLONSRC_COLOUR colour)
{
  if (is_prop_implicit (src, propColourBalance[colour])) {
    if (is_prop_set (src, propColourBalance[colour])) {
      if (strcmp (src->autoFeature[AUTOF_WHITEBALANCE], "off") == 0) {
        const char *name =
            feature_alias_available (src, "BalanceRatio", "BalanceRatioAbs");
        if (name != NULL) {
          GENAPIC_RESULT res;
          res =
              PylonDeviceFeatureFromString (src->deviceHandle,
              "BalanceRatioSelector", featColour[colour]);
          PYLONC_CHECK_ERROR (src, res);
          res =
              PylonDeviceSetFloatFeature (src->deviceHandle, name,
              src->balance[colour]);
          PYLONC_CHECK_ERROR (src, res);

          GST_DEBUG_OBJECT (src, "%s balance set to %.2lf", featColour[colour],
              src->balance[colour]);
        }
      } else {
        GST_DEBUG_OBJECT (src,
            "Auto White Balance is enabled. Not setting %s Balance Ratio.",
            featColour[colour]);
      }
    } else {
      GST_DEBUG_OBJECT (src, "Using current settings for the colour %s.",
          featColour[colour]);
    }
    reset_prop (src, propColourBalance[colour]);
  }

  return TRUE;
error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_colour_hue (GstPylonSrc * src, GST_PYLONSRC_COLOUR colour)
{
  if (is_prop_implicit (src, propColourHue[colour])) {
    if (is_prop_set (src, propColourHue[colour])) {
      const char *selector =
          feature_alias_available (src, "ColorAdjustmentSelector",
          "BslColorAdjustmentSelector");
      if (selector != NULL) {
        GENAPIC_RESULT res;
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            selector, featColour[colour]);
        PYLONC_CHECK_ERROR (src, res);

        const char *hue =
            feature_alias_available (src, "ColorAdjustmentHue",
            "BslColorAdjustmentHue");
        if (hue != NULL) {
          res =
              PylonDeviceSetFloatFeature (src->deviceHandle, hue,
              src->hue[colour]);
          PYLONC_CHECK_ERROR (src, res);
        } else if (feature_available (src, "ColorAdjustmentHueRaw")) {
          res =
              PylonDeviceSetIntegerFeature (src->deviceHandle,
              "ColorAdjustmentHueRaw", (int64_t) src->hue[colour]);
          PYLONC_CHECK_ERROR (src, res);
        }

        GST_DEBUG_OBJECT (src, "%s hue set to %.2lf", featColour[colour],
            src->hue[colour]);
      }
    } else {
      GST_DEBUG_OBJECT (src, "Using saved %s hue.", featColour[colour]);
    }
    reset_prop (src, propColourHue[colour]);
  }

  return TRUE;
error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_colour_saturation (GstPylonSrc * src,
    GST_PYLONSRC_COLOUR colour)
{
  if (is_prop_implicit (src, propColourSaturation[colour])) {
    if (is_prop_set (src, propColourSaturation[colour])) {
      const char *selector =
          feature_alias_available (src, "ColorAdjustmentSelector",
          "BslColorAdjustmentSelector");
      if (selector != NULL) {
        GENAPIC_RESULT res;
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            selector, featColour[colour]);
        PYLONC_CHECK_ERROR (src, res);

        const char *saturation =
            feature_alias_available (src, "ColorAdjustmentSaturation",
            "BslColorAdjustmentSaturation");
        if (saturation != NULL) {
          res =
              PylonDeviceSetFloatFeature (src->deviceHandle, saturation,
              src->saturation[colour]);
          PYLONC_CHECK_ERROR (src, res);
        } else if (feature_available (src, "ColorAdjustmentSaturationRaw")) {
          res =
              PylonDeviceSetIntegerFeature (src->deviceHandle,
              "ColorAdjustmentSaturationRaw",
              (int64_t) src->saturation[colour]);
          PYLONC_CHECK_ERROR (src, res);
        }

        GST_DEBUG_OBJECT (src, "%s saturation set to %.2lf", featColour[colour],
            src->saturation[colour]);
      }
    } else {
      GST_DEBUG_OBJECT (src, "Using saved %s saturation.", featColour[colour]);
    }
    reset_prop (src, propColourSaturation[colour]);
  }

  return TRUE;
error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_colour_transformation (GstPylonSrc * src, int i, int j)
{
  GENAPIC_RESULT res;

  if (is_prop_implicit (src, propTransform[j][i])) {
    if (is_prop_set (src, propTransform[j][i])) {
      if (feature_available (src, "ColorTransformationSelector")) {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle,
            "ColorTransformationValueSelector", featTransform[j][i]);
        PYLONC_CHECK_ERROR (src, res);
        res =
            PylonDeviceSetFloatFeature (src->deviceHandle,
            "ColorTransformationValue", src->transformation[j][i]);
        PYLONC_CHECK_ERROR (src, res);

        GST_DEBUG_OBJECT (src, "%s set to %.2lf", featTransform[j][i],
            src->transformation[j][i]);
      }
    } else {
      GST_DEBUG_OBJECT (src, "Using saved %s transformation value. ",
          featTransform[j][i]);
    }
    reset_prop (src, propTransform[j][i]);
  }

  return TRUE;
error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_color (GstPylonSrc * src)
{
  for (int i = 0; i < 3; i++) {
    if (!gst_pylonsrc_set_colour_balance (src, (GST_PYLONSRC_COLOUR) i)) {
      goto error;
    }
  }

  if (is_prop_implicit (src, PROP_COLORADJUSTMENTENABLE)) {
    if (is_prop_set (src, PROP_COLORADJUSTMENTENABLE)) {
      if (feature_available (src, "ColorAdjustmentEnable")) {
        GENAPIC_RESULT res = PylonDeviceSetBooleanFeature (src->deviceHandle,
            "ColorAdjustmentEnable", src->colorAdjustment);
        PYLONC_CHECK_ERROR (src, res);
      } else {
        src->colorAdjustment = TRUE;
      }
    }
    reset_prop (src, PROP_COLORADJUSTMENTENABLE);
  }

  if (src->colorAdjustment) {
    // Configure colour adjustment
    for (int i = 0; i < 6; i++) {
      const GST_PYLONSRC_COLOUR colour = (GST_PYLONSRC_COLOUR) i;
      if (!gst_pylonsrc_set_colour_hue (src, colour) ||
          !gst_pylonsrc_set_colour_saturation (src, colour)) {
        goto error;
      }
    }
  }
  // Configure colour transformation
  if (is_prop_implicit (src, PROP_TRANSFORMATIONSELECTOR)) {
    if (is_prop_set (src, PROP_TRANSFORMATIONSELECTOR)) {
      if (feature_available (src, "ColorTransformationSelector")) {
        ascii_strdown (&src->transformationselector, -1);
        GENAPIC_RESULT res;
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
        } else if (strcmp (src->transformationselector, "yuvrgb") == 0) {
          res =
              PylonDeviceFeatureFromString (src->deviceHandle,
              "ColorTransformationSelector", "YUVtoRGB");
          PYLONC_CHECK_ERROR (src, res);
        } else {
          GST_ERROR_OBJECT (src,
              "Invalid parameter value for transformationselector. Available values are: RGBRGB, RGBYUV, YUVRGB. Value provided: \"%s\".",
              src->transformationselector);
          GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
              ("Failed to initialise the camera"),
              ("Invalid parameters provided"));
          goto error;
        }
      }
    }
    reset_prop (src, PROP_TRANSFORMATIONSELECTOR);
  }

  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      if (!gst_pylonsrc_set_colour_transformation (src, i, j)) {
        goto error;
      }
    }
  }

  return TRUE;

error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_manual_feature (GstPylonSrc * src,
    GST_PYLONSRC_AUTOFEATURE feature)
{
  if (feature >= AUTOF_NUM_LIMITED) {
    GST_WARNING_OBJECT (src,
        "Trying to set manual value for unsupported autofeature: %d",
        (int) feature);
  } else {
    // Configure exposure/gain
    if (is_prop_implicit (src, propManualFeature[feature])) {
      if (is_prop_set (src, propManualFeature[feature])) {
        const char *name =
            feature_alias_available (src, featManualFeature[feature],
            featManualFeatureAlias[feature]);
        if (name != NULL) {
          if (strcmp (src->autoFeature[feature], "off") == 0) {
            GENAPIC_RESULT res;
            GST_DEBUG_OBJECT (src, "Setting %s to %0.2lf",
                name, src->limitedFeature[feature].manual);
            res =
                PylonDeviceSetFloatFeature (src->deviceHandle,
                name, src->limitedFeature[feature].manual);
            PYLONC_CHECK_ERROR (src, res);
          } else {
            GST_WARNING_OBJECT (src,
                "%s has been enabled, skipping setting manual %s.",
                featAutoFeature[feature], name);
          }
        }
      } else {
        GST_DEBUG_OBJECT (src,
            "%s property not set, using the saved setting.",
            featManualFeature[feature]);
      }
      reset_prop (src, propManualFeature[feature]);
    }
  }

  return TRUE;

error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_exposure_gain_level (GstPylonSrc * src)
{
  GENAPIC_RESULT res;

  for (int i = 0; i < AUTOF_NUM_LIMITED; i++) {
    if (!gst_pylonsrc_set_manual_feature (src, (GST_PYLONSRC_AUTOFEATURE) i)) {
      goto error;
    }
  }

  // Configure black level
  if (is_prop_implicit (src, PROP_BLACKLEVEL)) {
    const char *name =
        feature_alias_available (src, "BlackLevel", "BlackLevelRaw");
    if (name != NULL) {
      GST_DEBUG_OBJECT (src, "Setting black level to %0.2lf", src->blacklevel);
      res =
          PylonDeviceSetFloatFeature (src->deviceHandle, name, src->blacklevel);
      PYLONC_CHECK_ERROR (src, res);
    }
    reset_prop (src, PROP_BLACKLEVEL);
  }
  // Configure gamma correction
  if (is_prop_implicit (src, PROP_GAMMA)) {
    if (is_prop_set (src, PROP_GAMMA)) {
      if (feature_available (src, "GammaEnable")) {
        res =
            PylonDeviceSetBooleanFeature (src->deviceHandle, "GammaEnable", 1);
        PYLONC_CHECK_ERROR (src, res);
        res =
            PylonDeviceFeatureFromString (src->deviceHandle, "GammaSelector",
            "User");
        PYLONC_CHECK_ERROR (src, res);
      }

      if (feature_available (src, "Gamma")) {
        GST_DEBUG_OBJECT (src, "Setting gamma to %0.2lf", src->gamma);
        res =
            PylonDeviceSetFloatFeature (src->deviceHandle, "Gamma", src->gamma);
        PYLONC_CHECK_ERROR (src, res);
      }
    }
    reset_prop (src, PROP_GAMMA);
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_set_pgi (GstPylonSrc * src)
{
  if (is_prop_implicit (src, PROP_BASLERDEMOSAICING)) {
    const char *demosaicing =
        feature_alias_available (src, featDemosaicing[0].name, featDemosaicing[1].name);
    if (demosaicing != NULL) {
      GENAPIC_RESULT res;
      ptrdiff_t idx = (demosaicing == featDemosaicing[0].name) ? 0 : 1;
      const char *pgiOn = featDemosaicing[idx].on;
      const char *pgiOff = featDemosaicing[idx].off;

      if (src->demosaicing) {
        if (strncmp ("bayer", src->pixel_format, 5) != 0) {
          GST_DEBUG_OBJECT (src, "Enabling Basler's PGI.");
          res =
              PylonDeviceFeatureFromString (src->deviceHandle,
              demosaicing, pgiOn);
          PYLONC_CHECK_ERROR (src, res);

          // PGI Modules (Noise reduction and Sharpness enhancement).
          if (is_prop_implicit (src, PROP_DEMOSAICINGNOISEREDUCTION)) {
            if (is_prop_set (src, PROP_DEMOSAICINGNOISEREDUCTION)) {
              const char *noise =
                  feature_alias_available (src, "NoiseReduction",
                  "NoiseReductionAbs");
              if (noise != NULL) {
                GST_DEBUG_OBJECT (src, "Setting PGI noise reduction to %0.2lf",
                    src->noisereduction);
                res =
                    PylonDeviceSetFloatFeature (src->deviceHandle,
                    noise, src->noisereduction);
                PYLONC_CHECK_ERROR (src, res);
              }
            } else {
              GST_DEBUG_OBJECT (src,
                  "Using the stored value for noise reduction.");
            }
            reset_prop (src, PROP_DEMOSAICINGNOISEREDUCTION);
          }

          if (is_prop_implicit (src, PROP_DEMOSAICINGSHARPNESSENHANCEMENT)) {
            if (is_prop_set (src, PROP_DEMOSAICINGSHARPNESSENHANCEMENT)) {
              const char *sharpness =
                  feature_alias_available (src, "SharpnessEnhancement",
                  "SharpnessEnhancementAbs");
              if (sharpness != NULL) {
                GST_DEBUG_OBJECT (src,
                    "Setting PGI sharpness enhancement to %0.2lf",
                    src->sharpnessenhancement);
                res =
                    PylonDeviceSetFloatFeature (src->deviceHandle,
                    sharpness, src->sharpnessenhancement);
                PYLONC_CHECK_ERROR (src, res);
              }
            } else {
              GST_DEBUG_OBJECT (src,
                  "Using the stored value for sharpness enhancement.");
            }
            reset_prop (src, PROP_DEMOSAICINGSHARPNESSENHANCEMENT);
          }
        } else {
          GST_WARNING_OBJECT (src,
              "Usage of PGI is not permitted with bayer output. Skipping.");
        }
      } else {
        res =
            PylonDeviceFeatureFromString (src->deviceHandle, demosaicing,
            pgiOff);
        PYLONC_CHECK_ERROR (src, res);
      }
    }
    reset_prop (src, PROP_BASLERDEMOSAICING);
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_pylonsrc_configure_start_acquisition (GstPylonSrc * src)
{
  GENAPIC_RESULT res;
  size_t i;
  size_t num_streams;

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
  for (i = 0; i < GST_PYLONSRC_NUM_CAPTURE_BUFFERS; ++i) {
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
      GST_PYLONSRC_NUM_CAPTURE_BUFFERS);
  PYLONC_CHECK_ERROR (src, res);
  res =
      PylonStreamGrabberSetMaxBufferSize (src->streamGrabber, src->payloadSize);
  PYLONC_CHECK_ERROR (src, res);

  // Prepare the camera for grabbing
  res = PylonStreamGrabberPrepareGrab (src->streamGrabber);
  PYLONC_CHECK_ERROR (src, res);

  for (i = 0; i < GST_PYLONSRC_NUM_CAPTURE_BUFFERS; ++i) {
    res =
        PylonStreamGrabberRegisterBuffer (src->streamGrabber, src->buffers[i],
        src->payloadSize, &src->bufferHandle[i]);
    PYLONC_CHECK_ERROR (src, res);
  }

  for (i = 0; i < GST_PYLONSRC_NUM_CAPTURE_BUFFERS; ++i) {
    res =
        PylonStreamGrabberQueueBuffer (src->streamGrabber, src->bufferHandle[i],
        (void *) i);
    PYLONC_CHECK_ERROR (src, res);
  }

  // Output the bandwidth the camera will actually use [B/s]
  if (feature_supported (src, "DeviceLinkCurrentThroughput")
      && feature_supported (src, "DeviceLinkSpeed")) {
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
  }
  // Output sensor readout time [us]
  if (feature_supported (src, "SensorReadoutTime")) {
    double readoutTime = 0.0;

    res =
        PylonDeviceGetFloatFeature (src->deviceHandle, "SensorReadoutTime",
        &readoutTime);
    PYLONC_CHECK_ERROR (src, res);

    GST_DEBUG_OBJECT (src,
        "With these settings it will take approximately %.0lf microseconds to grab each frame.",
        readoutTime);
  }
  // Output final frame rate [Hz]
  const char *name =
      feature_alias_available (src, "ResultingFrameRate",
      "ResultingFrameRateAbs");
  if (name != NULL) {
    double frameRate = 0.0;

    res = PylonDeviceGetFloatFeature (src->deviceHandle, name, &frameRate);
    PYLONC_CHECK_ERROR (src, res);

    GST_DEBUG_OBJECT (src, "The resulting framerate is %.0lf fps.", frameRate);
    GST_DEBUG_OBJECT (src,
        "Each frame is %d bytes big (%.1lf MB). That's %.1lfMB/s.",
        src->payloadSize, (double) src->payloadSize / 1000000,
        (src->payloadSize * frameRate) / 1000000);
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
  src->frameNumber = 0;

  GST_DEBUG_OBJECT (src, "Initialised successfully.");
  return TRUE;

error:
  return FALSE;
}

static gchar *
read_string_feature (GstPylonSrc * src, const char *feature)
{
  if (feature_readable (src, feature)) {
    gchar *result = NULL;
    size_t bufLen = 0;

    for (int i = 0; i < 2; i++) {
      // g_malloc(0) == NULL
      result = g_malloc (bufLen);
      // get bufLen at first iteration
      // read value at second iteration
      GENAPIC_RESULT res =
          PylonDeviceFeatureToString (src->deviceHandle, feature, result,
          &bufLen);
      PYLONC_CHECK_ERROR (src, res);
    }
    GST_DEBUG_OBJECT (src, "Reading string feature: %s = %s", feature, result);
    return result;

  error:
    g_free (result);
  }
  return NULL;
}

static GENAPIC_RESULT
read_bool_feature (GstPylonSrc * src, const char *feature, _Bool * result)
{
  GENAPIC_RESULT res = GENAPI_E_FAIL;
  if (feature_readable (src, feature)) {
    res = PylonDeviceGetBooleanFeature (src->deviceHandle, feature, result);
    GST_DEBUG_OBJECT (src, "Reading bool feature: %s = %s", feature,
        boolalpha (*result));
  }

  return res;
}

static GENAPIC_RESULT
read_integer_feature (GstPylonSrc * src, const char *feature, int64_t * result)
{
  GENAPIC_RESULT res = GENAPI_E_FAIL;
  if (feature_readable (src, feature)) {
    res = PylonDeviceGetIntegerFeature (src->deviceHandle, feature, result);
    GST_DEBUG_OBJECT (src, "Reading integer feature: %s = %ld", feature,
        *result);
  }

  return res;
}

static GENAPIC_RESULT
read_float_feature (GstPylonSrc * src, const char *feature, double *result)
{
  GENAPIC_RESULT res = GENAPI_E_FAIL;
  if (feature_readable (src, feature)) {
    res = PylonDeviceGetFloatFeature (src->deviceHandle, feature, result);
    GST_DEBUG_OBJECT (src, "Reading float feature: %s = %f", feature, *result);
  }

  return res;
}

static GENAPIC_RESULT
read_float_feature_alias (GstPylonSrc * src, const char *feature,
    const char *alias, double *result)
{
  GENAPIC_RESULT res = GENAPI_E_FAIL;
  const char *name = feature_alias_readable (src, feature, alias);
  if (name != NULL) {
    res = PylonDeviceGetFloatFeature (src->deviceHandle, name, result);
    GST_DEBUG_OBJECT (src, "Reading float feature: %s = %f", name, *result);
  }

  return res;
}

static int64_t
gst_pylonsrc_read_max_size_axis (GstPylonSrc * src, GST_PYLONSRC_AXIS axis)
{
  int64_t maxSize;
  GENAPIC_RESULT res = read_integer_feature (src, featMaxSize[axis], &maxSize);
  if (res == GENAPI_E_OK) {
    return maxSize;
  } else {
    res =
        PylonDeviceGetIntegerFeatureMax (src->deviceHandle, featSize[axis],
        &maxSize);
    if (res == GENAPI_E_OK) {
      GST_INFO_OBJECT (src, "Read %s value from upper bound of %s",
          featMaxSize[axis], featSize[axis]);
      return maxSize;
    }
  }
  return G_MAXINT;
}

static void
gst_pylonsrc_read_offset_axis (GstPylonSrc * src, GST_PYLONSRC_AXIS axis)
{
  if (is_prop_not_set (src, propCenter[axis])) {
    read_bool_feature (src, featCenter[axis], &src->center[axis]);
  }

  if (is_prop_not_set (src, propOffset[axis])) {
    int64_t temp;
    GENAPIC_RESULT res = read_integer_feature (src, featOffset[axis], &temp);
    if (res == GENAPI_E_OK) {
      src->offset[axis] = temp;
    }
  }
}

static void
gst_pylonsrc_read_offset (GstPylonSrc * src)
{
  gst_pylonsrc_read_offset_axis (src, AXIS_X);
  gst_pylonsrc_read_offset_axis (src, AXIS_Y);
}

static void
gst_pylonsrc_read_reverse_axis (GstPylonSrc * src, GST_PYLONSRC_AXIS axis)
{
  if (is_prop_not_set (src, propReverse[axis])) {
    read_bool_feature (src, featReverse[axis], &src->flip[axis]);
  }
}

static void
gst_pylonsrc_read_reverse (GstPylonSrc * src)
{
  gst_pylonsrc_read_reverse_axis (src, AXIS_X);
  gst_pylonsrc_read_reverse_axis (src, AXIS_Y);
}

static void
gst_pylonsrc_read_pixel_format (GstPylonSrc * src)
{
  if (is_prop_not_set (src, PROP_PIXEL_FORMAT)) {
    g_free (src->pixel_format);
    src->pixel_format = read_string_feature (src, "PixelFormat");
    if (src->pixel_format == NULL) {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"), ("Unable to read PixelFormat"));
    }
  }
}

static void
gst_pylonsrc_read_test_image (GstPylonSrc * src)
{
  if (is_prop_not_set (src, PROP_TESTIMAGE)) {
    static const char prefix[] = "Testimage";
    char *ImageID = read_string_feature (src, "TestImageSelector");

    if ((ImageID != NULL)
        && (g_ascii_strncasecmp (ImageID, prefix, sizeof (prefix) - 1) == 0)) {
      src->testImage = atoi (&ImageID[sizeof (prefix) - 1]);
    } else {
      src->testImage = 0;
    }
    g_free (ImageID);
  }
}

static void
gst_pylonsrc_read_readout (GstPylonSrc * src)
{
  if (is_prop_not_set (src, PROP_SENSORREADOUTMODE)) {
    g_free (src->sensorMode);
    src->sensorMode = read_string_feature (src, "SensorReadoutMode");
  }
}

static void
gst_pylonsrc_read_bandwidth (GstPylonSrc * src)
{
  if (is_prop_not_set (src, PROP_LIMITBANDWIDTH)) {
    char *mode = read_string_feature (src, "DeviceLinkThroughputLimitMode");
    if ((mode != NULL) && (g_ascii_strncasecmp (mode, "On", -1) == 0)) {
      src->limitBandwidth = TRUE;
    } else {
      src->limitBandwidth = FALSE;
    }
    g_free (mode);
  }

  if (is_prop_not_set (src, PROP_MAXBANDWIDTH)) {
    int64_t temp;
    GENAPIC_RESULT res =
        read_integer_feature (src, "DeviceLinkThroughputLimit", &temp);
    if (res == GENAPI_E_OK) {
      src->maxBandwidth = temp;
    }
  }
}

static void
gst_pylonsrc_read_framerate (GstPylonSrc * src)
{
  if (is_prop_not_set (src, PROP_ACQUISITIONFRAMERATEENABLE)) {
    GENAPIC_RESULT res = read_bool_feature (src,
        "AcquisitionFrameRateEnable", &src->setFPS);
    if (res != GENAPI_E_OK) {
      src->setFPS = FALSE;
    }
  }

  if (is_prop_not_set (src, PROP_FPS)) {
    GENAPIC_RESULT res = read_float_feature_alias (src,
        "AcquisitionFrameRate", "AcquisitionFrameRateAbs", &src->fps);
    if (res != GENAPI_E_OK) {
      src->fps = 0.0;
    }
  }
}

static void
gst_pylonsrc_read_lightsource (GstPylonSrc * src)
{
  if (is_prop_not_set (src, PROP_LIGHTSOURCE)) {
    g_free (src->lightsource);
    src->lightsource = NULL;

    char *temp = read_string_feature (src, "LightSourcePreset");
    if (temp == NULL) {
      temp = read_string_feature (src, "BslLightSourcePreset");
      if (temp == NULL) {
        temp = read_string_feature (src, "LightSourceSelector");
      }
    }

    if (temp != NULL) {
      // if temp is something like "Daylight5000K"
      // We need to set src->lightsource to "5000K"
      for (int i = 0; i < strlen (temp); i++) {
        if (temp[i] >= '0' && temp[i] <= '9') {
          // found digit
          src->lightsource = g_strdup (&temp[i]);
          break;
        }
      }
      // if temp is "Custom" or "Off", use the entire string
      if (src->lightsource == NULL) {
        src->lightsource = temp;
      } else {
        g_free (temp);
      }
    }

    if (src->lightsource == NULL) {
      src->lightsource = g_strdup ("off");
    }
  }
}

static void
gst_pylonsrc_read_auto_feature (GstPylonSrc * src,
    GST_PYLONSRC_AUTOFEATURE feature)
{
  if (is_prop_not_set (src, propAutoFeature[feature])) {
    g_free (src->autoFeature[feature]);
    src->autoFeature[feature] =
        read_string_feature (src, featAutoFeature[feature]);
    if (src->autoFeature[feature] == NULL) {
      src->autoFeature[feature] = g_strdup ("off");
    }
  }
}

static void
gst_pylonsrc_read_limited_feature (GstPylonSrc * src,
    GST_PYLONSRC_AUTOFEATURE feature)
{
  if (feature >= AUTOF_NUM_LIMITED) {
    GST_WARNING_OBJECT (src,
        "Trying to read limits for unsupported autofeature: %d", (int) feature);
  } else {
    if (is_prop_not_set (src, propLimitedUpper[feature])) {
      read_float_feature_alias (src, featLimitedUpper[feature],
          featLimitedUpperAlias[feature], &src->limitedFeature[feature].upper);
    }

    if (is_prop_not_set (src, propLimitedLower[feature])) {
      read_float_feature_alias (src, featLimitedLower[feature],
          featLimitedLowerAlias[feature], &src->limitedFeature[feature].lower);
    }
  }
}


static void
gst_pylonsrc_read_auto_exp_gain_wb (GstPylonSrc * src)
{
  for (int i = 0; i < AUTOF_NUM_FEATURES; i++) {
    gst_pylonsrc_read_auto_feature (src, (GST_PYLONSRC_AUTOFEATURE) i);
  }

  for (int i = 0; i < AUTOF_NUM_LIMITED; i++) {
    gst_pylonsrc_read_limited_feature (src, (GST_PYLONSRC_AUTOFEATURE) i);
  }

  if (is_prop_not_set (src, PROP_AUTOBRIGHTNESSTARGET)) {
    read_float_feature_alias (src, "AutoTargetBrightness", "AutoTargetValue",
        &src->brightnesstarget);
  }

  if (is_prop_not_set (src, PROP_AUTOPROFILE)) {
    static const char prefix[] = "Minimize";
    g_free (src->autoprofile);

    char *temp = read_string_feature (src, "AutoFunctionProfile");
    if ((temp != NULL)) {
      if (g_ascii_strncasecmp (temp, prefix, sizeof (prefix) - 1) == 0) {
        // "MinimizeGain" -> "gain"
        src->autoprofile = g_ascii_strdown (&temp[sizeof (prefix) - 1], -1);
      } else {
        static const char gain[] = "Gain";
        static const char exposure[] = "Exposure";
        if (g_ascii_strncasecmp (temp, gain, sizeof (gain) - 1) == 0) {
          src->autoprofile = g_strdup ("gain");
        } else if (g_ascii_strncasecmp (temp, exposure,
                sizeof (exposure) - 1) == 0) {
          src->autoprofile = g_strdup ("exposure");
        } else {
          GST_WARNING_OBJECT (src,
              "Unexpectd AutoFuncitonProfile value on device: %s", temp);
          src->autoprofile = g_strdup (temp);
        }
      }
    } else {
      src->autoprofile = g_strdup ("off");
    }
    g_free (temp);
  }
}

static void
gst_pylonsrc_read_colour_balance (GstPylonSrc * src, GST_PYLONSRC_COLOUR colour)
{
  if (is_prop_not_set (src, propColourBalance[colour])) {
    GENAPIC_RESULT res = PylonDeviceFeatureFromString (src->deviceHandle,
        "BalanceRatioSelector", featColour[colour]);
    if (res == GENAPI_E_OK) {
      read_float_feature_alias (src, "BalanceRatio", "BalanceRatioAbs",
          &src->balance[colour]);
    }
  }
}

static void
gst_pylonsrc_read_colour_hue (GstPylonSrc * src, GST_PYLONSRC_COLOUR colour)
{
  if (is_prop_not_set (src, propColourHue[colour])) {
    const char *selector =
        feature_alias_available (src, "ColorAdjustmentSelector",
        "BslColorAdjustmentSelector");
    GENAPIC_RESULT res = PylonDeviceFeatureFromString (src->deviceHandle,
        selector, featColour[colour]);
    if (res == GENAPI_E_OK) {
      res =
          read_float_feature_alias (src, "ColorAdjustmentHue",
          "BslColorAdjustmentHue", &src->hue[colour]);
      if (res != GENAPI_E_OK) {
        int64_t hue;
        res = read_integer_feature (src, "ColorAdjustmentHueRaw", &hue);
        if (res == GENAPI_E_OK) {
          src->hue[colour] = (double) hue;
        }
      }
    }
  }
}

static void
gst_pylonsrc_read_colour_saturation (GstPylonSrc * src,
    GST_PYLONSRC_COLOUR colour)
{
  if (is_prop_not_set (src, propColourSaturation[colour])) {
    const char *selector =
        feature_alias_available (src, "ColorAdjustmentSelector",
        "BslColorAdjustmentSelector");
    GENAPIC_RESULT res = PylonDeviceFeatureFromString (src->deviceHandle,
        selector, featColour[colour]);
    if (res == GENAPI_E_OK) {
      res =
          read_float_feature_alias (src, "ColorAdjustmentSaturation",
          "BslColorAdjustmentSaturation", &src->saturation[colour]);
      if (res != GENAPI_E_OK) {
        int64_t saturation;
        res =
            read_integer_feature (src, "ColorAdjustmentSaturationRaw",
            &saturation);
        if (res == GENAPI_E_OK) {
          src->saturation[colour] = (double) saturation;
        }
      }
    }
  }
}

static void
gst_pylonsrc_read_colour_transformation (GstPylonSrc * src, int i, int j)
{
  if (is_prop_not_set (src, propTransform[j][i])) {
    GENAPIC_RESULT res = PylonDeviceFeatureFromString (src->deviceHandle,
        "ColorTransformationValueSelector", featTransform[j][i]);
    if (res == GENAPI_E_OK) {
      read_float_feature (src,
          "ColorTransformationValue", &src->transformation[j][i]);
    }
  }
}

static void
gst_pylonsrc_read_color (GstPylonSrc * src)
{
  for (int i = 0; i < 3; i++) {
    gst_pylonsrc_read_colour_balance (src, (GST_PYLONSRC_COLOUR) i);
  }

  if (is_prop_not_set (src, PROP_COLORADJUSTMENTENABLE)) {
    src->colorAdjustment = TRUE;
    read_bool_feature (src, "ColorAdjustmentEnable", &src->colorAdjustment);
  }

  for (int i = 0; i < 6; i++) {
    const GST_PYLONSRC_COLOUR colour = (GST_PYLONSRC_COLOUR) i;
    gst_pylonsrc_read_colour_hue (src, colour);
    gst_pylonsrc_read_colour_saturation (src, colour);
  }

  // Configure colour transformation
  if (is_prop_not_set (src, PROP_TRANSFORMATIONSELECTOR)) {
    char *temp = read_string_feature (src, "ColorTransformationSelector");
    g_free (src->transformationselector);
    src->transformationselector = NULL;

    if (temp != NULL) {
      if (strcmp (temp, "RGBtoRGB") == 0) {
        src->transformationselector = g_strdup ("rgbrgb");
      } else if (strcmp (temp, "RGBtoYUV") == 0) {
        src->transformationselector = g_strdup ("rgbyuv");
      } else if (strcmp (temp, "YUVtoRGB") == 0) {
        src->transformationselector = g_strdup ("yuvrgb");
      }
      g_free (temp);
    }

    if (src->transformationselector == NULL) {
      src->transformationselector = g_strdup ("off");
    }
  }

  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 3; i++) {
      gst_pylonsrc_read_colour_transformation (src, i, j);
    }
  }
}

static void
gst_pylonsrc_read_manual_feature (GstPylonSrc * src,
    GST_PYLONSRC_AUTOFEATURE feature)
{
  if (feature >= AUTOF_NUM_LIMITED) {
    GST_WARNING_OBJECT (src,
        "Trying to read manual value for unsupported autofeature: %d",
        (int) feature);
  } else {
    if (is_prop_not_set (src, propManualFeature[feature])) {
      read_float_feature_alias (src, featManualFeature[feature],
          featManualFeatureAlias[feature],
          &src->limitedFeature[feature].manual);
    }
  }
}

static void
gst_pylonsrc_read_exposure_gain_level (GstPylonSrc * src)
{
  for (int i = 0; i < AUTOF_NUM_LIMITED; i++) {
    gst_pylonsrc_read_manual_feature (src, (GST_PYLONSRC_AUTOFEATURE) i);
  }

  if (is_prop_not_set (src, PROP_BLACKLEVEL)) {
    read_float_feature_alias (src, "BlackLevel", "BlackLevelRaw",
        &src->blacklevel);
  }
  // Configure gamma correction
  if (is_prop_not_set (src, PROP_GAMMA)) {
    read_float_feature (src, "Gamma", &src->gamma);
  }
}

static void
gst_pylonsrc_read_pgi (GstPylonSrc * src)
{
  if (is_prop_not_set (src, PROP_BASLERDEMOSAICING)) {
    const char *demosaicing =
        feature_alias_readable (src, featDemosaicing[0].name, featDemosaicing[1].name);
    if (demosaicing != NULL) {
      const ptrdiff_t idx = (demosaicing == featDemosaicing[0].name) ? 0 : 1;
      const char *pgiOn = featDemosaicing[idx].on;
      char *temp = read_string_feature (src, demosaicing);
      if ((temp != NULL) && (strcmp (temp, pgiOn))) {
        src->demosaicing = TRUE;
      } else {
        src->demosaicing = FALSE;
      }
      g_free (temp);
    } else {
      src->demosaicing = FALSE;
    }
  }

  if (is_prop_not_set (src, PROP_DEMOSAICINGNOISEREDUCTION)) {
    read_float_feature_alias (src, "NoiseReduction", "NoiseReductionAbs",
        &src->noisereduction);
  }

  if (is_prop_not_set (src, PROP_DEMOSAICINGSHARPNESSENHANCEMENT)) {
    read_float_feature_alias (src,
        "SharpnessEnhancement", "SharpnessEnhancementAbs",
        &src->sharpnessenhancement);
  }
}

static void
gst_pylonsrc_read_trigger_selector_mode (GstPylonSrc * src,
    const char *trigger_selector)
{
  GST_DEBUG_OBJECT (src, "Reading trigger selector mode: %s", trigger_selector);
  GENAPIC_RESULT res =
      PylonDeviceFeatureFromString (src->deviceHandle, "TriggerSelector",
      trigger_selector);
  if (res == GENAPI_E_OK) {
    char *temp = read_string_feature (src, "TriggerMode");
    GST_DEBUG_OBJECT (src, "Trigger mode: %s", temp);
    if ((temp != NULL) && (strcmp (temp, "On") == 0)) {
      src->continuousMode = FALSE;
    } else {
      src->continuousMode = TRUE;
    }
    g_free (temp);
  } else {
    GST_WARNING_OBJECT (src,
        "Failed to get TriggerSelector. Assuming continuous acquisition");
    src->continuousMode = TRUE;
  }
}

static void
gst_pylonsrc_read_trigger (GstPylonSrc * src)
{
  if (is_prop_not_set (src, PROP_CONTINUOUSMODE)) {
    _Bool isAvailAcquisitionStart =
        PylonDeviceFeatureIsAvailable (src->deviceHandle,
        "EnumEntry_TriggerSelector_AcquisitionStart");
    _Bool isAvailFrameStart = PylonDeviceFeatureIsAvailable (src->deviceHandle,
        "EnumEntry_TriggerSelector_FrameStart");

    if (isAvailAcquisitionStart && !isAvailFrameStart) {
      gst_pylonsrc_read_trigger_selector_mode (src, "AcquisitionStart");
    } else {
      gst_pylonsrc_read_trigger_selector_mode (src, "FrameStart");
    }
  }
}

static void
gst_pylonsrc_read_resolution_axis (GstPylonSrc * src, GST_PYLONSRC_AXIS axis)
{
  if (is_prop_not_set (src, propBinning[axis])) {
    int64_t temp;
    GENAPIC_RESULT res = read_integer_feature (src, featBinning[axis], &temp);
    if (res == GENAPI_E_OK) {
      src->binning[axis] = temp;
    }
  }

  if (is_prop_not_set (src, propSize[axis])) {
    int64_t temp;
    GENAPIC_RESULT res = read_integer_feature (src, featSize[axis], &temp);
    if (res == GENAPI_E_OK) {
      src->size[axis] = temp;
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
          ("Failed to initialise the camera"),
          ("Camera isn't reporting it's resolution. (Unsupported device?)"));
    }

    src->maxSize[axis] = (gint) gst_pylonsrc_read_max_size_axis (src, axis);
  }
}

static void
gst_pylonsrc_read_resolution (GstPylonSrc * src)
{
  gst_pylonsrc_read_resolution_axis (src, AXIS_X);
  gst_pylonsrc_read_resolution_axis (src, AXIS_Y);
}

// read all features from device to plugin struct
static void
read_all_features (GstPylonSrc * src)
{
  gst_pylonsrc_read_offset (src);
  gst_pylonsrc_read_reverse (src);
  gst_pylonsrc_read_pixel_format (src);
  gst_pylonsrc_read_test_image (src);
  gst_pylonsrc_read_readout (src);
  gst_pylonsrc_read_bandwidth (src);
  gst_pylonsrc_read_framerate (src);
  gst_pylonsrc_read_lightsource (src);
  gst_pylonsrc_read_auto_exp_gain_wb (src);
  gst_pylonsrc_read_color (src);
  gst_pylonsrc_read_exposure_gain_level (src);
  gst_pylonsrc_read_pgi (src);
  gst_pylonsrc_read_trigger (src);
  gst_pylonsrc_read_resolution (src);
}

// Load features from PFS file to device
static _Bool
gst_pylonsrc_load_configuration (GstPylonSrc * src)
{
  if (is_prop_set (src, PROP_CONFIGFILE)) {
    GST_DEBUG_OBJECT (src, "Loading features from file: %s", src->configFile);
    NODEMAP_HANDLE hMap;
    GENAPIC_RESULT res = PylonDeviceGetNodeMap (src->deviceHandle, &hMap);
    PYLONC_CHECK_ERROR (src, res);
    // Do not verify features. This makes plugin more robust in case of floating point values being rounded
    // For example trying to set AcquisitionFrameRate to 300 will result in 300.030003 and verification failure
    res = PylonFeaturePersistenceLoad (hMap, src->configFile, FALSE);
    PYLONC_CHECK_ERROR (src, res);
    reset_prop (src, PROP_CONFIGFILE);
  }

  if (is_prop_set (src, PROP_IGNOREDEFAULTS)) {
    if (src->ignoreDefaults) {
      GST_DEBUG_OBJECT (src, "Ignoring defaults");
      for (int i = 0; i < GST_PYLONSRC_NUM_PROPS; i++) {
        const GST_PYLONSRC_PROP prop = (GST_PYLONSRC_PROP) i;
        if (is_prop_default (src, prop)) {
          reset_prop (src, prop);
        }
      }
      read_all_features (src);
    }
    reset_prop (src, PROP_IGNOREDEFAULTS);
  }

  return TRUE;
error:
  return FALSE;
}

static _Bool
gst_pylonsrc_set_properties (GstPylonSrc * src)
{
  return gst_pylonsrc_load_configuration (src) &&       // make sure configuration is loaded first
      gst_pylonsrc_set_resolution (src) &&      // make sure resolution is set before offset
      gst_pylonsrc_set_offset (src) &&
      gst_pylonsrc_set_reverse (src) &&
      gst_pylonsrc_set_pixel_format (src) &&
      gst_pylonsrc_set_test_image (src) &&
      gst_pylonsrc_set_readout (src) &&
      gst_pylonsrc_set_bandwidth (src) &&
      gst_pylonsrc_set_framerate (src) &&
      gst_pylonsrc_set_lightsource (src) &&
      gst_pylonsrc_set_auto_exp_gain_wb (src) &&
      gst_pylonsrc_set_color (src) &&
      gst_pylonsrc_set_exposure_gain_level (src) &&
      gst_pylonsrc_set_pgi (src) && gst_pylonsrc_set_trigger (src);
}

static gboolean
gst_pylonsrc_start (GstBaseSrc * bsrc)
{
  GstPylonSrc *src = GST_PYLONSRC (bsrc);

  const int count = gst_pylonsrc_ref_pylon_environment ();
  if (count <= 0) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to initialise the camera"),
        ("Pylon library initialization failed"));
    goto error;
  } else if (count == 1) {
    GST_DEBUG_OBJECT (src, "First object created");
  }


  if (!gst_pylonsrc_select_device (src) ||
      !gst_pylonsrc_connect_device (src) || !gst_pylonsrc_set_properties (src))
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

  if (!gst_pylonsrc_set_properties (src)) {
    // TODO: Maybe just shot warning if setting is not critical
    goto error;
  }

  if (!src->acquisition_configured) {
    if (!gst_pylonsrc_configure_start_acquisition (src))
      goto error;
    src->acquisition_configured = TRUE;
  }
  // Wait for the buffer to be filled  (up to 1 s)  
  res = PylonWaitObjectWait (src->waitObject, 1000, &bufferReady);
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
  if (grabResult.Status == Grabbed) {
    VideoFrame *vf = (VideoFrame *) g_malloc0 (sizeof (VideoFrame));

    *buf =
        gst_buffer_new_wrapped_full ((GstMemoryFlags) GST_MEMORY_FLAG_READONLY,
        (gpointer) grabResult.pBuffer, src->payloadSize, 0, src->payloadSize,
        vf, (GDestroyNotify) video_frame_free);

    vf->buffer_handle = grabResult.hBuffer;
    vf->src = src;
  } else {
    GST_ERROR_OBJECT (src, "Error in the image processing loop. Status=%d",
        grabResult.Status);
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

  g_free (src->pixel_format);
  g_free (src->sensorMode);
  g_free (src->lightsource);

  for (int i = 0; i < AUTOF_NUM_FEATURES; i++) {
    g_free (src->autoFeature[i]);
  }

  g_free (src->reset);
  g_free (src->autoprofile);
  g_free (src->transformationselector);
  g_free (src->userid);
  g_free (src->configFile);


  if (gst_pylonsrc_unref_pylon_environment () == 0) {
    GST_DEBUG_OBJECT (src, "Last object finalized");
  }

  G_OBJECT_CLASS (gst_pylonsrc_parent_class)->finalize (object);
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
    if(siz <= 2 || res != GENAPI_E_OK) {
      if(PylonDeviceFeatureIsReadable(deviceHandle, "DeviceID")) {
        siz = sizeof (serial);
        res =
          PylonDeviceFeatureToString (deviceHandle, "DeviceID", serial,
          &siz);
      } else {
        serial[0] = '0';
        serial[1] = '\0';
      }
    }

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
