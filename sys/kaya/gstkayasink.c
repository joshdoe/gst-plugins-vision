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
 * SECTION:element-gstkayasink
 *
 * The kayasink element is a sink for Kaya Chameleon CoaXPress devices.
 *
 * Note that kayasink requires a steady framerate to match the framerate in
 * the caps, so you might need to add a videorate element before the sink.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc ! videoconvert ! videorate ! kayasink
 * ]|
 * Outputs test pattern using Kaya Chameleon.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include <KYFGLib.h>

#include "gstkayasink.h"


/* GObject prototypes */
static void gst_kayasink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_kayasink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_kayasink_dispose (GObject * object);

/* GstBaseSink prototypes */
static gboolean gst_kayasink_start (GstBaseSink * basesink);
static gboolean gst_kayasink_stop (GstBaseSink * basesink);
static GstCaps *gst_kayasink_get_caps (GstBaseSink * basesink,
    GstCaps * filter_caps);
static gboolean gst_kayasink_set_caps (GstBaseSink * basesink, GstCaps * caps);
static GstFlowReturn gst_kayasink_render (GstBaseSink * basesink,
    GstBuffer * buffer);
static gboolean gst_kayasink_unlock (GstBaseSink * basesink);
static gboolean gst_kayasink_unlock_stop (GstBaseSink * basesink);

static void gst_kayasink_camera_callback (GstKayaSink * sink,
    STREAM_HANDLE streamHandle);
static void gst_kayasink_device_event_callback (GstKayaSink * sink,
    KYDEVICE_EVENT * pEvent);
static gboolean gst_kayasink_set_kaya_caps (GstKayaSink * sink, GstCaps * caps);

enum
{
  PROP_0,
  PROP_INTERFACE_INDEX,
  PROP_DEVICE_INDEX,
  PROP_NUM_RENDER_BUFFERS,
  PROP_TIMEOUT,
  PROP_PROJECT_FILE,
  PROP_XML_FILE,
  PROP_WAIT_FOR_RECEIVER,
  PROP_WAIT_TIMEOUT
};

#define DEFAULT_PROP_INTERFACE_INDEX 0
#define DEFAULT_PROP_DEVICE_INDEX 0
#define DEFAULT_PROP_NUM_RENDER_BUFFERS 3
#define DEFAULT_PROP_TIMEOUT 1000
#define DEFAULT_PROP_PROJECT_FILE NULL
#define DEFAULT_PROP_XML_FILE NULL
#define DEFAULT_PROP_WAIT_FOR_RECEIVER TRUE
#define DEFAULT_PROP_WAIT_TIMEOUT 10000

/* pad templates */

static GstStaticPadTemplate gst_kayasink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, RGB, BGR, RGBA, BGRA, IYU1, IYU2, UYVY }"))
    );

/* class initialization */

/* setup debug */
GST_DEBUG_CATEGORY_STATIC (kayasink_debug);
#define GST_CAT_DEFAULT kayasink_debug

G_DEFINE_TYPE (GstKayaSink, gst_kayasink, GST_TYPE_BASE_SINK);

static void
gst_kayasink_class_init (GstKayaSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "kayasink", 0,
      "Kaya Chameleon CoaXPress simulator sink");

  gobject_class->set_property = gst_kayasink_set_property;
  gobject_class->get_property = gst_kayasink_get_property;
  gobject_class->dispose = gst_kayasink_dispose;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_kayasink_sink_template));

  gst_element_class_set_details_simple (gstelement_class,
      "Kaya Chameleon Sink", "Sink/Video",
      "Kaya Chameleon Sink to output CoaXPress video",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_kayasink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_kayasink_stop);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_kayasink_set_caps);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_kayasink_render);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (gst_kayasink_unlock);
  gstbasesink_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_kayasink_unlock_stop);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_INTERFACE_INDEX,
      g_param_spec_int ("interface-index", "Interface index",
          "Interface index number (zero-based)",
          0, KAYA_SINK_MAX_FG_HANDLES, DEFAULT_PROP_INTERFACE_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("device-index", "Device index",
          "Device index number, zero-based",
          0, G_MAXINT, DEFAULT_PROP_DEVICE_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_NUM_RENDER_BUFFERS,
      g_param_spec_int ("num-render-buffers", "Number of render buffers",
          "Number of render buffers", 1, G_MAXINT,
          DEFAULT_PROP_NUM_RENDER_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TIMEOUT,
      g_param_spec_int ("timeout", "Timeout (ms)",
          "Timeout in ms (0 to use default)", 0, G_MAXINT,
          DEFAULT_PROP_TIMEOUT,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (gobject_class, PROP_PROJECT_FILE,
      g_param_spec_string ("project-file", "Project file",
          "Filepath of a project file to configure frame grabber",
          DEFAULT_PROP_PROJECT_FILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_XML_FILE,
      g_param_spec_string ("xml-file", "XML file",
          "Filepath of a XML file to use with camera, or NULL to use camera's native XML",
          DEFAULT_PROP_XML_FILE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_WAIT_FOR_RECEIVER, g_param_spec_boolean ("wait-for-receiver",
          "Wait for receiver",
          "If true, wait until receiver starts stream, else drop buffers.",
          DEFAULT_PROP_WAIT_FOR_RECEIVER,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (gobject_class, PROP_WAIT_TIMEOUT,
      g_param_spec_int ("wait-timeout", "Receiver wait timeout (ms)",
          "Time to wait for receiver to start stream (ms)", 0, G_MAXINT,
          DEFAULT_PROP_WAIT_TIMEOUT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
}

static void
gst_kayasink_cleanup (GstKayaSink * sink)
{
  GST_LOG_OBJECT (sink, "cleanup");

  sink->stop_requested = FALSE;
  sink->acquisition_started = FALSE;

  if (sink->cam_handle != INVALID_CAMHANDLE) {
    KYFG_CameraCallbackUnregister (sink->cam_handle,
        gst_kayasink_camera_callback);
    KYFG_CameraStop (sink->cam_handle);
    KYFG_CameraClose (sink->cam_handle);
    sink->stream_handle = INVALID_STREAMHANDLE;
    sink->cam_handle = INVALID_CAMHANDLE;
  }

  if (sink->fg_data) {
    g_mutex_lock (&sink->fg_data->fg_mutex);
    GST_DEBUG_OBJECT (sink, "Framegrabber open with refcount=%d",
        sink->fg_data->ref_count);
    sink->fg_data->ref_count--;
    if (sink->fg_data->ref_count == 0) {
      GST_DEBUG_OBJECT (sink, "Framegrabber ref dropped to 0, closing");
      KYFG_Close (sink->fg_data->fg_handle);
      sink->fg_data->fg_handle = INVALID_FGHANDLE;
    }
    g_mutex_unlock (&sink->fg_data->fg_mutex);
    sink->fg_data = NULL;
  }
}

static void
gst_kayasink_init (GstKayaSink * sink)
{
  /* properties */
  sink->interface_index = DEFAULT_PROP_INTERFACE_INDEX;
  sink->num_render_buffers = DEFAULT_PROP_NUM_RENDER_BUFFERS;
  sink->timeout = DEFAULT_PROP_TIMEOUT;
  sink->project_file = DEFAULT_PROP_PROJECT_FILE;
  sink->xml_file = DEFAULT_PROP_PROJECT_FILE;
  sink->num_render_buffers = 3;
  sink->wait_for_receiver = TRUE;
  sink->wait_timeout = 10000;

  sink->queue = g_async_queue_new ();

  sink->receiver_connected = FALSE;

  sink->acquisition_started = FALSE;
  sink->stop_requested = FALSE;

  g_mutex_init (&sink->mutex);
  g_cond_init (&sink->cond);
}

void
gst_kayasink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstKayaSink *sink;

  g_return_if_fail (GST_IS_KAYASINK (object));
  sink = GST_KAYASINK (object);

  switch (property_id) {
    case PROP_INTERFACE_INDEX:
      sink->interface_index = g_value_get_int (value);
      break;
    case PROP_DEVICE_INDEX:
      sink->device_index = g_value_get_int (value);
      break;
    case PROP_NUM_RENDER_BUFFERS:
      sink->num_render_buffers = g_value_get_int (value);
      break;
    case PROP_TIMEOUT:
      sink->timeout = g_value_get_int (value);
      break;
    case PROP_PROJECT_FILE:
      g_free (sink->project_file);
      sink->project_file = g_value_dup_string (value);
      break;
    case PROP_XML_FILE:
      g_free (sink->xml_file);
      sink->xml_file = g_value_dup_string (value);
      break;
    case PROP_WAIT_FOR_RECEIVER:
      sink->wait_for_receiver = g_value_get_boolean (value);
      break;
    case PROP_WAIT_TIMEOUT:
      sink->wait_timeout = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_kayasink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstKayaSink *sink;

  g_return_if_fail (GST_IS_KAYASINK (object));
  sink = GST_KAYASINK (object);

  switch (property_id) {
    case PROP_INTERFACE_INDEX:
      g_value_set_int (value, sink->interface_index);
      break;
    case PROP_DEVICE_INDEX:
      g_value_set_int (value, sink->device_index);
      break;
    case PROP_NUM_RENDER_BUFFERS:
      g_value_set_int (value, sink->num_render_buffers);
      break;
    case PROP_TIMEOUT:
      g_value_set_int (value, sink->timeout);
      break;
    case PROP_PROJECT_FILE:
      g_value_set_string (value, sink->project_file);
      break;
    case PROP_XML_FILE:
      g_value_set_string (value, sink->xml_file);
      break;
    case PROP_WAIT_FOR_RECEIVER:
      g_value_set_boolean (value, sink->wait_for_receiver);
      break;
    case PROP_WAIT_TIMEOUT:
      g_value_set_int (value, sink->wait_timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_kayasink_dispose (GObject * object)
{
  GstKayaSink *sink;

  g_return_if_fail (GST_IS_KAYASINK (object));
  sink = GST_KAYASINK (object);

  /* clean up as possible.  may be called multiple times */

  g_mutex_clear (&sink->mutex);
  g_cond_clear (&sink->cond);

  g_free (sink->project_file);
  g_free (sink->xml_file);

  G_OBJECT_CLASS (gst_kayasink_parent_class)->dispose (object);
}

gboolean
gst_kayasink_start (GstBaseSink * basesink)
{
  GstKayaSink *sink = GST_KAYASINK (basesink);
  GstKayaSinkClass *sinkclass = GST_KAYA_SINK_GET_CLASS (sink);
  int ret = 0;
  int i, num_ifaces = 0;
  KY_DEVICE_INFO devinfo;
  GstKayaSinkFramegrabber *fg_data;

  GST_DEBUG_OBJECT (sink, "Starting");

  /* find and list all KAYA interfaces */
  ret = KY_DeviceScan (&num_ifaces);
  if (num_ifaces == 0) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED, ("No KAYA interfaces found"),
        (NULL));
    goto error;
  }
  for (i = 0; i < num_ifaces; ++i) {
    ret = KY_DeviceInfo (i, &devinfo);
    GST_DEBUG_OBJECT (sink,
        "Found KAYA interface '%s', index=%d, bus=%d, slot=%d, fcn=%d, PID=%d, isVirtual=%s",
        devinfo.szDeviceDisplayName, i, devinfo.nBus, devinfo.nSlot,
        devinfo.nFunction, devinfo.DevicePID,
        (devinfo.isVirtual ? "Yes" : "No"));
  }

  g_assert (sink->interface_index >= 0
      && sink->interface_index < KAYA_SINK_MAX_FG_HANDLES);
  if (sink->interface_index >= num_ifaces) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Interface index provided (%d) out of bounds [0,%d]",
            sink->interface_index, num_ifaces - 1), (NULL));
    goto error;
  }

  /* lock mutex until we have a camera opened */
  fg_data = &(sinkclass->fg_data[sink->interface_index]);
  g_mutex_lock (&fg_data->fg_mutex);

  /* open framegrabber if it isn't already opened */
  if (fg_data->ref_count > 0) {
    GST_DEBUG_OBJECT (sink,
        "Framegrabber interface already opened in this process, reusing");
    if (sink->project_file && strlen (sink->project_file) > 0) {
      GST_ELEMENT_WARNING (sink, RESOURCE, SETTINGS,
          ("Project file specified, but framegrabber is already opened, so it won't be used."),
          (NULL));
    }
  } else {
    g_assert (fg_data->ref_count == 0);

    /* project files are optional */
    if (sink->project_file && strlen (sink->project_file) > 0) {
      if (!g_file_test (sink->project_file, G_FILE_TEST_EXISTS)) {
        g_mutex_unlock (&fg_data->fg_mutex);
        GST_ELEMENT_ERROR (sink, RESOURCE, NOT_FOUND,
            ("Project file specified does not exist: %s", sink->project_file),
            (NULL));
        goto error;
      }

      GST_DEBUG_OBJECT (sink,
          "About to open interface at index %d with project file '%s'",
          sink->interface_index, sink->project_file);
      fg_data->fg_handle =
          KYFG_OpenEx (sink->interface_index, sink->project_file);
    } else {
      GST_DEBUG_OBJECT (sink, "About to open interface at index %d",
          sink->interface_index);
      fg_data->fg_handle = KYFG_Open (sink->interface_index);
    }

    if (fg_data->fg_handle == INVALID_FGHANDLE) {
      g_mutex_unlock (&fg_data->fg_mutex);
      GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
          ("Failed to open interface at index %d", sink->interface_index),
          (NULL));
      goto error;
    }

    if (fg_data->num_cams == 0) {
      GST_DEBUG_OBJECT (sink, "Scanning for cameras");
      /* find and list all cameras */
      fg_data->num_cams = KAYA_SINK_MAX_CAM_HANDLES;
      ret =
          KYFG_UpdateCameraList (fg_data->fg_handle, fg_data->cam_handles,
          &fg_data->num_cams);
      GST_DEBUG_OBJECT (sink, "Found %d cameras connected", fg_data->num_cams);
      if (fg_data->num_cams == 0) {
        g_mutex_unlock (&fg_data->fg_mutex);
        GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
            ("Failed to detect any cameras on interface"), (NULL));
        goto error;
      }
    }
    for (i = 0; i < fg_data->num_cams; ++i) {
      KYFGCAMERA_INFO caminfo;
      ret = KYFG_CameraInfo (fg_data->cam_handles[i], &caminfo);
      GST_DEBUG_OBJECT (sink,
          "Found camera '%s', index=%d, %s, %s %s, %s, ver=%s",
          caminfo.deviceUserID, i, caminfo.deviceID, caminfo.deviceVendorName,
          caminfo.deviceModelName, caminfo.deviceManufacturerInfo,
          caminfo.deviceVersion);
    }
  }

  if (sink->device_index >= fg_data->num_cams) {
    g_mutex_unlock (&fg_data->fg_mutex);
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Camera device index provided out of bounds"), (NULL));
    goto error;
  }
  GST_DEBUG_OBJECT (sink, "About to open camera at index %d",
      sink->device_index);
  if (sink->xml_file && strlen (sink->xml_file) > 0) {
    if (!g_file_test (sink->xml_file, G_FILE_TEST_EXISTS)) {
      g_mutex_unlock (&fg_data->fg_mutex);
      GST_ELEMENT_ERROR (sink, RESOURCE, NOT_FOUND,
          ("XML file specified does not exist: %s", sink->xml_file), (NULL));
      goto error;
    }
  }
  ret =
      KYFG_CameraOpen2 (fg_data->cam_handles[sink->device_index],
      sink->xml_file);
  if (ret != FGSTATUS_OK) {
    g_mutex_unlock (&fg_data->fg_mutex);
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Failed to open camera at index %d", sink->device_index), (NULL));
    goto error;
  }
  sink->cam_handle = fg_data->cam_handles[sink->device_index];

  /* increase refcount since we now have a camera open */
  sink->fg_data = fg_data;
  sink->fg_data->ref_count++;
  g_mutex_unlock (&sink->fg_data->fg_mutex);

  ret =
      KYFG_CameraCallbackRegister (sink->cam_handle,
      gst_kayasink_camera_callback, sink);
  if (ret != FGSTATUS_OK) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Failed to register stream callback function"), (NULL));
    goto error;
  }

  ret =
      KYDeviceEventCallBackRegister (sink->fg_data->fg_handle,
      gst_kayasink_device_event_callback, sink);
  if (ret != FGSTATUS_OK) {
    GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
        ("Failed to register device callback function"), (NULL));
    goto error;
  }

  sink->acquisition_started = FALSE;
  sink->stop_requested = FALSE;

  return TRUE;

error:
  return FALSE;
}

gboolean
gst_kayasink_stop (GstBaseSink * basesink)
{
  GstKayaSink *sink = GST_KAYASINK (basesink);

  gst_kayasink_cleanup (sink);

  return TRUE;
}

gboolean
gst_kayasink_set_caps (GstBaseSink * basesink, GstCaps * caps)
{
  GstKayaSink *sink = GST_KAYASINK (basesink);

  GST_DEBUG_OBJECT (sink, "Caps being set");

  if (!gst_kayasink_set_kaya_caps (sink, caps))
    return FALSE;

  return TRUE;
}

gboolean
gst_kayasink_set_kaya_caps (GstKayaSink * sink, GstCaps * caps)
{
  FGSTATUS ret;
  const char *format;
  float fps;

  g_assert (caps);
  g_assert (sink->cam_handle != INVALID_CAMHANDLE);

  gst_video_info_from_caps (&sink->vinfo, caps);
  fps = (float) sink->vinfo.fps_n / sink->vinfo.fps_d;

  GST_DEBUG_OBJECT (sink, "Setting video output to %dx%d@%.2fHz",
      sink->vinfo.width, sink->vinfo.height, fps);

  // set from caps:
  ret = KYFG_SetCameraValueInt (sink->cam_handle, "Width", sink->vinfo.width);
  if (ret != FGSTATUS_OK)
    goto error;
  ret = KYFG_SetCameraValueInt (sink->cam_handle, "Height", sink->vinfo.height);
  if (ret != FGSTATUS_OK)
    goto error;
  ret =
      KYFG_SetCameraValueFloat (sink->cam_handle, "AcquisitionFrameRate", fps);
  if (ret != FGSTATUS_OK)
    goto error;

  switch (GST_VIDEO_INFO_FORMAT (&sink->vinfo)) {
    case GST_VIDEO_FORMAT_GRAY8:
      format = "Mono8";
      break;
    case GST_VIDEO_FORMAT_GRAY16_LE:
      format = "Mono16";
      break;
    case GST_VIDEO_FORMAT_RGB:
      format = "RGB8";
      break;
    case GST_VIDEO_FORMAT_RGBA:
      format = "RGBa8";
      break;
    case GST_VIDEO_FORMAT_BGR:
      format = "BGR8";
      break;
    case GST_VIDEO_FORMAT_BGRA:
      format = "BGRa8";
      break;
    case GST_VIDEO_FORMAT_IYU1:
      format = "YUV411_8_UYYVYY";
      break;
    case GST_VIDEO_FORMAT_IYU2:
      format = "YUV8_UYV";
      break;
    case GST_VIDEO_FORMAT_UYVY:
      format = "YUV422_8_UYVY";
      break;
    default:
      goto error;
  }

  ret =
      KYFG_SetCameraValueEnum_ByValueName (sink->cam_handle, "PixelFormat",
      format);
  if (ret != FGSTATUS_OK)
    goto error;

  return TRUE;

error:
  GST_ELEMENT_ERROR (sink, LIBRARY, FAILED,
      ("Failed to set width, height, FPS, or pixel format."), (NULL));
  return FALSE;
}

GstFlowReturn
gst_kayasink_render (GstBaseSink * basesink, GstBuffer * buffer)
{
  GstKayaSink *sink = GST_KAYASINK (basesink);
  GST_LOG_OBJECT (sink, "Rendering buffer");

  if (!sink->receiver_connected) {
    if (sink->wait_for_receiver) {
      gint64 end_time;
      /* wait for receiver/framegrabber to start */
      end_time =
          g_get_monotonic_time () +
          sink->wait_timeout * G_TIME_SPAN_MILLISECOND;
      g_mutex_lock (&sink->mutex);
      while (!sink->receiver_connected && !sink->stop_requested) {
        GST_DEBUG_OBJECT (sink,
            "Neither stream started nor stop requested, waiting %dms",
            sink->wait_timeout);
        if (!g_cond_wait_until (&sink->cond, &sink->mutex, end_time)) {
          g_mutex_unlock (&sink->mutex);
          GST_ELEMENT_WARNING (sink, RESOURCE, FAILED,
              ("Timed out waiting for receiver to start stream."), (NULL));
          return GST_FLOW_ERROR;
        }
      }
      g_mutex_unlock (&sink->mutex);
    } else {
      GST_WARNING_OBJECT (sink, "No receiver connected, dropping buffer");
      /* TODO: use flag to either queue or drop frames */
      return GST_FLOW_OK;
    }
  }

  if (sink->stop_requested) {
    GST_DEBUG_OBJECT (sink, "stop requested, flushing");
    return GST_FLOW_FLUSHING;
  }
  ///* wait for client to send AcquisitionStart command */
  //g_mutex_lock (&sink->mutex);
  //end_time = g_get_monotonic_time () + sink->timeout * G_TIME_SPAN_MILLISECOND;
  //while (!sink->acquisition_started && !sink->stop_requested) {
  //  GST_DEBUG_OBJECT (sink, "acq not started and stop not requested, waiting");
  //  if (!g_cond_wait_until (&sink->cond, &sink->mutex, end_time)) {
  //    g_mutex_unlock (&sink->mutex);
  //    GST_ELEMENT_WARNING (sink, RESOURCE, FAILED,
  //        ("Timed out waiting for client to send AcquisitionStart, dropping frame"),
  //        (NULL));
  //    return GST_FLOW_OK;
  //  }
  //}
  //if (sink->stop_requested) {
  //  GST_DEBUG_OBJECT (sink, "stop requested, flushing");
  //  g_mutex_unlock (&sink->mutex);
  //  return GST_FLOW_FLUSHING;
  //}
  //g_mutex_unlock (&sink->mutex);

  g_async_queue_push (sink->queue, gst_buffer_ref (buffer));

  return GST_FLOW_OK;
}

gboolean
gst_kayasink_unlock (GstBaseSink * basesink)
{
  GstKayaSink *sink = GST_KAYASINK (basesink);

  g_mutex_lock (&sink->mutex);
  sink->stop_requested = TRUE;
  g_cond_signal (&sink->cond);
  g_mutex_unlock (&sink->mutex);

  return TRUE;
}

gboolean
gst_kayasink_unlock_stop (GstBaseSink * basesink)
{
  GstKayaSink *sink = GST_KAYASINK (basesink);

  sink->stop_requested = FALSE;

  return TRUE;
}

void
gst_kayasink_device_event_callback (GstKayaSink * sink, KYDEVICE_EVENT * pEvent)
{
  FGSTATUS ret;

  switch (pEvent->eventId) {
    case KYDEVICE_EVENT_CAMERA_START_REQUEST:
    {
      CAMHANDLE eventCameraHandle =
          ((KYDEVICE_EVENT_CAMERA_START *) pEvent)->camHandle;
      if (eventCameraHandle == sink->cam_handle) {
        GST_DEBUG_OBJECT (sink, "Detected remote request to start generation");
        //StartGeneration();
        ret =
            KYFG_StreamCreateAndAlloc (sink->cam_handle, &sink->stream_handle,
            sink->num_render_buffers, 0);
        if (ret != FGSTATUS_OK) {
          GST_ELEMENT_WARNING (sink, RESOURCE, FAILED,
              ("Failed to create stream"), (NULL));
          return;
        }

        GST_DEBUG_OBJECT (sink, "Starting camera output");
        ret = KYFG_CameraStart (sink->cam_handle, sink->stream_handle, 0);

        g_mutex_lock (&sink->mutex);
        sink->receiver_connected = TRUE;
        g_cond_signal (&sink->cond);
        g_mutex_unlock (&sink->mutex);
      } else {
        GST_WARNING_OBJECT (sink,
            "Got start request for invalid camera handle");
      }
    }
      break;
  }
}

void
gst_kayasink_camera_callback (GstKayaSink * sink, STREAM_HANDLE streamHandle)
{
  uint32_t currentIndex;        // Indicates the Nth frame that was currently send.
  void *ptr;                    // Pointer to current frame the data
  int64_t size;                 // Size of each frame in bytes
  GstBuffer *buf;
  GstMapInfo minfo;

  if (!streamHandle) {
    // callback with streamHandle == 0 indicates that stream generation has stopped
    // any data retrieved using this handle (frame index, buffer pointer, etc.) won't be valid
    GST_DEBUG_OBJECT (sink, "Stream generation stopped");

    g_mutex_lock (&sink->mutex);
    sink->receiver_connected = FALSE;
    g_cond_signal (&sink->cond);
    g_mutex_unlock (&sink->mutex);

    return;
  }

  currentIndex = KYFG_StreamGetFrameIndex (streamHandle);

  ptr = KYFG_StreamGetPtr (streamHandle, currentIndex);
  size = KYFG_StreamGetSize (streamHandle);

  GST_DEBUG_OBJECT (sink, "Copying new frame to index %d", currentIndex);

  //g_mutex_lock(&sink->mutex);

  //while (g_async_queue_length(sink->queue) == 0 && !sink->flushing) {
  //    g_cond_wait(&sink->cond, &sink->mutex);
  //}
  //if (sink->flushing) {
  //    return;
  //}
  buf =
      GST_BUFFER (g_async_queue_timeout_pop (sink->queue,
          sink->timeout * 1000));
  if (!buf) {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_READ,
        ("Failed to get buffer in %d ms", sink->timeout), (NULL));
    return;
  }

  gst_buffer_map (buf, &minfo, GST_MAP_READ);
  /* TODO: fix stride? */
  g_assert (minfo.size <= (gsize) size);
  memcpy (ptr, minfo.data, minfo.size);
  gst_buffer_unmap (buf, &minfo);
  gst_buffer_unref (buf);
}
