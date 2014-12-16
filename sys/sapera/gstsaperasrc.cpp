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
 * SECTION:element-gstsaperasrc
 *
 * The saperasrc element is a source for Teledyne DALSA Sapera framegrabbers.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v saperasrc ! ffmpegcolorspace ! autovideosink
 * ]|
 * Shows video from the default DALSA framegrabber
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/video.h>

#include "gstsaperasrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_saperasrc_debug);
#define GST_CAT_DEFAULT gst_saperasrc_debug

gboolean gst_saperasrc_create_objects (GstSaperaSrc * src);
gboolean gst_saperasrc_destroy_objects (GstSaperaSrc * src);

class SapMyProcessing:public SapProcessing
{
public:
  SapMyProcessing (SapBuffer * pBuffers, SapProCallback pCallback,
      void *pContext)
  : SapProcessing (pBuffers, pCallback, pContext)
  {
    src = (GstSaperaSrc *) pContext;
  }

  virtual ~ SapMyProcessing ()
  {
    if (m_bInitOK)
      Destroy ();
  }

protected:
  virtual BOOL Run () {
    // TODO: handle bayer
    //if (src->sap_bayer->IsEnabled () && src->sap_bayer->IsSoftware ()) {
    //    src->sap_bayer->Convert (GetIndex());
    //}

    push_buffer ();

    return TRUE;
  }

  gboolean push_buffer ()
  {
    void *pData;
    GstMapInfo minfo;

    // TODO: check for failure
    src->sap_buffers->GetAddress (&pData);
    int pitch = src->sap_buffers->GetPitch ();
    int height = src->sap_buffers->GetHeight ();
    gssize size = pitch * height;

    GstBuffer *buf;
    /* create a new buffer assign to it the clock time as timestamp */
    buf = gst_buffer_new_and_alloc (size);

    gst_buffer_set_size (buf, size);

    GstClock *clock = gst_element_get_clock (GST_ELEMENT (src));
    GST_BUFFER_TIMESTAMP (buf) =
        GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
        gst_clock_get_time (clock));
    gst_object_unref (clock);

    // TODO: define duration?
    //GST_BUFFER_DURATION (buf) = duration;

    if (!gst_buffer_map (buf, &minfo, GST_MAP_WRITE)) {
      gst_buffer_unref (buf);
      GST_ERROR_OBJECT (src, "Failed to map buffer");
      return FALSE;
    }
    // TODO: optimize this
    if (pitch == src->gst_stride) {
      memcpy (minfo.data, pData, size);
    } else {
      for (int line = 0; line < src->height; line++) {
        memcpy (minfo.data + (line * src->gst_stride),
            (guint8 *) pData + (line * pitch), pitch);
      }
    }

    src->sap_buffers->ReleaseAddress (pData);

    gst_buffer_unmap (buf, &minfo);

    GST_DEBUG ("push_buffer => pts %" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)));

    g_mutex_lock (&src->buffer_mutex);
    if (src->buffer != NULL)
      gst_buffer_unref (src->buffer);
    src->buffer = buf;
    g_cond_signal (&src->buffer_cond);
    g_mutex_unlock (&src->buffer_mutex);

    return TRUE;
  }

protected:
  GstSaperaSrc * src;
};

void
gst_saperasrc_xfer_callback (SapXferCallbackInfo * pInfo)
{
  GstSaperaSrc *src = (GstSaperaSrc *) pInfo->GetContext ();

  if (pInfo->IsTrash ()) {
    /* TODO: update dropped buffer count */
  } else {
    /* Process current buffer */
    src->sap_pro->Execute ();
  }
}

void
gst_saperasrc_pro_callback (SapProCallbackInfo * pInfo)
{
  GstSaperaSrc *src = (GstSaperaSrc *) pInfo->GetContext ();

  /* TODO: handle buffer */
}

gboolean
gst_saperasrc_init_objects (GstSaperaSrc * src)
{
  char name[128];

  GST_DEBUG_OBJECT (src, "There are %d servers available",
      SapManager::GetServerCount ());

  if (!SapManager::GetServerName (src->server_index, name)) {
    GST_ERROR_OBJECT (src, "Invalid server index %d", src->server_index);
    return FALSE;
  }

  GST_DEBUG_OBJECT (src, "Trying to open server index %d ('%s')",
      src->server_index, name);

  GST_DEBUG_OBJECT (src, "Resource count: %d",
      SapManager::GetResourceCount (src->server_index,
          SapManager::ResourceAcq));

  if (!SapManager::GetResourceName (src->server_index, SapManager::ResourceAcq,
          src->resource_index, name, 128)) {
    GST_ERROR_OBJECT (src, "Invalid resource index %d", src->resource_index);
    return FALSE;
  }
  GST_DEBUG_OBJECT (src, "Trying to open resource '%s'", name);

  SapLocation loc (src->server_index, src->resource_index);
  src->sap_acq = new SapAcquisition (loc, src->format_file);
  /* TODO: allow configuring buffer count? */
  src->sap_buffers = new SapBufferWithTrash (3, src->sap_acq);
  src->sap_xfer =
      new SapAcqToBuf (src->sap_acq, src->sap_buffers,
      gst_saperasrc_xfer_callback, src);
  // TODO: handle bayer
  //src->sap_bayer = new SapBayer(m_Acq, m_Buffers);
  src->sap_pro =
      new SapMyProcessing (src->sap_buffers, gst_saperasrc_pro_callback, src);

  return TRUE;
}

gboolean
gst_saperasrc_create_objects (GstSaperaSrc * src)
{
  UINT32 video_type = 0;

  /* Create acquisition object */
  if (src->sap_acq && !*src->sap_acq && !src->sap_acq->Create ()) {
    gst_saperasrc_destroy_objects (src);
    return FALSE;
  }

  if (!src->sap_acq->GetParameter (CORACQ_PRM_VIDEO, &video_type)) {
    gst_saperasrc_destroy_objects (src);
    return FALSE;
  }

  /* TODO: handle Bayer
     //if (videoType != CORACQ_VAL_VIDEO_BAYER)

     // Enable/Disable bayer conversion
     // This call may require to modify the acquisition output format.
     // For this reason, it has to be done after creating the acquisition object but before
     // creating the output buffer object.
     //if( m_Bayer && !m_Bayer->Enable( m_BayerEnabled, m_BayerUseHardware))
     //{
     //    m_BayerEnabled= FALSE;
     //} */

  // Create buffer objects
  if (src->sap_buffers && !*src->sap_buffers) {
    if (!src->sap_buffers->Create ()) {
      gst_saperasrc_destroy_objects (src);
      return FALSE;
    }
    // Clear all buffers
    src->sap_buffers->Clear ();
  }

  /* TODO: handle Bayer
     // Create bayer object
     //if (m_Bayer && !*m_Bayer && !m_Bayer->Create())
     //{
     //    DestroyObjects();
     //    return FALSE;
     //} */

  /* Create transfer object */
  if (src->sap_xfer && !*src->sap_xfer) {
    if (!src->sap_xfer->Create ()) {
      gst_saperasrc_destroy_objects (src);
      return FALSE;
    }

    src->sap_xfer->SetAutoEmpty (FALSE);
  }

  /* Create processing object */
  if (src->sap_pro && !*src->sap_pro) {
    if (!src->sap_pro->Create ()) {
      gst_saperasrc_destroy_objects (src);
      return FALSE;
    }

    src->sap_pro->SetAutoEmpty (TRUE);
  }

  return TRUE;
}

gboolean
gst_saperasrc_destroy_objects (GstSaperaSrc * src)
{
  if (src->sap_xfer && *src->sap_xfer)
    src->sap_xfer->Destroy ();

  if (src->sap_pro && *src->sap_pro)
    src->sap_pro->Destroy ();

  // TODO: handle bayer
  //if (src->sap_bayer && *src->sap_bayer) src->sap_bayer->Destroy ();

  if (src->sap_buffers && *src->sap_buffers)
    src->sap_buffers->Destroy ();

  if (src->sap_acq && *src->sap_acq)
    src->sap_acq->Destroy ();

  return TRUE;
}

/* prototypes */
static void gst_saperasrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_saperasrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_saperasrc_dispose (GObject * object);
static void gst_saperasrc_finalize (GObject * object);

static gboolean gst_saperasrc_start (GstBaseSrc * src);
static gboolean gst_saperasrc_stop (GstBaseSrc * src);
static GstCaps *gst_saperasrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_saperasrc_set_caps (GstBaseSrc * src, GstCaps * caps);

static GstFlowReturn gst_saperasrc_create (GstPushSrc * src, GstBuffer ** buf);

static GstCaps *gst_saperasrc_create_caps (GstSaperaSrc * src);

enum
{
  PROP_0,
  PROP_FORMAT_FILE,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_SERVER_INDEX,
  PROP_RESOURCE_INDEX,
};

#define DEFAULT_PROP_FORMAT_FILE ""
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 2
#define DEFAULT_PROP_SERVER_INDEX 1
#define DEFAULT_PROP_RESOURCE_INDEX 0

/* pad templates */

static GstStaticPadTemplate gst_saperasrc_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, GRAY16_BE, BGR, BGRA }"))
    );

/* class initialization */

G_DEFINE_TYPE (GstSaperaSrc, gst_saperasrc, GST_TYPE_PUSH_SRC);

static void
gst_saperasrc_class_init (GstSaperaSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_saperasrc_set_property;
  gobject_class->get_property = gst_saperasrc_get_property;
  gobject_class->dispose = gst_saperasrc_dispose;
  gobject_class->finalize = gst_saperasrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_saperasrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Teledyne DALSA Sapera Video Source", "Source/Video",
      "Teledyne DALSA Sapera framegrabber video source",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_saperasrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_saperasrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_saperasrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_saperasrc_set_caps);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_saperasrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_FORMAT_FILE,
      g_param_spec_string ("format-file", "Format file",
          "Filepath of the video file for the selected camera "
          "(specify only one of format-name or format-file)",
          DEFAULT_PROP_FORMAT_FILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_uint ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 1, G_MAXUINT,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_SERVER_INDEX,
      g_param_spec_int ("server-index", "Server index",
          "Server (frame grabber card) index", 0, G_MAXINT,
          DEFAULT_PROP_SERVER_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_RESOURCE_INDEX,
      g_param_spec_int ("resource-index", "Resource index",
          "Resource index, such as different ports or configurations", 0,
          G_MAXINT, DEFAULT_PROP_RESOURCE_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void
gst_saperasrc_reset (GstSaperaSrc * src)
{
  src->dropped_frame_count = 0;
  src->last_buffer_number = 0;
  src->acq_started = FALSE;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }
  if (src->buffer) {
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }

  gst_saperasrc_destroy_objects (src);

  delete src->sap_acq;
  src->sap_acq = NULL;
  delete src->sap_buffers;
  src->sap_buffers = NULL;
  delete src->sap_bayer;
  src->sap_bayer = NULL;
  delete src->sap_xfer;
  src->sap_xfer = NULL;
  delete src->sap_pro;
  src->sap_pro = NULL;
}

static void
gst_saperasrc_init (GstSaperaSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->format_file = g_strdup (DEFAULT_PROP_FORMAT_FILE);
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;

  g_mutex_init (&src->buffer_mutex);
  g_cond_init (&src->buffer_cond);

  src->caps = NULL;
  src->buffer = NULL;

  gst_saperasrc_reset (src);
}

void
gst_saperasrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSaperaSrc *src;

  src = GST_SAPERA_SRC (object);

  switch (property_id) {
    case PROP_FORMAT_FILE:
      g_free (src->format_file);
      src->format_file = g_strdup (g_value_get_string (value));
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
    case PROP_SERVER_INDEX:
      src->server_index = g_value_get_int (value);
      break;
    case PROP_RESOURCE_INDEX:
      src->resource_index = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_saperasrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstSaperaSrc *src;

  g_return_if_fail (GST_IS_SAPERA_SRC (object));
  src = GST_SAPERA_SRC (object);

  switch (property_id) {
    case PROP_FORMAT_FILE:
      g_value_set_string (value, src->format_file);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      g_value_set_uint (value, src->num_capture_buffers);
      break;
    case PROP_SERVER_INDEX:
      g_value_set_int (value, src->server_index);
      break;
    case PROP_RESOURCE_INDEX:
      g_value_set_int (value, src->resource_index);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_saperasrc_dispose (GObject * object)
{
  GstSaperaSrc *src;

  g_return_if_fail (GST_IS_SAPERA_SRC (object));
  src = GST_SAPERA_SRC (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_saperasrc_parent_class)->dispose (object);
}

void
gst_saperasrc_finalize (GObject * object)
{
  GstSaperaSrc *src;

  g_return_if_fail (GST_IS_SAPERA_SRC (object));
  src = GST_SAPERA_SRC (object);

  /* clean up object here */
  g_free (src->format_file);

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  if (src->buffer) {
    gst_buffer_unref (src->buffer);
    src->buffer = NULL;
  }

  G_OBJECT_CLASS (gst_saperasrc_parent_class)->finalize (object);
}

static gboolean
gst_saperasrc_start (GstBaseSrc * bsrc)
{
  GstSaperaSrc *src = GST_SAPERA_SRC (bsrc);
  GstVideoInfo vinfo;
  SapFormat sap_format;
  GstVideoFormat gst_format;

  GST_DEBUG_OBJECT (src, "start");

  if (!strlen (src->format_file)) {
    GST_ERROR_OBJECT (src, "Format file must be specified");
    return FALSE;
  }

  if (!g_file_test (src->format_file, G_FILE_TEST_EXISTS)) {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        ("Format file does not exist: %s", src->format_file), (NULL));
    return FALSE;
  }

  GST_DEBUG_OBJECT (src, "About to initialize and create Sapera objects");
  if (!gst_saperasrc_init_objects (src) || !gst_saperasrc_create_objects (src)) {
    GST_ERROR_OBJECT (src, "Failed to create Sapera objects");
    return FALSE;
  }

  GST_DEBUG_OBJECT (src, "Creating caps from Sapera buffer format");
  sap_format = src->sap_buffers->GetFormat ();
  switch (sap_format) {
    case SapFormatMono8:
      gst_format = GST_VIDEO_FORMAT_GRAY8;
      break;
    case SapFormatMono16:
      gst_format = GST_VIDEO_FORMAT_GRAY16_LE;
      break;
    case SapFormatRGB888:
      gst_format = GST_VIDEO_FORMAT_BGR;
      break;
    case SapFormatRGB8888:
      gst_format = GST_VIDEO_FORMAT_BGRA;
      break;
    default:
      gst_format = GST_VIDEO_FORMAT_UNKNOWN;
  }

  if (gst_format == GST_VIDEO_FORMAT_UNKNOWN) {
    char format_name[17];
    SapManager::GetStringFromFormat (sap_format, format_name);
    GST_ERROR_OBJECT (src, "Unsupported format: %s", format_name);

  }

  gst_video_info_init (&vinfo);
  vinfo.width = src->sap_buffers->GetWidth ();
  vinfo.height = src->sap_buffers->GetHeight ();
  vinfo.finfo = gst_video_format_get_info (gst_format);
  src->caps = gst_video_info_to_caps (&vinfo);

  src->height = vinfo.height;
  src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);

  if (!src->sap_xfer->Grab ()) {
    GST_ERROR_OBJECT (src, "Failed to start grab");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_saperasrc_stop (GstBaseSrc * bsrc)
{
  GstSaperaSrc *src = GST_SAPERA_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "stop");

  if (!src->sap_xfer->Freeze ()) {
    GST_ERROR_OBJECT (src, "Failed to stop camera acquisition");
    return FALSE;
  }

  if (!src->sap_xfer->Wait (250)) {
    GST_ERROR_OBJECT (src, "Acquisition failed to stop camera, aborting");
    src->sap_xfer->Abort ();
    return FALSE;
  }

  gst_saperasrc_reset (src);

  return TRUE;
}

static GstCaps *
gst_saperasrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstSaperaSrc *src = GST_SAPERA_SRC (bsrc);
  GstCaps *caps;

  if (src->sap_acq && *src->sap_acq) {
    caps = gst_caps_copy (src->caps);
  } else {
    caps = gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD (src));
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
gst_saperasrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstSaperaSrc *src = GST_SAPERA_SRC (bsrc);
  GstVideoInfo vinfo;
  GstStructure *s = gst_caps_get_structure (caps, 0);

  GST_DEBUG_OBJECT (src, "The caps being set are %" GST_PTR_FORMAT, caps);

  gst_video_info_from_caps (&vinfo, caps);

  if (GST_VIDEO_INFO_FORMAT (&vinfo) != GST_VIDEO_FORMAT_UNKNOWN) {
    src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);
  } else {
    goto unsupported_caps;
  }

  return TRUE;

unsupported_caps:
  GST_ERROR_OBJECT (src, "Unsupported caps: %" GST_PTR_FORMAT, caps);
  return FALSE;
}

static GstFlowReturn
gst_saperasrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstSaperaSrc *src = GST_SAPERA_SRC (psrc);

  g_mutex_lock (&src->buffer_mutex);
  while (src->buffer == NULL)
    g_cond_wait (&src->buffer_cond, &src->buffer_mutex);
  *buf = src->buffer;
  src->buffer = NULL;
  g_mutex_unlock (&src->buffer_mutex);

  GST_DEBUG ("saperasrc_create => pts %" GST_TIME_FORMAT " duration %"
      GST_TIME_FORMAT, GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (*buf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (*buf)));

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_saperasrc_debug, "saperasrc", 0,
      "debug category for saperasrc element");
  gst_element_register (plugin, "saperasrc", GST_RANK_NONE,
      gst_saperasrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    sapera,
    "Teledyne DALSA Sapera frame grabber source",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
