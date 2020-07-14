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

#define VIDEO_CAPS_MAKE_BAYER8(format)                     \
  "video/x-bayer, "                                        \
  "format = (string) " format ", "                         \
  "width = " GST_VIDEO_SIZE_RANGE ", "                     \
  "height = " GST_VIDEO_SIZE_RANGE ", "                    \
  "framerate = " GST_VIDEO_FPS_RANGE

#define VIDEO_CAPS_MAKE_BAYER16(format)                    \
  "video/x-bayer, "                                        \
  "format = (string) " format ", "                         \
  "endianness = (int) 1234, "                              \
  "bpp = (int) {16, 14, 12, 10}, "                         \
  "width = " GST_VIDEO_SIZE_RANGE ", "                     \
  "height = " GST_VIDEO_SIZE_RANGE ", "                    \
  "framerate = " GST_VIDEO_FPS_RANGE

enum
{
  PROP_0,
  PROP_SYSTEM,
  PROP_BOARD,
  PROP_CHANNEL,
  PROP_CONFIG_FILE,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_TIMEOUT,
  PROP_BAYER_MODE
};

#define DEFAULT_PROP_SYSTEM 0
#define DEFAULT_PROP_BOARD -1
#define DEFAULT_PROP_CHANNEL -1
#define DEFAULT_PROP_CONFIG_FILE "M_DEFAULT"
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 2
#define DEFAULT_PROP_TIMEOUT 1000
#define DEFAULT_PROP_BAYER_MODE GST_MATROX_BAYER_MODE_BAYER


#define GST_TYPE_MATROX_BAYER_MODE (gst_matrox_bayer_mode_get_type())
static GType
gst_matrox_bayer_mode_get_type (void)
{
  static GType matrox_bayer_mode_type = 0;
  static const GEnumValue matrox_bayer_mode[] = {
    {GST_MATROX_BAYER_MODE_BAYER, "bayer", "Get raw bayer"},
    {GST_MATROX_BAYER_MODE_GRAY, "gray", "Get raw bayer as grayscale"},
    {GST_MATROX_BAYER_MODE_RGB, "rgb", "Get demosaiced RGB"},
    {0, NULL, NULL},
  };

  if (!matrox_bayer_mode_type) {
    matrox_bayer_mode_type =
        g_enum_register_static ("GstMatroxBayerMode", matrox_bayer_mode);
  }
  return matrox_bayer_mode_type;
}

static const GEnumValue matrox_system_enum[] = {
  {0, "Default from MilConfig", "M_SYSTEM_DEFAULT"},
  {1, "Clarity UHD", "M_SYSTEM_CLARITY_UHD"},
  {2, "GigE Vision", "M_SYSTEM_GIGE_VISION"},
  {3, "Host", "M_SYSTEM_HOST"},
  {4, "Iris GTR", "M_SYSTEM_IRIS_GTR"},
  {5, "Morphis Dual/Quad", "M_SYSTEM_MORPHIS"},
  {6, "Morphis QxT", "M_SYSTEM_MORPHISQXT"},
  {7, "Orion HD", "M_SYSTEM_ORION_HD"},
  {8, "Radient eCL", "M_SYSTEM_RADIENT"},
  {9, "Radient eV-CL", "M_SYSTEM_RADIENTEVCL"},
  {10, "Radient eV-CLHS", "M_SYSTEM_RADIENTCLHS"},
  {11, "Radient eV-CXP", "M_SYSTEM_RADIENTCXP"},
  {12, "RadientPro CL", "M_SYSTEM_RADIENTPRO"},
  {13, "Rapixo CXP", "M_SYSTEM_RAPIXOCXP"},
  {14, "Solios", "M_SYSTEM_SOLIOS"},
  {15, "USB3 Vision", "M_SYSTEM_USB3_VISION"},
  {0, NULL, NULL},
};

#define GST_TYPE_MATROX_SYSTEM (gst_matrox_system_get_type())
static GType
gst_matrox_system_get_type (void)
{
  static GType matrox_system_type = 0;


  if (!matrox_system_type) {
    matrox_system_type =
        g_enum_register_static ("GstMatroxSystem", matrox_system_enum);
  }
  return matrox_system_type;
}


/* pad templates */

static GstStaticPadTemplate gst_matroxsrc_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, GRAY16_BE, BGRA }") ";"
        VIDEO_CAPS_MAKE_BAYER8 ("{ bggr, grbg, rggb, gbrg }") ";"
        VIDEO_CAPS_MAKE_BAYER16 ("{ bggr, grbg, rggb, gbrg }")
    )
    );

/* class initialization */

G_DEFINE_TYPE (GstMatroxSrc, gst_matroxsrc, GST_TYPE_PUSH_SRC);

static MIL_ID g_milapp = M_NULL;
static int g_milapp_use_count = 0;

/* Matrox only wants an Application object to be created once per process,
 * and it must be freed in the same thread it was created in */
static MIL_ID
gst_matroxsrc_milapp_get ()
{
  if (g_once_init_enter (&g_milapp)) {
    MIL_ID setup_value = M_NULL;
    g_assert (g_milapp_use_count == 0);
    MappAlloc (M_NULL, M_DEFAULT, &setup_value);
    g_once_init_leave (&g_milapp, setup_value);
  }
  g_milapp_use_count += 1;
  return g_milapp;
}

static void
gst_matroxsrc_milapp_unref ()
{
  g_milapp_use_count--;
  if (g_milapp_use_count == 0) {
    MappFree (g_milapp);
    g_milapp = M_NULL;
  }
}


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
  g_object_class_install_property (gobject_class, PROP_SYSTEM,
      g_param_spec_enum ("system", "System",
          "System descriptor, default is specified in MilConfig",
          GST_TYPE_MATROX_SYSTEM, DEFAULT_PROP_SYSTEM,
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
  g_object_class_install_property (gobject_class, PROP_CONFIG_FILE,
      g_param_spec_string ("config-file", "Format or format file",
          "Format, as predefined string or DCF file path, default is specified in MilConfig",
          DEFAULT_PROP_CONFIG_FILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_uint ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 1, G_MAXUINT,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TIMEOUT,
      g_param_spec_int ("timeout", "Timeout (ms)",
          "Timeout in ms (0 to use default)", 0, G_MAXINT, DEFAULT_PROP_TIMEOUT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BAYER_MODE,
      g_param_spec_enum ("bayer-mode", "Bayer mode",
          "Pull Bayer frames as raw bayer, grayscale, or demosaiced RGB",
          GST_TYPE_MATROX_BAYER_MODE, DEFAULT_PROP_BAYER_MODE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
}

static void
gst_matroxsrc_reset (GstMatroxSrc * src)
{
  gint i;
  src->acq_started = FALSE;

  src->height = 0;
  src->gst_stride = 0;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }
  if (src->buffer) {
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }

  if (src->MilGrabBufferList) {
    for (i = 0; i < src->num_capture_buffers; ++i) {
      if (src->MilGrabBufferList[i]) {
        MbufFree (src->MilGrabBufferList[i]);
        src->MilGrabBufferList[i] = NULL;
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
}

static void
gst_matroxsrc_init (GstMatroxSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->system = DEFAULT_PROP_SYSTEM;
  src->board = DEFAULT_PROP_BOARD;
  src->channel = DEFAULT_PROP_CHANNEL;
  src->config_file = g_strdup (DEFAULT_PROP_CONFIG_FILE);
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;
  src->timeout = DEFAULT_PROP_TIMEOUT;
  src->bayer_mode = DEFAULT_PROP_BAYER_MODE;

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

  if (src->MilApplication == M_NULL) {
    src->MilApplication = gst_matroxsrc_milapp_get ();
  }
}

void
gst_matroxsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMatroxSrc *src;

  src = GST_MATROX_SRC (object);

  switch (property_id) {
    case PROP_SYSTEM:
      src->system = g_value_get_enum (value);
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
    case PROP_CONFIG_FILE:
      g_free (src->config_file);
      src->config_file = g_strdup (g_value_get_string (value));
      break;
    case PROP_TIMEOUT:
      src->timeout = g_value_get_int (value);
      break;
    case PROP_BAYER_MODE:
      src->bayer_mode = g_value_get_enum (value);
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
    case PROP_SYSTEM:
      g_value_set_enum (value, src->system);
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
    case PROP_CONFIG_FILE:
      g_value_set_string (value, src->config_file);
      break;
    case PROP_TIMEOUT:
      g_value_set_int (value, src->timeout);
      break;
    case PROP_BAYER_MODE:
      g_value_set_enum (value, src->bayer_mode);
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
  g_free (src->config_file);

  gst_matroxsrc_reset (src);

  gst_matroxsrc_milapp_unref ();

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
  gint bayer_pattern;
  gchar bayer_pattern_format[5];
  gint is_bayer_conversion;
  GstVideoInfo vinfo;

  GST_DEBUG_OBJECT (src, "start");

  if (src->MilApplication == M_NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to allocate a MIL application"), (NULL));
    return FALSE;
  }

  /* create System */
  if (src->board == -1) {
    ret =
        MsysAlloc (src->MilApplication,
        matrox_system_enum[src->system].value_nick, M_DEFAULT, M_DEFAULT,
        &src->MilSystem);
  } else {
    ret =
        MsysAlloc (src->MilApplication,
        matrox_system_enum[src->system].value_nick, src->board, M_DEFAULT,
        &src->MilSystem);
  }
  if (ret == M_NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to allocate a MIL system"), (NULL));
    return FALSE;
  }

  /* create Digitizer */
  ret = MdigAlloc (src->MilSystem, M_DEFAULT, src->config_file, M_DEFAULT,
      &src->MilDigitizer);
  if (ret == M_NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to allocate a MIL digitizer"), (NULL));
    return FALSE;
  }

  /* get format info and create caps */
  bayer_pattern = MdigInquire (src->MilDigitizer, M_BAYER_PATTERN, M_NULL);
  is_bayer_conversion =
      MdigInquire (src->MilDigitizer, M_BAYER_CONVERSION, M_NULL);
  if (bayer_pattern != M_NULL) {
    if (src->bayer_mode == GST_MATROX_BAYER_MODE_RGB) {
      MdigControl (src->MilDigitizer, M_BAYER_CONVERSION, M_ENABLE);
    } else {
      MdigControl (src->MilDigitizer, M_BAYER_CONVERSION, M_DISABLE);
    }

    switch (bayer_pattern) {
      case M_BAYER_BG:
        g_strlcpy (bayer_pattern_format, "bggr", 5);
        break;
      case M_BAYER_GB:
        g_strlcpy (bayer_pattern_format, "gbrg", 5);
        break;
      case M_BAYER_GR:
        g_strlcpy (bayer_pattern_format, "grbg", 5);
        break;
      case M_BAYER_RG:
        g_strlcpy (bayer_pattern_format, "rggb", 5);
        break;
      default:
        g_assert_not_reached ();
    }
  }

  width = MdigInquire (src->MilDigitizer, M_SIZE_X, M_NULL);
  height = MdigInquire (src->MilDigitizer, M_SIZE_Y, M_NULL);
  bpp = MdigInquire (src->MilDigitizer, M_SIZE_BIT, M_NULL);
  n_bands = MdigInquire (src->MilDigitizer, M_SIZE_BAND, M_NULL);
  src->mil_type = MdigInquire (src->MilDigitizer, M_TYPE, M_NULL);

  gst_video_info_init (&vinfo);

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  /* bayer is described as 3 bands even before demosaic */
  if (n_bands == 1) {
    if (bpp == 8) {
      src->video_format = GST_VIDEO_FORMAT_GRAY8;

      if (bayer_pattern != M_NULL
          && src->bayer_mode == GST_MATROX_BAYER_MODE_BAYER) {
        src->caps =
            gst_caps_new_simple ("video/x-bayer", "format", G_TYPE_STRING,
            bayer_pattern_format, "width", G_TYPE_INT, width, "height",
            G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1,
            G_MAXINT, 1, NULL);
      }
    } else if (bpp > 8 && bpp <= 16) {
      src->video_format = GST_VIDEO_FORMAT_GRAY16_LE;

      if (bayer_pattern != M_NULL
          && src->bayer_mode == GST_MATROX_BAYER_MODE_BAYER) {
        src->caps =
            gst_caps_new_simple ("video/x-bayer", "format", G_TYPE_STRING,
            bayer_pattern_format, "bpp", G_TYPE_INT, bpp, "endianness",
            G_TYPE_INT, G_LITTLE_ENDIAN, "width", G_TYPE_INT, width, "height",
            G_TYPE_INT, height, "framerate", GST_TYPE_FRACTION_RANGE, 0, 1,
            G_MAXINT, 1, NULL);
      } else {
        GValue val = G_VALUE_INIT;
        GstStructure *s;
        src->video_format = GST_VIDEO_FORMAT_GRAY16_LE;
        gst_video_info_set_format (&vinfo, src->video_format, width, height);
        src->caps = gst_video_info_to_caps (&vinfo);

        /* set bpp, extra info for GRAY16 so elements can scale properly */
        s = gst_caps_get_structure (src->caps, 0);
        g_value_init (&val, G_TYPE_INT);
        g_value_set_int (&val, bpp);
        gst_structure_set_value (s, "bpp", &val);
        g_value_unset (&val);
      }
    } else {
      GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
          ("Unknown or unsupported bit depth (%d).", bpp), (NULL));
      return FALSE;
    }
  } else if (n_bands == 3) {
    /* TODO: handle non-Solios color formats */
    src->video_format = GST_VIDEO_FORMAT_BGRx;
  }

  /* note that we abuse formats with Bayer */
  gst_video_info_set_format (&vinfo, src->video_format, width, height);

  if (!src->caps) {
    src->caps = gst_video_info_to_caps (&vinfo);
  }

  src->height = height;
  src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);

  g_assert (src->MilGrabBufferList == NULL);
  src->MilGrabBufferList = g_new (MIL_ID, src->num_capture_buffers);
  for (i = 0; i < src->num_capture_buffers; i++) {
    if (n_bands == 1) {
      MbufAlloc2d (src->MilSystem, width, height, src->mil_type,
          M_IMAGE + M_GRAB + M_PROC, &src->MilGrabBufferList[i]);
    } else {
      MbufAllocColor (src->MilSystem,
          n_bands,
          width,
          height,
          src->mil_type, M_IMAGE + M_GRAB + M_PROC + M_PACKED + M_BGR32,
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

  GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);

  return TRUE;
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
    MbufGetColor (buffer_id, M_PACKED | M_BGR32, M_ALL_BANDS, minfo.data);
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
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
