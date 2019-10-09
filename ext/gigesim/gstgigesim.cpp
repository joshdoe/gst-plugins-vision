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
 * SECTION:element-gstgigesimsink
 *
 * The gigesimsink element is a sink for A&B Soft GigESim to output a GigE Vision video stream.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc ! autovideoconvert ! gigesimsink
 * ]|
 * Outputs test pattern using GigESim.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstgigesim.h"

/* GObject prototypes */
static void gst_gigesimsink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_gigesimsink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_gigesimsink_dispose (GObject * object);

/* GstBaseSink prototypes */
static gboolean gst_gigesimsink_start (GstBaseSink * basesink);
static gboolean gst_gigesimsink_stop (GstBaseSink * basesink);
static GstCaps *gst_gigesimsink_get_caps (GstBaseSink * basesink,
    GstCaps * filter_caps);
static gboolean gst_gigesimsink_set_caps (GstBaseSink * basesink,
    GstCaps * caps);
static GstFlowReturn gst_gigesimsink_render (GstBaseSink * basesink,
    GstBuffer * buffer);
static gboolean gst_gigesimsink_unlock (GstBaseSink * basesink);
static gboolean gst_gigesimsink_unlock_stop (GstBaseSink * basesink);

enum
{
  PROP_0,
  PROP_TIMEOUT,
  PROP_ADDRESS,
  PROP_MANUFACTURER,
  PROP_MODEL,
  PROP_VERSION,
  PROP_INFO,
  PROP_SERIAL,
  PROP_MAC
};

#define DEFAULT_PROP_TIMEOUT 10000
#define DEFAULT_PROP_ADDRESS      ""
#define DEFAULT_PROP_MANUFACTURER "A&B Software"
#define DEFAULT_PROP_MODEL        "GigESim GStreamer"
#define DEFAULT_PROP_VERSION      "0.1"
#define DEFAULT_PROP_INFO         "A&B Soft GigESim GStreamer Sink"
#define DEFAULT_PROP_SERIAL       "0001"
#define DEFAULT_PROP_MAC          "AA-00-00-00-00-00"

/* pad templates */

static GstStaticPadTemplate gst_gigesimsink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, RGB, BGR, RGBA, BGRA, IYU1, IYU2, UYVY }"))
    );

/* class initialization */

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (gigesimsink_debug);
#define GST_CAT_DEFAULT gigesimsink_debug

G_DEFINE_TYPE (GstGigesimSink, gst_gigesimsink, GST_TYPE_BASE_SINK);

static void
gst_gigesimsink_class_init (GstGigesimSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "gigesimsink", 0,
      "A&B Soft GigESim Camera Link simulator sink");

  gobject_class->set_property = gst_gigesimsink_set_property;
  gobject_class->get_property = gst_gigesimsink_get_property;
  gobject_class->dispose = gst_gigesimsink_dispose;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_gigesimsink_sink_template));

  gst_element_class_set_details_simple (gstelement_class,
      "A&B Soft GigESim Sink", "Sink/Video",
      "A&B Soft GigESim Sink to output GigE Vision video",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_gigesimsink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_gigesimsink_stop);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_gigesimsink_set_caps);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_gigesimsink_render);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (gst_gigesimsink_unlock);
  gstbasesink_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_gigesimsink_unlock_stop);

  g_object_class_install_property (gobject_class, PROP_ADDRESS,
      g_param_spec_string ("address", "IP address",
          "The IP address of the network interface to bind to (default is first found)",
          DEFAULT_PROP_ADDRESS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  g_object_class_install_property (gobject_class, PROP_MANUFACTURER,
      g_param_spec_string ("manufacturer", "Manufacturer",
          "Manufacturer of the virtual camera",
          DEFAULT_PROP_MANUFACTURER,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_MODEL,
      g_param_spec_string ("model", "Model",
          "Model of the virtual camera",
          DEFAULT_PROP_MODEL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_VERSION,
      g_param_spec_string ("version", "Version",
          "Version of the virtual camera",
          DEFAULT_PROP_VERSION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_INFO,
      g_param_spec_string ("info", "Info",
          "Info of the virtual camera",
          DEFAULT_PROP_INFO,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_SERIAL,
      g_param_spec_string ("serial", "Serial",
          "Serial of the virtual camera",
          DEFAULT_PROP_SERIAL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_MAC,
      g_param_spec_string ("mac", "MAC address",
          "MAC address of the virtual camera",
          DEFAULT_PROP_MAC,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
}

bool
gst_gigesimsink_on_feature_write (void *context, const char *feature)
{
  GstGigesimSink *sink = GST_GIGESIMSINK (context);
  GST_LOG_OBJECT (sink, "Feature written '%s'", feature);
  if (g_strcmp0 (feature, "AcquisitionStart") == 0) {
    g_mutex_lock (&sink->mutex);
    sink->acquisition_started = TRUE;
    g_cond_signal (&sink->cond);
    g_mutex_unlock (&sink->mutex);
  } else if (g_strcmp0 (feature, "AcquisitionStop") == 0) {
    g_mutex_lock (&sink->mutex);
    sink->acquisition_started = FALSE;
    g_cond_signal (&sink->cond);
    g_mutex_unlock (&sink->mutex);
  }
  return true;
}

static void
gst_gigesimsink_init (GstGigesimSink * sink)
{
  /* properties */
  sink->timeout = DEFAULT_PROP_TIMEOUT;
  sink->address = g_strdup (DEFAULT_PROP_ADDRESS);
  sink->manufacturer = g_strdup (DEFAULT_PROP_MANUFACTURER);
  sink->model = g_strdup (DEFAULT_PROP_MODEL);
  sink->version = g_strdup (DEFAULT_PROP_VERSION);
  sink->info = g_strdup (DEFAULT_PROP_INFO);
  sink->serial = g_strdup (DEFAULT_PROP_SERIAL);
  sink->mac = g_strdup (DEFAULT_PROP_MAC);

  sink->camera_connected = FALSE;

  sink->acquisition_started = FALSE;
  sink->stop_requested = FALSE;

  sink->pCamera = createCamera ();
  sink->pCamera->SetWriteCallback (sink, gst_gigesimsink_on_feature_write);

  g_mutex_init (&sink->mutex);
  g_cond_init (&sink->cond);
}

void
gst_gigesimsink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGigesimSink *sink;

  g_return_if_fail (GST_IS_GIGESIMSINK (object));
  sink = GST_GIGESIMSINK (object);

  switch (property_id) {
    case PROP_TIMEOUT:
      sink->timeout = g_value_get_int (value);
      break;
    case PROP_ADDRESS:
      g_free (sink->address);
      sink->address = g_strdup (g_value_get_string (value));
      break;
    case PROP_MANUFACTURER:
      g_free (sink->manufacturer);
      sink->manufacturer = g_strdup (g_value_get_string (value));
      break;
    case PROP_MODEL:
      g_free (sink->model);
      sink->model = g_strdup (g_value_get_string (value));
      break;
    case PROP_VERSION:
      g_free (sink->version);
      sink->version = g_strdup (g_value_get_string (value));
      break;
    case PROP_INFO:
      g_free (sink->info);
      sink->info = g_strdup (g_value_get_string (value));
      break;
    case PROP_SERIAL:
      g_free (sink->serial);
      sink->serial = g_strdup (g_value_get_string (value));
      break;
    case PROP_MAC:
      g_free (sink->mac);
      sink->mac = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_gigesimsink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstGigesimSink *sink;

  g_return_if_fail (GST_IS_GIGESIMSINK (object));
  sink = GST_GIGESIMSINK (object);

  switch (property_id) {
    case PROP_TIMEOUT:
      g_value_set_int (value, sink->timeout);
      break;
    case PROP_ADDRESS:
      g_value_set_string (value, sink->address);
      break;
    case PROP_MANUFACTURER:
      g_value_set_string (value, sink->manufacturer);
      break;
    case PROP_MODEL:
      g_value_set_string (value, sink->model);
      break;
    case PROP_VERSION:
      g_value_set_string (value, sink->version);
      break;
    case PROP_INFO:
      g_value_set_string (value, sink->info);
      break;
    case PROP_SERIAL:
      g_value_set_string (value, sink->serial);
      break;
    case PROP_MAC:
      g_value_set_string (value, sink->mac);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_gigesimsink_dispose (GObject * object)
{
  GstGigesimSink *sink;

  g_return_if_fail (GST_IS_GIGESIMSINK (object));
  sink = GST_GIGESIMSINK (object);

  /* clean up as possible.  may be called multiple times */
  if (sink->pCamera) {
    delete sink->pCamera;
    sink->pCamera = NULL;
  }

  g_mutex_clear (&sink->mutex);
  g_cond_clear (&sink->cond);

  g_free (sink->address);

  G_OBJECT_CLASS (gst_gigesimsink_parent_class)->dispose (object);
}

gboolean
gst_gigesimsink_start (GstBaseSink * basesink)
{
  GstGigesimSink *sink = GST_GIGESIMSINK (basesink);
  int ret = 0;

  GST_DEBUG_OBJECT (sink, "Starting");

  sink->acquisition_started = FALSE;
  sink->stop_requested = FALSE;

  return TRUE;
}

gboolean
gst_gigesimsink_stop (GstBaseSink * basesink)
{
  GstGigesimSink *sink = GST_GIGESIMSINK (basesink);

  if (sink->pCamera)
    sink->pCamera->Disconnect ();

  sink->camera_connected = FALSE;

  sink->acquisition_started = FALSE;
  sink->stop_requested = FALSE;

  return TRUE;
}

gboolean
gst_gigesimsink_set_caps (GstBaseSink * basesink, GstCaps * caps)
{
  GstGigesimSink *sink = GST_GIGESIMSINK (basesink);

  GST_DEBUG_OBJECT (sink, "Caps being set");

  gst_video_info_from_caps (&sink->vinfo, caps);

  return TRUE;
}

GstFlowReturn
gst_gigesimsink_render (GstBaseSink * basesink, GstBuffer * buffer)
{
  GstGigesimSink *sink = GST_GIGESIMSINK (basesink);
  GstMapInfo minfo;
  GST_LOG_OBJECT (sink, "Rendering buffer");
  gint64 end_time;
  int ret;

  /* configure and connect virtual camera */
  if (!sink->camera_connected) {
    GST_DEBUG_OBJECT (sink, "Setting video output to %dx%d", sink->vinfo.width,
        sink->vinfo.height);
    sink->pCamera->SetImageSize (sink->vinfo.width, sink->vinfo.height);

    switch (GST_VIDEO_INFO_FORMAT (&sink->vinfo)) {
      case GST_VIDEO_FORMAT_GRAY8:
        sink->pCamera->AddPixelFormat ("Mono8");
        sink->pCamera->SetPixelFormat ("Mono8");
        break;
      case GST_VIDEO_FORMAT_GRAY16_LE:
        sink->pCamera->AddPixelFormat ("Mono16");
        sink->pCamera->SetPixelFormat ("Mono16");
        break;
      case GST_VIDEO_FORMAT_RGB:
        sink->pCamera->AddPixelFormat ("RGB8");
        sink->pCamera->SetPixelFormat ("RGB8");
        break;
      case GST_VIDEO_FORMAT_RGBA:
        sink->pCamera->AddPixelFormat ("RGBa8");
        sink->pCamera->SetPixelFormat ("RGBa8");
        break;
      case GST_VIDEO_FORMAT_BGR:
        sink->pCamera->AddPixelFormat ("BGR8");
        sink->pCamera->SetPixelFormat ("BGR8");
        break;
      case GST_VIDEO_FORMAT_BGRA:
        sink->pCamera->AddPixelFormat ("BGRa8");
        sink->pCamera->SetPixelFormat ("BGRa8");
        break;
      case GST_VIDEO_FORMAT_IYU1:
        sink->pCamera->AddPixelFormat ("YUV411_8_UYYVYY");
        sink->pCamera->SetPixelFormat ("YUV411_8_UYYVYY");
        break;
      case GST_VIDEO_FORMAT_IYU2:
        sink->pCamera->AddPixelFormat ("YUV8_UYV");
        sink->pCamera->SetPixelFormat ("YUV8_UYV");
        break;
      case GST_VIDEO_FORMAT_UYVY:
        sink->pCamera->AddPixelFormat ("YUV422_8_UYVY");
        sink->pCamera->SetPixelFormat ("YUV422_8_UYVY");
        break;
      default:
        g_assert_not_reached ();
    }

    ret =
        sink->pCamera->SetDeviceInfo (sink->manufacturer, sink->model,
        sink->version, sink->info, sink->serial);
    if (ret) {
      GST_ELEMENT_WARNING (sink, RESOURCE, FAILED,
          ("Failed to set device info"), (NULL));
      return GST_FLOW_ERROR;
    }

    ret = sink->pCamera->SetMacAddress (sink->mac);
    if (ret) {
      GST_ELEMENT_WARNING (sink, RESOURCE, FAILED, ("Failed to MAC address"),
          (NULL));
      return GST_FLOW_ERROR;
    }

    ret = sink->pCamera->SetTimerMode (CGevCamera::TIMER_MODE::TIMER_UNIX);
    if (ret) {
      GST_ELEMENT_WARNING (sink, RESOURCE, FAILED, ("Failed to timer mode"),
          (NULL));
      return GST_FLOW_ERROR;
    }

    if (g_strcmp0 (sink->address, "") == 0) {
      char ipstr[16];
      GST_DEBUG_OBJECT (sink, "No address specified, using first found");
      ret = sink->pCamera->GetInterfaceAtIndex (0, ipstr);
      if (ret) {
        GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
            ("No network interfaces available"), (NULL));
        return GST_FLOW_ERROR;
      }
      g_free (sink->address);
      sink->address = g_strdup (ipstr);
    }

    GST_DEBUG_OBJECT (sink, "Connecting to IP address %s", sink->address);
    ret = sink->pCamera->Connect (sink->address);

    if (ret) {
      GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
          ("Unable to bind virtual camera to network interface"), (NULL));
      return GST_FLOW_ERROR;
    }

    sink->camera_connected = TRUE;
  }

  if (sink->stop_requested) {
    GST_DEBUG_OBJECT (sink, "stop requested, flushing");
    return GST_FLOW_FLUSHING;
  }

  /* wait for client to send AcquisitionStart command */
  g_mutex_lock (&sink->mutex);
  end_time = g_get_monotonic_time () + sink->timeout * G_TIME_SPAN_MILLISECOND;
  while (!sink->acquisition_started && !sink->stop_requested) {
    GST_DEBUG_OBJECT (sink, "acq not started and stop not requested, waiting");
    if (!g_cond_wait_until (&sink->cond, &sink->mutex, end_time)) {
      g_mutex_unlock (&sink->mutex);
      GST_ELEMENT_WARNING (sink, RESOURCE, FAILED,
          ("Timed out waiting for client to send AcquisitionStart, dropping frame"),
          (NULL));
      return GST_FLOW_OK;
    }
  }
  if (sink->stop_requested) {
    GST_DEBUG_OBJECT (sink, "stop requested, flushing");
    g_mutex_unlock (&sink->mutex);
    return GST_FLOW_FLUSHING;
  }
  g_mutex_unlock (&sink->mutex);

  /* we should already have AcquisitionStart, so this shouldn't block */
  sink->pCamera->LockFormat ();

  gst_buffer_map (buffer, &minfo, GST_MAP_READ);
  /* TODO: fix stride? */
  ret = sink->pCamera->SendImage ((const char *) minfo.data);
  if (ret) {
    GST_ELEMENT_WARNING (sink, RESOURCE, FAILED, ("Failed to send image"),
        (NULL));
    return GST_FLOW_ERROR;
  }
  gst_buffer_unmap (buffer, &minfo);

  return GST_FLOW_OK;
}

gboolean
gst_gigesimsink_unlock (GstBaseSink * basesink)
{
  GstGigesimSink *sink = GST_GIGESIMSINK (basesink);

  g_mutex_lock (&sink->mutex);
  sink->stop_requested = TRUE;
  g_cond_signal (&sink->cond);
  g_mutex_unlock (&sink->mutex);

  return TRUE;
}

gboolean
gst_gigesimsink_unlock_stop (GstBaseSink * basesink)
{
  GstGigesimSink *sink = GST_GIGESIMSINK (basesink);

  sink->stop_requested = FALSE;

  return TRUE;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "gigesimsink", GST_RANK_NONE,
          GST_TYPE_GIGESIMSINK)) {
    return FALSE;
  }

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gigesim,
    "A&B Soft GigESim elements",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
