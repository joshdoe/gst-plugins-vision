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
 * SECTION:element-gstedt_pdv_src
 *
 * The edtpdvsrc element is a source for EDT framegrabbers supported by the EDT PDV library.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v edtpdvsrc ! ffmpegcolorspace ! autovideosink
 * ]|
 * Shows video from the default camera source (unit 0, channel 0).
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#include "gstedtpdvsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_edt_pdv_src_debug);
#define GST_CAT_DEFAULT gst_edt_pdv_src_debug

/* prototypes */
static void gst_edt_pdv_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_edt_pdv_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_edt_pdv_src_dispose (GObject * object);
static void gst_edt_pdv_src_finalize (GObject * object);

static gboolean gst_edt_pdv_src_start (GstBaseSrc * src);
static gboolean gst_edt_pdv_src_stop (GstBaseSrc * src);
static GstCaps *gst_edt_pdv_src_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_edt_pdv_src_set_caps (GstBaseSrc * src, GstCaps * caps);

static GstFlowReturn gst_edt_pdv_src_create (GstPushSrc * src,
    GstBuffer ** buf);

static GstCaps *gst_edt_pdv_src_create_caps (GstEdtPdvSrc * src);
static void gst_edt_pdv_src_reset (GstEdtPdvSrc * src);
enum
{
  PROP_0,
  PROP_UNIT,
  PROP_CHANNEL,
  PROP_CONFIG_FILE,
  PROP_NUM_RING_BUFFERS
};

#define DEFAULT_PROP_UNIT 0
#define DEFAULT_PROP_CHANNEL 0
#define DEFAULT_PROP_NUM_RING_BUFFERS 4

/* pad templates */

static GstStaticPadTemplate gst_edt_pdv_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, GRAY16_BE, RGB }"))
    );

/* class initialization */

G_DEFINE_TYPE (GstEdtPdvSrc, gst_edt_pdv_src, GST_TYPE_PUSH_SRC);

static void
gst_edt_pdv_src_class_init (GstEdtPdvSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "edtpdvsrc", 0,
      "EDT PDV Camera Link source");

  gobject_class->set_property = gst_edt_pdv_src_set_property;
  gobject_class->get_property = gst_edt_pdv_src_get_property;
  gobject_class->dispose = gst_edt_pdv_src_dispose;
  gobject_class->finalize = gst_edt_pdv_src_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_edt_pdv_src_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "EDT PDV Video Source", "Source/Video",
      "EDT PDV framegrabber video source", "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_edt_pdv_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_edt_pdv_src_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_edt_pdv_src_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_edt_pdv_src_set_caps);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_edt_pdv_src_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_UNIT,
      g_param_spec_uint ("unit", "Unit", "Unit number", 0, G_MAXUINT,
          DEFAULT_PROP_UNIT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_uint ("channel", "Channel", "Channel number (0 for auto)", 0,
          G_MAXUINT, DEFAULT_PROP_CHANNEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_CONFIG_FILE,
      g_param_spec_string ("config-file", "Config file",
          "Camera configuration path (empty or NULL to use previous config)",
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_NUM_RING_BUFFERS,
      g_param_spec_uint ("num-ring-buffers", "Number of ring buffers",
          "Number of ring buffers to use for DMAing frames from card", 1,
          G_MAXUINT, DEFAULT_PROP_NUM_RING_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
}

static void
gst_edt_pdv_src_init (GstEdtPdvSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize properties */
  src->unit = DEFAULT_PROP_UNIT;
  src->channel = DEFAULT_PROP_CHANNEL;
  src->config_file_path = NULL;
  src->num_ring_buffers = DEFAULT_PROP_NUM_RING_BUFFERS;

  gst_edt_pdv_src_reset (src);
}

static void
gst_edt_pdv_src_reset (GstEdtPdvSrc * src)
{
  src->dev = NULL;
  src->total_timeouts = 0;
  src->acq_started = FALSE;
}

void
gst_edt_pdv_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstEdtPdvSrc *src;

  src = GST_EDT_PDV_SRC (object);

  switch (property_id) {
    case PROP_UNIT:
      src->unit = g_value_get_uint (value);
      break;
    case PROP_CHANNEL:
      src->channel = g_value_get_uint (value);
      break;
    case PROP_CONFIG_FILE:
      if (src->config_file_path) {
        g_free (src->config_file_path);
      }
      src->config_file_path = g_strdup (g_value_get_string (value));
      break;
    case PROP_NUM_RING_BUFFERS:
      src->num_ring_buffers = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_edt_pdv_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstEdtPdvSrc *src;

  g_return_if_fail (GST_IS_EDT_PDV_SRC (object));
  src = GST_EDT_PDV_SRC (object);

  switch (property_id) {
    case PROP_UNIT:
      g_value_set_uint (value, src->unit);
      break;
    case PROP_CHANNEL:
      g_value_set_uint (value, src->channel);
      break;
    case PROP_CONFIG_FILE:
      g_value_set_string (value, src->config_file_path);
      break;
    case PROP_NUM_RING_BUFFERS:
      g_value_set_uint (value, src->num_ring_buffers);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_edt_pdv_src_dispose (GObject * object)
{
  GstEdtPdvSrc *src;

  g_return_if_fail (GST_IS_EDT_PDV_SRC (object));
  src = GST_EDT_PDV_SRC (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_edt_pdv_src_parent_class)->dispose (object);
}

void
gst_edt_pdv_src_finalize (GObject * object)
{
  GstEdtPdvSrc *src;

  g_return_if_fail (GST_IS_EDT_PDV_SRC (object));
  src = GST_EDT_PDV_SRC (object);

  /* clean up object here */

  G_OBJECT_CLASS (gst_edt_pdv_src_parent_class)->finalize (object);
}

static gboolean
gst_edt_pdv_src_start (GstBaseSrc * bsrc)
{
  GstEdtPdvSrc *src = GST_EDT_PDV_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "start");

  if (src->config_file_path && strlen (src->config_file_path)) {
    Dependent *dd_p;
    Edtinfo edtinfo;
    EdtDev *edt_p;
    char bitdir[256];
    *bitdir = '\0';

    dd_p = pdv_alloc_dependent ();
    g_assert (dd_p != NULL);

    if (pdv_readcfg (src->config_file_path, dd_p, &edtinfo)) {
      GST_ERROR_OBJECT (src, "Failed to read config file: '%s'",
          src->config_file_path);
      goto fail;
    }

    edt_p = edt_open_channel (EDT_INTERFACE, src->unit, src->channel);
    if (edt_p == NULL) {
      GST_ERROR_OBJECT (src, "Failed to open channel to perform configuration");
      edt_perror ("error message");
      free (dd_p);
      goto fail;
    }

    if (pdv_initcam (edt_p, dd_p, src->unit, &edtinfo, src->config_file_path,
            bitdir, 0)) {
      GST_ERROR_OBJECT (src, "Failed to initialize camera");
      free (dd_p);
      edt_close (edt_p);
      goto fail;
    }

    edt_close (edt_p);
  }

  src->dev = pdv_open_channel (EDT_INTERFACE, src->unit, src->channel);
  if (src->dev == NULL) {
    GST_ERROR_OBJECT (src, "Failed to open EDT PDV unit %d channel %d)",
        src->unit, src->channel);
    pdv_perror ("error message");
    goto fail;
  }

  if (pdv_multibuf (src->dev, src->num_ring_buffers)) {
    GST_ERROR_OBJECT (src, "Failed to setup ring buffer");
    goto fail;
  }

  return TRUE;

fail:
  if (src->dev) {
    pdv_close (src->dev);
    src->dev = NULL;
  }
  return FALSE;
}

static gboolean
gst_edt_pdv_src_stop (GstBaseSrc * bsrc)
{
  GstEdtPdvSrc *src = GST_EDT_PDV_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "stop");

  g_assert (src->dev != NULL);
  if (pdv_close (src->dev)) {
    GST_ERROR_OBJECT (src, "Failed to close device");
    return FALSE;
  }

  gst_edt_pdv_src_reset (src);

  return TRUE;
}

static GstCaps *
gst_edt_pdv_src_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstEdtPdvSrc *src = GST_EDT_PDV_SRC (bsrc);
  GstCaps *caps;

  if (src->dev == NULL) {
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
  } else {
    gint width, height, depth;
    GstVideoInfo vinfo;

    /* Create video info */
    gst_video_info_init (&vinfo);

    width = pdv_get_width (src->dev);
    height = pdv_get_height (src->dev);
    depth = pdv_get_depth (src->dev);

    if (depth <= 8) {
      gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_GRAY8, width, height);
      caps = gst_video_info_to_caps (&vinfo);
    } else if (depth <= 16) {
      GValue val = G_VALUE_INIT;
      GstStructure *s;

      /* TODO: check endianness */
      gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_GRAY16_LE, width,
          height);
      caps = gst_video_info_to_caps (&vinfo);

      /* set bpp, extra info for GRAY16 so elements can scale properly */
      s = gst_caps_get_structure (caps, 0);
      g_value_init (&val, G_TYPE_INT);
      g_value_set_int (&val, depth);
      gst_structure_set_value (s, "bpp", &val);
      g_value_unset (&val);
    } else if (depth == 24) {
      gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_RGB, width, height);
      caps = gst_video_info_to_caps (&vinfo);
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
gst_edt_pdv_src_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstEdtPdvSrc *src = GST_EDT_PDV_SRC (bsrc);
  GstVideoInfo vinfo;
  GstStructure *s = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);

  gst_video_info_from_caps (&vinfo, caps);

  if (GST_VIDEO_INFO_FORMAT (&vinfo) != GST_VIDEO_FORMAT_UNKNOWN) {
    g_assert (src->dev != NULL);
    src->edt_stride = pdv_get_pitch (src->dev);
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
gst_edt_pdv_src_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstEdtPdvSrc *src = GST_EDT_PDV_SRC (psrc);
  GstMapInfo minfo;
  guint8 *image;
  gint timeouts;

  if (!src->acq_started) {
    /* start freerun/continuous capture */
    pdv_start_images (src->dev, 0);
    src->acq_started = TRUE;
  }

  /* TODO: any way to know if this particular image is good? */
  /* TODO: use pdv_ wait_image_timed to get rough timestamp */
  image = pdv_wait_image (src->dev);

  /* TODO: if there are timeouts, do we drop frame, return GST_FLOW_FATAL? */
  timeouts = pdv_timeouts (src->dev);
  if (timeouts > src->total_timeouts) {
    GST_WARNING_OBJECT (src,
        "Received timeout, data might be incomplete. Check cables and system bandwidth.");
    src->total_timeouts = timeouts;

    /* TODO: perhaps call twice as in take.c to be more robust */
    pdv_timeout_restart (src->dev, TRUE);
  }

  /* TODO: use allocator */
  *buf = gst_buffer_new_and_alloc (src->height * src->gst_stride);

  /* Copy image to buffer from surface TODO: use orc_memcpy */
  gst_buffer_map (*buf, &minfo, GST_MAP_WRITE);

  if (src->gst_stride == src->edt_stride) {
    memcpy (minfo.data, image, minfo.size);
  } else {
    int i;
    GST_LOG_OBJECT (src, "Stride not a multiple of 4, extra copy needed");
    for (i = 0; i < src->height; i++) {
      memcpy (minfo.data + i * src->gst_stride,
          image + i * src->edt_stride, src->edt_stride);
    }
  }
  gst_buffer_unmap (*buf, &minfo);


  return GST_FLOW_OK;
}
