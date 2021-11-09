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
 * SECTION:element-gstgentlsrc
 *
 * The gentlsrc element is a source for GenTL producers.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v gentlsrc ! videoconvert ! autovideosink
 * ]|
 * Shows video from the first found GenTL producer.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gmodule.h>

#include <gio/gio.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#include "genicampixelformat.h"
#include "get_unix_ns.h"

#include "unzip.h"

#include "gstgentlsrc.h"

#ifdef HAVE_ORC
#include <orc/orc.h>
#else
#define orc_memcpy memcpy
#endif

// "C:\\BitFlow SDK 6.20\\Bin64\\BFGTL.cti";
// "C:\\Program Files\\Basler\\pylon 5\\Runtime\\x64\\ProducerGEV.cti";

static void
initialize_evt_addresses (GstGenTlProducer * producer)
{
  memset (producer, 0, sizeof (producer));
  producer->cti_path =
      g_strdup ("C:\\Program Files\\EVT\\eSDK\\bin\\EmergentGenTL.cti");
  producer->acquisition_mode_value = 0;
  producer->width = 0xA000;
  producer->height = 0xA004;
  producer->pixel_format = 0xA008;
  producer->payload_size = 0xD008;
  producer->acquisition_mode = 0xB000;
  producer->acquisition_start = 0xB004;
  producer->acquisition_stop = 0xB008;
  producer->tick_frequency_low = 0x0940;
  producer->tick_frequency_high = 0x093C;
  producer->timestamp_control_latch = 0x944;
  producer->timestamp_low = 0x094C;
  producer->timestamp_high = 0x0948;
}

static void
initialize_basler_addresses (GstGenTlProducer * producer)
{
  memset (producer, 0, sizeof (producer));
  producer->cti_path =
      g_strdup
      ("C:\\Program Files\\Basler\\pylon 5\\Runtime\\x64\\ProducerGEV.cti");
  producer->acquisition_mode_value = 2;
  producer->width = 0x30204;
  producer->height = 0x30224;
  producer->pixel_format = 0x30024;
  producer->payload_size = 0x10088;
  producer->acquisition_mode = 0x40004;
  producer->acquisition_start = 0x40024;
  producer->acquisition_stop = 0x40044;
}

#define GST_TYPE_GENTLSRC_PRODUCER (gst_gentlsrc_producer_get_type())
static GType
gst_gentlsrc_producer_get_type (void)
{
  static GType gentlsrc_producer_type = 0;
  static const GEnumValue gentlsrc_producer[] = {
    {GST_GENTLSRC_PRODUCER_BASLER, "Basler producer", "basler"},
    {GST_GENTLSRC_PRODUCER_EVT, "EVT producer", "evt"},
    {0, NULL, NULL},
  };

  if (!gentlsrc_producer_type) {
    gentlsrc_producer_type =
        g_enum_register_static ("GstGenTlSrcProducer", gentlsrc_producer);
  }
  return gentlsrc_producer_type;
}

GST_DEBUG_CATEGORY_STATIC (gst_gentlsrc_debug);
#define GST_CAT_DEFAULT gst_gentlsrc_debug

/* prototypes */
static void gst_gentlsrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_gentlsrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_gentlsrc_dispose (GObject * object);
static void gst_gentlsrc_finalize (GObject * object);

static gboolean gst_gentlsrc_start (GstBaseSrc * src);
static gboolean gst_gentlsrc_stop (GstBaseSrc * src);
static GstCaps *gst_gentlsrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_gentlsrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_gentlsrc_unlock (GstBaseSrc * src);
static gboolean gst_gentlsrc_unlock_stop (GstBaseSrc * src);

static GstFlowReturn gst_gentlsrc_create (GstPushSrc * src, GstBuffer ** buf);

static gchar *gst_gentlsrc_get_error_string (GstGenTlSrc * src);
static void gst_gentlsrc_cleanup_tl (GstGenTlSrc * src);

enum
{
  PROP_0,
  PROP_PRODUCER,
  PROP_INTERFACE_INDEX,
  PROP_INTERFACE_ID,
  PROP_DEVICE_INDEX,
  PROP_DEVICE_ID,
  PROP_STREAM_INDEX,
  PROP_STREAM_ID,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_TIMEOUT,
  PROP_ATTRIBUTES
};

#define DEFAULT_PROP_PRODUCER GST_GENTLSRC_PRODUCER_BASLER
#define DEFAULT_PROP_INTERFACE_INDEX 0
#define DEFAULT_PROP_INTERFACE_ID ""
#define DEFAULT_PROP_DEVICE_INDEX 0
#define DEFAULT_PROP_DEVICE_ID ""
#define DEFAULT_PROP_STREAM_INDEX 0
#define DEFAULT_PROP_STREAM_ID ""
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 3
#define DEFAULT_PROP_TIMEOUT 1000
#define DEFAULT_PROP_ATTRIBUTES ""

/* pad templates */

static GstStaticPadTemplate gst_gentlsrc_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, GRAY16_BE, BGRA, UYVY }") ";"
        GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER8 ("{ bggr, grbg, rggb, gbrg }") ";"
        GST_GENICAM_PIXEL_FORMAT_MAKE_BAYER16
        ("{ bggr16, grbg16, rggb16, gbrg16 }", "1234")
    )
    );

#define HANDLE_GTL_ERROR(arg)  \
  if (ret != GC_ERR_SUCCESS) {  \
    GST_ELEMENT_ERROR (src, LIBRARY, FAILED,  \
      (arg ": %s", gst_gentlsrc_get_error_string (src)), (NULL));  \
    goto error; \
  }

#define HANDLE_GTL_WARNING(arg)  \
  if (ret != GC_ERR_SUCCESS) {  \
    GST_ELEMENT_WARNING (src, LIBRARY, FAILED,  \
      (arg ": %s", gst_gentlsrc_get_error_string (src)), (NULL));  \
    goto error; \
  }

PGCGetInfo GTL_GCGetInfo;
PGCGetLastError GTL_GCGetLastError;
PGCInitLib GTL_GCInitLib;
PGCCloseLib GTL_GCCloseLib;
PGCReadPort GTL_GCReadPort;
PGCWritePort GTL_GCWritePort;
PGCGetPortURL GTL_GCGetPortURL;
PGCGetPortInfo GTL_GCGetPortInfo;
PGCRegisterEvent GTL_GCRegisterEvent;
PGCUnregisterEvent GTL_GCUnregisterEvent;
PEventGetData GTL_EventGetData;
PEventGetDataInfo GTL_EventGetDataInfo;
PEventGetInfo GTL_EventGetInfo;
PEventFlush GTL_EventFlush;
PEventKill GTL_EventKill;
PTLOpen GTL_TLOpen;
PTLClose GTL_TLClose;
PTLGetInfo GTL_TLGetInfo;
PTLGetNumInterfaces GTL_TLGetNumInterfaces;
PTLGetInterfaceID GTL_TLGetInterfaceID;
PTLGetInterfaceInfo GTL_TLGetInterfaceInfo;
PTLOpenInterface GTL_TLOpenInterface;
PTLUpdateInterfaceList GTL_TLUpdateInterfaceList;
PIFClose GTL_IFClose;
PIFGetInfo GTL_IFGetInfo;
PIFGetNumDevices GTL_IFGetNumDevices;
PIFGetDeviceID GTL_IFGetDeviceID;
PIFUpdateDeviceList GTL_IFUpdateDeviceList;
PIFGetDeviceInfo GTL_IFGetDeviceInfo;
PIFOpenDevice GTL_IFOpenDevice;
PDevGetPort GTL_DevGetPort;
PDevGetNumDataStreams GTL_DevGetNumDataStreams;
PDevGetDataStreamID GTL_DevGetDataStreamID;
PDevOpenDataStream GTL_DevOpenDataStream;
PDevGetInfo GTL_DevGetInfo;
PDevClose GTL_DevClose;
PDSAnnounceBuffer GTL_DSAnnounceBuffer;
PDSAllocAndAnnounceBuffer GTL_DSAllocAndAnnounceBuffer;
PDSFlushQueue GTL_DSFlushQueue;
PDSStartAcquisition GTL_DSStartAcquisition;
PDSStopAcquisition GTL_DSStopAcquisition;
PDSGetInfo GTL_DSGetInfo;
PDSGetBufferID GTL_DSGetBufferID;
PDSClose GTL_DSClose;
PDSRevokeBuffer GTL_DSRevokeBuffer;
PDSQueueBuffer GTL_DSQueueBuffer;
PDSGetBufferInfo GTL_DSGetBufferInfo;
PGCGetNumPortURLs GTL_GCGetNumPortURLs;
PGCGetPortURLInfo GTL_GCGetPortURLInfo;

#define GTL_BIND(fcn) if (!g_module_symbol (module, G_STRINGIFY(fcn), (gpointer *) & GTL_##fcn)) { \
  GST_DEBUG_OBJECT(src, "Failed to bind function " G_STRINGIFY(fcn)); goto error; }

gboolean
gst_gentlsrc_bind_functions (GstGenTlSrc * src)
{
  GModule *module;
  const char *cti_path = src->producer.cti_path;
  gboolean ret = TRUE;

  GST_DEBUG_OBJECT (src, "Trying to bind functions from '%s'", cti_path);

  module = g_module_open (cti_path, G_MODULE_BIND_LAZY);
  if (!module) {
    GST_ELEMENT_ERROR (src, LIBRARY, INIT,
        ("GenTL CTI %s could not be opened: %s", cti_path, g_module_error ()),
        (NULL));
    return FALSE;
  }

  GTL_BIND (GCGetInfo);
  GTL_BIND (GCGetLastError);
  GTL_BIND (GCInitLib);
  GTL_BIND (GCCloseLib);
  GTL_BIND (GCReadPort);
  GTL_BIND (GCWritePort);
  GTL_BIND (GCGetPortURL);
  GTL_BIND (GCGetPortInfo);
  GTL_BIND (GCRegisterEvent);
  GTL_BIND (GCUnregisterEvent);
  GTL_BIND (EventGetData);
  GTL_BIND (EventGetDataInfo);
  GTL_BIND (EventGetInfo);
  GTL_BIND (EventFlush);
  GTL_BIND (EventKill);
  GTL_BIND (TLOpen);
  GTL_BIND (TLClose);
  GTL_BIND (TLGetInfo);
  GTL_BIND (TLGetNumInterfaces);
  GTL_BIND (TLGetInterfaceID);
  GTL_BIND (TLGetInterfaceInfo);
  GTL_BIND (TLOpenInterface);
  GTL_BIND (TLUpdateInterfaceList);
  GTL_BIND (IFClose);
  GTL_BIND (IFGetInfo);
  GTL_BIND (IFGetNumDevices);
  GTL_BIND (IFGetDeviceID);
  GTL_BIND (IFUpdateDeviceList);
  GTL_BIND (IFGetDeviceInfo);
  GTL_BIND (IFOpenDevice);
  GTL_BIND (DevGetPort);
  GTL_BIND (DevGetNumDataStreams);
  GTL_BIND (DevGetDataStreamID);
  GTL_BIND (DevOpenDataStream);
  GTL_BIND (DevGetInfo);
  GTL_BIND (DevClose);
  GTL_BIND (DSAnnounceBuffer);
  GTL_BIND (DSAllocAndAnnounceBuffer);
  GTL_BIND (DSFlushQueue);
  GTL_BIND (DSStartAcquisition);
  GTL_BIND (DSStopAcquisition);
  GTL_BIND (DSGetInfo);
  GTL_BIND (DSGetBufferID);
  GTL_BIND (DSClose);
  GTL_BIND (DSRevokeBuffer);
  GTL_BIND (DSQueueBuffer);
  GTL_BIND (DSGetBufferInfo);
  GTL_BIND (GCGetNumPortURLs);
  GTL_BIND (GCGetPortURLInfo);

  return TRUE;

error:
  GST_ERROR_OBJECT (src, "One or more functions doesn't exist in %s", cti_path);
  return FALSE;
}


/* class initialization */

G_DEFINE_TYPE (GstGenTlSrc, gst_gentlsrc, GST_TYPE_PUSH_SRC);

static void
gst_gentlsrc_class_init (GstGenTlSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_gentlsrc_set_property;
  gobject_class->get_property = gst_gentlsrc_get_property;
  gobject_class->dispose = gst_gentlsrc_dispose;
  gobject_class->finalize = gst_gentlsrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_gentlsrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "GenTL Video Source", "Source/Video",
      "GenTL framegrabber video source", "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_gentlsrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_gentlsrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_gentlsrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_gentlsrc_set_caps);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_gentlsrc_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_gentlsrc_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_gentlsrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_PRODUCER,
      g_param_spec_enum ("producer", "Producer", "GenTL producer",
          GST_TYPE_GENTLSRC_PRODUCER, DEFAULT_PROP_PRODUCER,
          G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE |
          GST_PARAM_MUTABLE_PLAYING));

  g_object_class_install_property (gobject_class, PROP_INTERFACE_INDEX,
      g_param_spec_uint ("interface-index", "Interface index",
          "Interface index number, zero-based, overridden by interface-id",
          0, G_MAXUINT, DEFAULT_PROP_INTERFACE_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_INTERFACE_ID,
      g_param_spec_string ("interface-id", "Interface ID",
          "Interface ID, overrides interface-index if not empty string",
          DEFAULT_PROP_INTERFACE_ID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_uint ("device-index", "Device index",
          "Device index number, zero-based, overridden by device-id",
          0, G_MAXUINT, DEFAULT_PROP_DEVICE_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_DEVICE_ID,
      g_param_spec_string ("device-id", "Device ID",
          "Device ID, overrides device-index if not empty string",
          DEFAULT_PROP_DEVICE_ID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_STREAM_INDEX,
      g_param_spec_uint ("stream-index", "Stream index",
          "Stream index number, zero-based, overridden by stream-id",
          0, G_MAXUINT, DEFAULT_PROP_STREAM_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_STREAM_ID,
      g_param_spec_string ("stream-id", "Stream ID",
          "Stream ID, overrides stream-index if not empty string",
          DEFAULT_PROP_STREAM_ID,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_uint ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 1, G_MAXUINT,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_TIMEOUT, g_param_spec_int ("timeout",
          "Timeout (ms)",
          "Timeout in ms (0 to use default)", 0, G_MAXINT,
          DEFAULT_PROP_TIMEOUT, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_ATTRIBUTES, g_param_spec_string ("attributes",
          "Attributes", "Attributes to change, comma separated key=value pairs",
          DEFAULT_PROP_ATTRIBUTES, G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE));

  klass->hTL = NULL;
  g_mutex_init (&klass->tl_mutex);
  klass->tl_refcount = 0;
}

static void
gst_gentlsrc_reset (GstGenTlSrc * src)
{
  src->gentl_latched_ns = 0;
  src->unix_latched_ns = 0;

  src->error_string[0] = 0;
  src->last_frame_count = 0;
  src->total_dropped_frames = 0;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }
}

static void
gst_gentlsrc_init (GstGenTlSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->producer_prop = DEFAULT_PROP_PRODUCER;
  src->interface_index = DEFAULT_PROP_INTERFACE_INDEX;
  src->interface_id = g_strdup (DEFAULT_PROP_INTERFACE_ID);
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;
  src->timeout = DEFAULT_PROP_TIMEOUT;
  src->attributes = g_strdup (DEFAULT_PROP_ATTRIBUTES);

  src->stop_requested = FALSE;
  src->caps = NULL;

  src->hTL = NULL;
  src->hIF = NULL;
  src->hDEV = NULL;
  src->hDS = NULL;

  gst_gentlsrc_reset (src);
}

void
gst_gentlsrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGenTlSrc *src;

  src = GST_GENTL_SRC (object);

  switch (property_id) {
    case PROP_PRODUCER:
      src->producer_prop = g_value_get_enum (value);
      break;
    case PROP_INTERFACE_INDEX:
      src->interface_index = g_value_get_uint (value);
      break;
    case PROP_INTERFACE_ID:
      g_free (src->interface_id);
      src->interface_id = g_strdup (g_value_get_string (value));
      break;
    case PROP_DEVICE_INDEX:
      src->device_index = g_value_get_uint (value);
      break;
    case PROP_DEVICE_ID:
      g_free (src->device_id);
      src->device_id = g_strdup (g_value_get_string (value));
      break;
    case PROP_STREAM_INDEX:
      src->stream_index = g_value_get_uint (value);
      break;
    case PROP_STREAM_ID:
      g_free (src->stream_id);
      src->stream_id = g_strdup (g_value_get_string (value));
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      src->num_capture_buffers = g_value_get_uint (value);
      break;
    case PROP_TIMEOUT:
      src->timeout = g_value_get_int (value);
      break;
    case PROP_ATTRIBUTES:
      if (src->attributes)
        g_free (src->attributes);
      src->attributes = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_gentlsrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstGenTlSrc *src;

  g_return_if_fail (GST_IS_GENTL_SRC (object));
  src = GST_GENTL_SRC (object);

  switch (property_id) {
    case PROP_PRODUCER:
      g_value_set_enum (value, src->producer_prop);
      break;
    case PROP_INTERFACE_INDEX:
      g_value_set_uint (value, src->interface_index);
      break;
    case PROP_INTERFACE_ID:
      g_value_set_string (value, src->interface_id);
      break;
    case PROP_DEVICE_INDEX:
      g_value_set_uint (value, src->device_index);
      break;
    case PROP_DEVICE_ID:
      g_value_set_string (value, src->device_id);
      break;
    case PROP_STREAM_INDEX:
      g_value_set_uint (value, src->stream_index);
      break;
    case PROP_STREAM_ID:
      g_value_set_string (value, src->stream_id);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      g_value_set_uint (value, src->num_capture_buffers);
      break;
    case PROP_TIMEOUT:
      g_value_set_int (value, src->timeout);
      break;
    case PROP_ATTRIBUTES:
      g_value_set_string (value, src->attributes);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_gentlsrc_dispose (GObject * object)
{
  GstGenTlSrc *src;

  g_return_if_fail (GST_IS_GENTL_SRC (object));
  src = GST_GENTL_SRC (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_gentlsrc_parent_class)->dispose (object);
}

void
gst_gentlsrc_finalize (GObject * object)
{
  GstGenTlSrc *src;

  g_return_if_fail (GST_IS_GENTL_SRC (object));
  src = GST_GENTL_SRC (object);

  /* clean up object here */

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  G_OBJECT_CLASS (gst_gentlsrc_parent_class)->finalize (object);
}

#define GTL_MAX_STR_SIZE 256

void
gst_gentl_print_gentl_impl_info (GstGenTlSrc * src)
{
  size_t str_size;
  char id[GTL_MAX_STR_SIZE];
  char vendor[GTL_MAX_STR_SIZE];
  char model[GTL_MAX_STR_SIZE];
  char version[GTL_MAX_STR_SIZE];
  char tl_type[GTL_MAX_STR_SIZE];
  char name[GTL_MAX_STR_SIZE];
  char path_name[GTL_MAX_STR_SIZE];
  char display_name[GTL_MAX_STR_SIZE];
  INFO_DATATYPE datatype;

  str_size = GTL_MAX_STR_SIZE;
  GTL_GCGetInfo (TL_INFO_ID, &datatype, id, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_GCGetInfo (TL_INFO_VENDOR, &datatype, vendor, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_GCGetInfo (TL_INFO_MODEL, &datatype, model, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_GCGetInfo (TL_INFO_VERSION, &datatype, version, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_GCGetInfo (TL_INFO_TLTYPE, &datatype, tl_type, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_GCGetInfo (TL_INFO_NAME, &datatype, name, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_GCGetInfo (TL_INFO_PATHNAME, &datatype, path_name, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_GCGetInfo (TL_INFO_DISPLAYNAME, &datatype, display_name, &str_size);

  GST_DEBUG_OBJECT (src,
      "ID=%s, Vendor=%s, Model=%s, Version=%s, TL_Type=%s, Name=%s, Path_Name=%s, Display_Name=%s",
      id, vendor, model, version, tl_type, name, path_name, display_name);
}

void
gst_gentl_print_system_info (GstGenTlSrc * src)
{
  size_t str_size;
  char id[GTL_MAX_STR_SIZE];
  char vendor[GTL_MAX_STR_SIZE];
  char model[GTL_MAX_STR_SIZE];
  char version[GTL_MAX_STR_SIZE];
  char tl_type[GTL_MAX_STR_SIZE];
  char name[GTL_MAX_STR_SIZE];
  char path_name[GTL_MAX_STR_SIZE];
  char display_name[GTL_MAX_STR_SIZE];
  INFO_DATATYPE datatype;

  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInfo (src->hTL, TL_INFO_ID, &datatype, id, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInfo (src->hTL, TL_INFO_VENDOR, &datatype, vendor, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInfo (src->hTL, TL_INFO_MODEL, &datatype, model, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInfo (src->hTL, TL_INFO_VERSION, &datatype, version, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInfo (src->hTL, TL_INFO_TLTYPE, &datatype, tl_type, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInfo (src->hTL, TL_INFO_NAME, &datatype, name, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInfo (src->hTL, TL_INFO_PATHNAME, &datatype, path_name, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInfo (src->hTL, TL_INFO_DISPLAYNAME, &datatype, display_name,
      &str_size);

  GST_DEBUG_OBJECT (src,
      "System: ID=%s, Vendor=%s, Model=%s, Version=%s, TL_Type=%s, Name=%s, Path_Name=%s, Display_Name=%s",
      id, vendor, model, version, tl_type, name, path_name, display_name);
}

void
gst_gentl_print_interface_info (GstGenTlSrc * src, uint32_t index)
{
  GC_ERROR ret;
  size_t str_size;
  char iface_id[GTL_MAX_STR_SIZE];
  char id[GTL_MAX_STR_SIZE];
  char tl_type[GTL_MAX_STR_SIZE];
  char display_name[GTL_MAX_STR_SIZE];
  INFO_DATATYPE datatype;

  str_size = GTL_MAX_STR_SIZE;
  ret = GTL_TLGetInterfaceID (src->hTL, index, iface_id, &str_size);
  if (ret != GC_ERR_SUCCESS) {
    GST_WARNING_OBJECT (src, "Failed to get interface id (error=%d): %s", ret,
        gst_gentlsrc_get_error_string (src));
    return;
  }

  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInterfaceInfo (src->hTL, iface_id, INTERFACE_INFO_ID, &datatype, id,
      &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInterfaceInfo (src->hTL, iface_id, INTERFACE_INFO_DISPLAYNAME,
      &datatype, display_name, &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_TLGetInterfaceInfo (src->hTL, iface_id, INTERFACE_INFO_TLTYPE, &datatype,
      tl_type, &str_size);

  GST_DEBUG_OBJECT (src, "Interface %d: ID=%s, TL_Type=%s, Display_Name=%s",
      index, id, tl_type, display_name);
}

void
gst_gentl_print_device_info (GstGenTlSrc * src, uint32_t index)
{
  GC_ERROR ret;
  size_t str_size;
  char dev_id[GTL_MAX_STR_SIZE];
  char id[GTL_MAX_STR_SIZE];
  char vendor[GTL_MAX_STR_SIZE];
  char model[GTL_MAX_STR_SIZE];
  char tl_type[GTL_MAX_STR_SIZE];
  char display_name[GTL_MAX_STR_SIZE];
  gint32 access_status;
  INFO_DATATYPE datatype;

  ret = GTL_IFGetDeviceID (src->hIF, index, dev_id, &str_size);
  if (ret != GC_ERR_SUCCESS) {
    GST_WARNING_OBJECT (src, "Failed to get device id: %s",
        gst_gentlsrc_get_error_string (src));
    return;
  }

  str_size = GTL_MAX_STR_SIZE;
  GTL_IFGetDeviceInfo (src->hIF, dev_id, DEVICE_INFO_ID, &datatype, id,
      &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_IFGetDeviceInfo (src->hIF, dev_id, DEVICE_INFO_VENDOR, &datatype, vendor,
      &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_IFGetDeviceInfo (src->hIF, dev_id, DEVICE_INFO_MODEL, &datatype, model,
      &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_IFGetDeviceInfo (src->hIF, dev_id, DEVICE_INFO_TLTYPE, &datatype, tl_type,
      &str_size);
  str_size = GTL_MAX_STR_SIZE;
  GTL_IFGetDeviceInfo (src->hIF, dev_id, DEVICE_INFO_DISPLAYNAME, &datatype,
      display_name, &str_size);
  str_size = sizeof (access_status);
  GTL_IFGetDeviceInfo (src->hIF, dev_id, DEVICE_INFO_ACCESS_STATUS, &datatype,
      &access_status, &str_size);

  GST_DEBUG_OBJECT (src,
      "Device %d: ID=%s, Vendor=%s, Model=%s, TL_Type=%s, Display_Name=%s, Access_Status=%d",
      index, id, vendor, model, tl_type, display_name, access_status);
}

//void gst_gentl_print_stream_info (GstGenTlSrc * src)
//{
//    GC_ERROR ret;
//    size_t str_size;
//    char dev_id[GTL_MAX_STR_SIZE];
//    char id[GTL_MAX_STR_SIZE];
//    char vendor[GTL_MAX_STR_SIZE];
//    char model[GTL_MAX_STR_SIZE];
//    char tl_type[GTL_MAX_STR_SIZE];
//    char display_name[GTL_MAX_STR_SIZE];
//    gint32 access_status;
//    INFO_DATATYPE datatype;
//
//    //ret = GTL_DevGetDataStreamID(src->hIF, index, dev_id, &str_size);
//    //if (ret != GC_ERR_SUCCESS) {
//    //    GST_WARNING_OBJECT (src, "Failed to get stream id: %s", gst_gentlsrc_get_error_string(src));
//    //    return;
//    //}
//
//    str_size = GTL_MAX_STR_SIZE;
//    GTL_DSGetInfo(src->hDEV, DEVICE_INFO_ID, &datatype, id, &str_size);
//    str_size = GTL_MAX_STR_SIZE;
//    GTL_DSGetInfo(src->hDEV, DEVICE_INFO_VENDOR, &datatype, vendor, &str_size);
//    str_size = GTL_MAX_STR_SIZE;
//    GTL_DSGetInfo(src->hDEV, DEVICE_INFO_MODEL, &datatype, model, &str_size);
//    str_size = GTL_MAX_STR_SIZE;
//    GTL_DSGetInfo(src->hDEV, DEVICE_INFO_TLTYPE, &datatype, tl_type, &str_size);
//    str_size = GTL_MAX_STR_SIZE;
//    GTL_DSGetInfo(src->hDEV, DEVICE_INFO_DISPLAYNAME, &datatype, display_name, &str_size);
//    str_size = sizeof(access_status);
//    GTL_DSGetInfo(src->hDEV, DEVICE_INFO_ACCESS_STATUS, &datatype, &access_status, &str_size);
//
//    GST_DEBUG_OBJECT (src, "Device %d: ID=%s, Vendor=%s, Model=%s, TL_Type=%s, Display_Name=%s, Access_Status=%d",
//        index, id, vendor, model, tl_type, display_name, access_status);
//}


static size_t
gst_gentlsrc_get_payload_size (GstGenTlSrc * src)
{
  GC_ERROR ret;
  INFO_DATATYPE info_datatype;
  size_t info_size;
  bool8_t size_defined;
  size_t payload_size = 0;

  info_size = sizeof (size_defined);
  ret =
      GTL_DSGetInfo (src->hDS, STREAM_INFO_DEFINES_PAYLOADSIZE, &info_datatype,
      &size_defined, &info_size);

  if (size_defined) {
    info_size = sizeof (payload_size);
    ret =
        GTL_DSGetInfo (src->hDS, STREAM_INFO_PAYLOAD_SIZE, &info_datatype,
        &payload_size, &info_size);
    GST_DEBUG_OBJECT (src, "Payload size defined by stream info: %d",
        payload_size);
  } else {
    guint32 val = 0;
    size_t datasize = 4;
    // TODO: use node map
    ret =
        GTL_GCReadPort (src->hDevPort, src->producer.payload_size, &val,
        &datasize);
    HANDLE_GTL_ERROR ("Failed to get payload size");
    payload_size = GUINT32_FROM_BE (val);
    GST_DEBUG_OBJECT (src, "Payload size defined by node map: %d",
        payload_size);

    //PORT_HANDLE port_handle;
    //ret = GTL_DevGetPort(src->hDEV, &port_handle);

    //GTL_GCGetNum

    //GTL_GCReadPort(port_handle, )
    ////GTL_GCGetPortInfo(port_handle, )
  }

  return payload_size;

error:
  return 0;
}

static gboolean
gst_gentlsrc_prepare_buffers (GstGenTlSrc * src)
{
  size_t payload_size;
  guint i;
  BUFFER_HANDLE hBuffer;
  GC_ERROR ret;

  /* TODO: query Data Stream features to find min/max num_buffers */
  payload_size = gst_gentlsrc_get_payload_size (src);
  if (payload_size == 0) {
    GST_DEBUG_OBJECT (src, "Payload size is zero");
    return FALSE;
  }

  for (i = 0; i < src->num_capture_buffers; ++i) {
    ret = GTL_DSAllocAndAnnounceBuffer (src->hDS, payload_size, NULL, &hBuffer);
    HANDLE_GTL_ERROR ("Failed to alloc and announce buffer");

    ret = GTL_DSQueueBuffer (src->hDS, hBuffer);
    HANDLE_GTL_ERROR ("Failed to queue buffer");
  }

  ret = GTL_DSFlushQueue (src->hDS, ACQ_QUEUE_ALL_TO_INPUT);
  HANDLE_GTL_ERROR ("Failed to queue all buffers to input");

  return TRUE;

error:
  return FALSE;
}

static guint64
gst_gentlsrc_get_gev_tick_frequency (GstGenTlSrc * src)
{
  GC_ERROR ret;

  if (!src->producer.tick_frequency_high || !src->producer.tick_frequency_low)
    return 0;

  guint32 freq_low, freq_high;
  size_t datasize = 4;
  ret = GTL_GCReadPort (src->hDevPort, src->producer.tick_frequency_low, &freq_low, &datasize); // GevTimestampTickFrequencyLow
  HANDLE_GTL_ERROR ("Failed to get GevTimestampTickFrequencyLow");
  ret = GTL_GCReadPort (src->hDevPort, src->producer.tick_frequency_high, &freq_high, &datasize);       // GevTimestampTickFrequencyHigh
  HANDLE_GTL_ERROR ("Failed to get GevTimestampTickFrequencyHigh");

  guint64 tick_frequency =
      GUINT64_FROM_BE ((guint64) freq_low << 32 | freq_high);
  GST_DEBUG_OBJECT (src, "GEV Timestamp tick frequency is %llu",
      tick_frequency);

  return tick_frequency;

error:
  return 0;
}

static guint64
gst_gentlsrc_get_gev_timestamp_ticks (GstGenTlSrc * src)
{
  GC_ERROR ret;
  size_t datasize = 4;
  guint32 val, ts_low, ts_high;

  val = GUINT32_TO_BE (2);
  datasize = sizeof (val);
  ret = GTL_GCWritePort (src->hDevPort, src->producer.timestamp_control_latch, &val, &datasize);        // GevTimestampControlLatch
  HANDLE_GTL_WARNING ("Failed to latch timestamp GevTimestampControlLatch");

  ret = GTL_GCReadPort (src->hDevPort, src->producer.timestamp_low, &ts_low, &datasize);        // GevTimestampValueLow
  HANDLE_GTL_WARNING ("Failed to get GevTimestampValueLow");
  ret = GTL_GCReadPort (src->hDevPort, src->producer.timestamp_high, &ts_high, &datasize);      // GevTimestampValueHigh
  HANDLE_GTL_WARNING ("Failed to get GevTimestampValueHigh");
  guint64 ticks = GUINT64_FROM_BE ((guint64) ts_low << 32 | ts_high);
  GST_LOG_OBJECT (src, "Timestamp ticks are %llu", ticks);

  return ticks;

error:
  return 0;
}

static void
gst_gentlsrc_src_latch_timestamps (GstGenTlSrc * src)
{
  guint64 unix_ts, gev_ts;

  unix_ts = get_unix_ns ();
  gev_ts = gst_gentlsrc_get_gev_timestamp_ticks (src);

  if (gev_ts != 0) {
    src->unix_latched_ns = unix_ts;
    src->gentl_latched_ns = ((gint64)
      (gev_ts * ((double)GST_SECOND / src->tick_frequency)));;
  } else {
    GST_WARNING_OBJECT (src, "Failed to latch GEV time, using old latch value");
  }
}

static void
gst_gentlsrc_set_attributes (GstGenTlSrc * src)
{
  gchar **pairs;
  int i;
  guint32 val;
  size_t datasize;
  GC_ERROR ret;

  if (!src->attributes || src->attributes == 0) {
    return;
  }

  GST_DEBUG_OBJECT (src, "Trying to set following attributes: '%s'",
      src->attributes);

  pairs = g_strsplit (src->attributes, ";", 0);

  for (i = 0;; i++) {
    gchar **pair;

    if (!pairs[i])
      break;

    pair = g_strsplit (pairs[i], "=", 2);

    if (!pair[0] || !pair[1]) {
      GST_WARNING_OBJECT (src, "Failed to parse attribute/value: '%s'", pair);
      continue;
    }

    GST_DEBUG_OBJECT (src, "Setting attribute, '%s'='%s'", pair[0], pair[1]);

    val = GUINT32_TO_BE (atoi (pair[1]));
    datasize = sizeof (val);
    ret =
        GTL_GCWritePort (src->hDevPort, strtol (pair[0], NULL, 16), &val,
        &datasize);
    if (ret != GC_ERR_SUCCESS) {
      GST_WARNING_OBJECT (src, "Failed to set attribute: %s",
          gst_gentlsrc_get_error_string (src));
    }
    g_strfreev (pair);
  }
  g_strfreev (pairs);

  if (src->attributes) {
    g_free (src->attributes);
    src->attributes = NULL;
  }
}

static gboolean
gst_gentlsrc_open_tl (GstGenTlSrc * src)
{
  GstGenTlSrcClass *klass = GST_GENTL_SRC_GET_CLASS (src);
  GC_ERROR ret;
  uint32_t i, num_ifaces;

  /* open framegrabber if it isn't already opened */
  if (klass->tl_refcount > 0) {
    GST_DEBUG_OBJECT (src,
        "Framegrabber interface already opened in this process, reusing");
    src->hTL = klass->hTL;
    klass->tl_refcount++;
  } else {
    /* initialize library and print info */
    ret = GTL_GCInitLib ();
    //HANDLE_GTL_ERROR ("GenTL Producer library could not be initialized");

    gst_gentl_print_gentl_impl_info (src);

    /* open GenTL, print info, and update interface list */
    ret = GTL_TLOpen (&src->hTL);
    HANDLE_GTL_ERROR ("System module failed to open");

    gst_gentl_print_system_info (src);

    ret = GTL_TLUpdateInterfaceList (src->hTL, NULL, src->timeout);
    HANDLE_GTL_ERROR ("Failed to update interface list within timeout");

    /* print info for all interfaces and open specified interface */
    ret = GTL_TLGetNumInterfaces (src->hTL, &num_ifaces);
    HANDLE_GTL_ERROR ("Failed to get number of interfaces");
    if (num_ifaces > 0) {
      GST_DEBUG_OBJECT (src, "Found %d GenTL interfaces", num_ifaces);
      for (i = 0; i < num_ifaces; ++i) {
        gst_gentl_print_interface_info (src, i);
      }
    } else {
      GST_ELEMENT_ERROR (src, LIBRARY, FAILED, ("No interfaces found"), (NULL));
      goto error;
    }

    klass->hTL = src->hTL;
    klass->tl_refcount++;
  }

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_gentlsrc_open_interface (GstGenTlSrc * src)
{
  GstGenTlSrcClass *klass = GST_GENTL_SRC_GET_CLASS (src);
  GC_ERROR ret;

  if (!src->interface_id || src->interface_id[0] == 0) {
    size_t id_size;
    GST_DEBUG_OBJECT (src, "Trying to find interface ID at index %d",
        src->interface_index);

    ret = GTL_TLGetInterfaceID (src->hTL, src->interface_index, NULL, &id_size);
    HANDLE_GTL_ERROR ("Failed to get interface ID at specified index");
    if (src->interface_id) {
      g_free (src->interface_id);
    }
    src->interface_id = (gchar *) g_malloc (id_size);
    ret =
        GTL_TLGetInterfaceID (src->hTL, src->interface_index, src->interface_id,
        &id_size);
    HANDLE_GTL_ERROR ("Failed to get interface ID at specified index");
  }

  GST_DEBUG_OBJECT (src, "Trying to open interface '%s'", src->interface_id);
  ret = GTL_TLOpenInterface (src->hTL, src->interface_id, &src->hIF);
  HANDLE_GTL_ERROR ("Interface module failed to open");

  return TRUE;

error:
  return FALSE;
}

static gboolean
gst_gentlsrc_start (GstBaseSrc * bsrc)
{
  GstGenTlSrc *src = GST_GENTL_SRC (bsrc);
  GstGenTlSrcClass *klass = GST_GENTL_SRC_GET_CLASS (src);
  GC_ERROR ret;
  uint32_t i, num_devs;
  guint32 width, height, stride;
  GstVideoInfo vinfo;

  GST_DEBUG_OBJECT (src, "start");

  if (src->producer_prop == GST_GENTLSRC_PRODUCER_BASLER) {
    initialize_basler_addresses (&src->producer);
  } else if (src->producer_prop == GST_GENTLSRC_PRODUCER_EVT) {
    initialize_evt_addresses (&src->producer);
  } else {
    g_assert_not_reached ();
  }

  /* bind functions from CTI */
  /* TODO: Enumerate CTI files in env var GENTL_GENTL64_PATH */
  if (!gst_gentlsrc_bind_functions (src)) {
    GST_ELEMENT_ERROR (src, LIBRARY, INIT,
        ("GenTL CTI could not be opened: %s", g_module_error ()), (NULL));
    return FALSE;
  }

  g_mutex_lock (&klass->tl_mutex);

  if (!gst_gentlsrc_open_tl (src)) {
    g_mutex_unlock (&klass->tl_mutex);
    goto error;
  }

  if (!gst_gentlsrc_open_interface (src)) {
    g_mutex_unlock (&klass->tl_mutex);
    goto error;
  }

  g_mutex_unlock (&klass->tl_mutex);

  ret = GTL_IFUpdateDeviceList (src->hIF, NULL, src->timeout);
  HANDLE_GTL_ERROR ("Failed to update device list within timeout");

  /* print info for all devices and open specified device */
  ret = GTL_IFGetNumDevices (src->hIF, &num_devs);
  HANDLE_GTL_ERROR ("Failed to get number of devices");
  if (num_devs > 0) {
    for (i = 0; i < num_devs; ++i) {
      gst_gentl_print_device_info (src, i);
    }
  } else {
    GST_ELEMENT_ERROR (src, LIBRARY, FAILED,
        ("No devices found on interface"), (NULL));
    goto error;
  }

  if (!src->device_id || src->device_id[0] == 0) {
    size_t id_size;
    GST_DEBUG_OBJECT (src, "Trying to find device ID at index %d",
        src->device_index);

    GTL_IFGetDeviceID (src->hIF, src->device_index, NULL, &id_size);
    HANDLE_GTL_ERROR ("Failed to get device ID at specified index");
    if (src->device_id) {
      g_free (src->device_id);
    }
    src->device_id = (gchar *) g_malloc (id_size);
    GTL_IFGetDeviceID (src->hIF, src->device_index, src->device_id, &id_size);
    HANDLE_GTL_ERROR ("Failed to get device ID at specified index");
  }

  GST_DEBUG_OBJECT (src, "Trying to open device '%s'", src->device_id);
  ret =
      GTL_IFOpenDevice (src->hIF, src->device_id, DEVICE_ACCESS_CONTROL,
      &src->hDEV);
  HANDLE_GTL_ERROR ("Failed to open device");

  uint32_t num_data_streams;
  ret = GTL_DevGetNumDataStreams (src->hDEV, &num_data_streams);
  HANDLE_GTL_ERROR ("Failed to get number of data streams");
  GST_DEBUG_OBJECT (src, "Found %d data streams", num_data_streams);

  /* find and open specified data stream id */
  if (!src->stream_id || src->stream_id[0] == 0) {
    size_t id_size;
    GST_DEBUG_OBJECT (src, "Trying to find stream ID at index %d",
        src->stream_index);

    GTL_DevGetDataStreamID (src->hDEV, src->stream_index, NULL, &id_size);
    HANDLE_GTL_ERROR ("Failed to get stream ID at specified index");
    if (src->stream_id) {
      g_free (src->stream_id);
    }
    src->stream_id = (gchar *) g_malloc (id_size);
    GTL_DevGetDataStreamID (src->hDEV, src->stream_index, src->stream_id,
        &id_size);
    HANDLE_GTL_ERROR ("Failed to get stream ID at specified index");
  }

  GST_DEBUG_OBJECT (src, "Trying to open data stream '%s'", src->stream_id);
  ret = GTL_DevOpenDataStream (src->hDEV, src->stream_id, &src->hDS);
  HANDLE_GTL_ERROR ("Failed to open data stream");

  {
    uint32_t num_urls = 0;
    char url[2048];
    size_t url_len = sizeof (url);
    INFO_DATATYPE datatype;
    const uint32_t url_index = 0;

    ret = GTL_DevGetPort (src->hDEV, &src->hDevPort);
    HANDLE_GTL_ERROR ("Failed to get port on device");
    ret = GTL_GCGetNumPortURLs (src->hDevPort, &num_urls);
    HANDLE_GTL_ERROR ("Failed to get number of port URLs");

    GST_DEBUG_OBJECT (src, "Found %d port URLs", num_urls);

    GST_DEBUG_OBJECT (src, "Trying to get URL index %d", url_index);
    GTL_GCGetPortURLInfo (src->hDevPort, url_index, URL_INFO_URL, &datatype,
        url, &url_len);
    HANDLE_GTL_ERROR ("Failed to get URL");
    GST_DEBUG_OBJECT (src, "Found URL '%s'", url);

    g_assert (url_len > 6);
    if (g_str_has_prefix (url, "file")) {
      GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY,
          ("file url not supported yet"), (NULL));
      goto error;
    } else if (g_ascii_strncasecmp (url, "local", 5) == 0) {
      GError *err = NULL;
      GMatchInfo *matchInfo;
      GRegex *regex;
      gchar *filename, *addr_str, *len_str;
      uint64_t addr;
      size_t len;
      gchar *buf;

      regex =
          g_regex_new
          ("[lL]ocal:(?:///)?(?<filename>[^;]+);(?<address>[^;]+);(?<length>[^?]+)(?:[?]SchemaVersion=([^&]+))?",
          (GRegexCompileFlags) 0, (GRegexMatchFlags) 0, &err);
      if (!regex) {
        goto error;
      }
      g_regex_match (regex, url, (GRegexMatchFlags) 0, &matchInfo);
      filename = g_match_info_fetch_named (matchInfo, "filename");
      addr_str = g_match_info_fetch_named (matchInfo, "address");
      len_str = g_match_info_fetch_named (matchInfo, "length");
      if (!filename || !addr_str || !len_str) {
        GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY,
            ("Failed to parse local URL"), (NULL));
        goto error;
      }

      addr = g_ascii_strtoull (addr_str, NULL, 16);
      len = g_ascii_strtoull (len_str, NULL, 16);
      buf = (gchar *) g_malloc (len);
      GTL_GCReadPort (src->hDevPort, addr, buf, &len);
      HANDLE_GTL_ERROR ("Failed to read XML from port");

      if (g_str_has_suffix (filename, "zip")) {
        gchar *zipfilepath;
        unzFile uf;
        unz_file_info64 fileinfo;
        gchar xmlfilename[2048];
        gchar *xml;

        zipfilepath = g_build_filename (g_get_tmp_dir (), filename, NULL);
        GST_DEBUG_OBJECT (src, "Writing XML ZIP file to %s", zipfilepath);
        if (!g_file_set_contents (zipfilepath, buf, len, &err)) {
          GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY,
              ("Failed to write zipped XML to %s", zipfilepath), (NULL));
          goto error;
        }
        uf = unzOpen64 (zipfilepath);
        if (!uf) {
          GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY,
              ("Failed to open zipped XML %s", zipfilepath), (NULL));
          goto error;
        }
        //ret = unzGetGlobalInfo64(uf, &gi);
        ret =
            unzGetCurrentFileInfo64 (uf, &fileinfo, xmlfilename,
            sizeof (xmlfilename), NULL, 0, NULL, 0);
        if (ret != UNZ_OK) {
          GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY,
              ("Failed to query zip file %s", zipfilepath), (NULL));
          goto error;
        }

        ret = unzOpenCurrentFile (uf);
        if (ret != UNZ_OK) {
          GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY,
              ("Failed to extract file %s", xmlfilename), (NULL));
          goto error;
        }

        xml = (gchar *) g_malloc (fileinfo.uncompressed_size);
        if (!xml) {
          GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY,
              ("Failed to allocate memory to extract XML file"), (NULL));
          goto error;
        }

        ret = unzReadCurrentFile (uf, xml, fileinfo.uncompressed_size);
        if (ret != fileinfo.uncompressed_size) {
          GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY,
              ("Failed to extract XML file %s", xmlfilename), (NULL));
          goto error;
        }
        unzClose (uf);
        g_free (zipfilepath);

        zipfilepath = g_build_filename (g_get_tmp_dir (), xmlfilename, NULL);
        GST_DEBUG_OBJECT (src, "Writing XML file to %s", zipfilepath);
        g_file_set_contents (zipfilepath, xml, fileinfo.uncompressed_size,
            &err);
        g_free (zipfilepath);

        g_free (xml);
        //GZlibDecompressor *decompress;
        //char *unzipped;
        //gsize outbuf_size, bytes_read, bytes_written;
        //GInputStream *zippedstream, *unzippedstream;
        //decompress = g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_ZLIB);

        ////zippedstream = g_memory_input_stream_new_from_data(buf, len, g_free);
        ////unzippedstream = g_converter_input_stream_new (zippedstream, G_CONVERTER(decompress));
        ////g_input_stream_read_all (G_INPUT_STREAM(unzippedstream), 
        ////    g_converter_output_stream
        //outbuf_size = 10000000;
        //unzipped = (gchar*) g_malloc(outbuf_size);
        //g_converter_convert (G_CONVERTER (decompress), buf, len, unzipped, outbuf_size, G_CONVERTER_NO_FLAGS, &bytes_read, &bytes_written, &err);
        //GST_DEBUG_OBJECT (src, unzipped);
      }

      g_free (filename);
      g_free (addr_str);
      g_free (len_str);
      g_free (buf);
    } else if (g_str_has_prefix (url, "http")) {
      GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY,
          ("file url not supported yet"), (NULL));
      goto error;
    }
  }

  src->tick_frequency = gst_gentlsrc_get_gev_tick_frequency (src);

  gst_gentlsrc_set_attributes (src);

  {
    // TODO: use GenTl node map for this
    guint32 val = 0;
    size_t datasize = 4;
    ret = GTL_GCReadPort (src->hDevPort, src->producer.width, &val, &datasize);
    HANDLE_GTL_ERROR ("Failed to get width");
    width = GUINT32_FROM_BE (val);
    ret = GTL_GCReadPort (src->hDevPort, src->producer.height, &val, &datasize);
    HANDLE_GTL_ERROR ("Failed to get height");
    height = GUINT32_FROM_BE (val);
    GST_DEBUG_OBJECT (src, "Width and height %dx%d", width, height);

    ret =
        GTL_GCReadPort (src->hDevPort, src->producer.pixel_format, &val,
        &datasize);
    HANDLE_GTL_ERROR ("Failed to get height");
    const char *genicam_pixfmt;
    guint32 pixfmt_enum = GUINT32_FROM_BE (val);
    switch (pixfmt_enum) {
      case 0x1:                // Basler Ace
      case 0x01080001:
        genicam_pixfmt = "Mono8";
        break;
      case 0x5:                // Basler Ace
      case 0x01100005:
        genicam_pixfmt = "Mono12";
        break;
      case 0x1100010:          // Basler Ace
        genicam_pixfmt = "BayerGR12";
        break;
      case 0x01080009:
        genicam_pixfmt = "BayerRG8";
        break;
      case 0x01100011:
        genicam_pixfmt = "BayerRG12";
        break;
      case 0x02180014:
        genicam_pixfmt = "RGB8Packed";
        break;
      case 0x02180015:
        genicam_pixfmt = "BGR8Packed";
        break;
      case 0x0210001F:
        genicam_pixfmt = "YUV422Packed";
        break;
      case 0x02180020:
        genicam_pixfmt = "YUV444Packed";
        break;
      default:
        GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY,
            ("Unrecognized PixelFormat enum value: %d", pixfmt_enum), (NULL));
        goto error;
    }

    /* create caps */
    if (src->caps) {
      gst_caps_unref (src->caps);
      src->caps = NULL;
    }

    src->caps =
        gst_genicam_pixel_format_caps_from_pixel_format (genicam_pixfmt,
        G_LITTLE_ENDIAN, width, height, 30, 1, 1, 1);
    gst_video_info_from_caps (&vinfo, src->caps);
    if (!src->caps) {
      GST_ELEMENT_ERROR (src, STREAM, WRONG_TYPE,
          ("Unknown or unsupported pixel format (%s).", genicam_pixfmt),
          (NULL));
      goto error;
    }

    src->height = vinfo.height;
    src->gst_stride = GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0);
  }

  if (!gst_gentlsrc_prepare_buffers (src)) {
    GST_ELEMENT_ERROR (src, RESOURCE, TOO_LAZY, ("Failed to prepare buffers"),
        (NULL));
    goto error;
  }

  {
    ret =
        GTL_GCRegisterEvent (src->hDS, EVENT_NEW_BUFFER, &src->hNewBufferEvent);
    HANDLE_GTL_ERROR ("Failed to register New Buffer event");
  }

  ret =
      GTL_DSStartAcquisition (src->hDS, ACQ_START_FLAGS_DEFAULT,
      GENTL_INFINITE);
  HANDLE_GTL_ERROR ("Failed to start stream acquisition");

  {
    // TODO: use GenTl node map for this
    guint32 val;
    size_t datasize;

    /* set AcquisitionMode to Continuous */
    // TODO: "Continuous" value can have different integer values, we need
    // to look it up in the node map (EVT is 0, Basler is 2)
    val = GUINT32_TO_BE (src->producer.acquisition_mode_value);
    datasize = sizeof (val);
    ret =
        GTL_GCWritePort (src->hDevPort, src->producer.acquisition_mode, &val,
        &datasize);
    HANDLE_GTL_ERROR ("Failed to start device acquisition");

    /* send AcquisitionStart command */
    val = GUINT32_TO_BE (1);
    datasize = sizeof (val);
    ret =
        GTL_GCWritePort (src->hDevPort, src->producer.acquisition_start, &val,
        &datasize);
    HANDLE_GTL_ERROR ("Failed to start device acquisition");
  }

  GST_DEBUG_OBJECT (src, "starting acquisition");
//TODO: start acquisition engine

  /* TODO: check timestamps on buffers vs start time */
  src->acq_start_time =
      gst_clock_get_time (gst_element_get_clock (GST_ELEMENT (src)));

  return TRUE;

error:
  if (src->hDS) {
    GTL_DSClose (src->hDS);
    src->hDS = NULL;
  }

  if (src->hDEV) {
    GTL_DevClose (src->hDEV);
    src->hDEV = NULL;
  }

  if (src->hIF) {
    GTL_IFClose (src->hIF);
    src->hIF = NULL;
  }

  gst_gentlsrc_cleanup_tl (src);

  return FALSE;
}

static void
gst_gentlsrc_cleanup_tl (GstGenTlSrc * src)
{
  GstGenTlSrcClass *klass = GST_GENTL_SRC_GET_CLASS (src);
  if (src->hTL) {
    g_mutex_lock (&klass->tl_mutex);
    GST_DEBUG_OBJECT (src, "Framegrabber open with refcount=%d",
        klass->tl_refcount);
    klass->tl_refcount--;
    if (klass->tl_refcount == 0) {
      GST_DEBUG_OBJECT (src, "Framegrabber ref dropped to 0, closing");
      GTL_TLClose (src->hTL);
      src->hTL = NULL;
    }
    g_mutex_unlock (&klass->tl_mutex);
    src->hTL = NULL;

    // don't close library, otherwise we can't reopen in the same process
    //GTL_GCCloseLib ();
  }
}

static gboolean
gst_gentlsrc_stop (GstBaseSrc * bsrc)
{
  GstGenTlSrc *src = GST_GENTL_SRC (bsrc);

  GST_DEBUG_OBJECT (src, "stop");

  if (src->hDS) {
    /* command AcquisitionStop */
    guint32 val = GUINT32_TO_BE (1);
    gsize datasize = sizeof (val);
    GC_ERROR ret =
        GTL_GCWritePort (src->hDevPort, src->producer.acquisition_stop, &val,
        &datasize);

    GTL_DSStopAcquisition (src->hDS, ACQ_STOP_FLAGS_DEFAULT);
    GTL_DSFlushQueue (src->hDS, ACQ_QUEUE_INPUT_TO_OUTPUT);
    GTL_DSFlushQueue (src->hDS, ACQ_QUEUE_OUTPUT_DISCARD);
    GTL_DSClose (src->hDS);
    src->hDS = NULL;
  }

  if (src->hDEV) {
    GTL_DevClose (src->hDEV);
    src->hDEV = NULL;
  }

  if (src->hIF) {
    GTL_IFClose (src->hIF);
    src->hIF = NULL;
  }

  gst_gentlsrc_cleanup_tl (src);

  GST_DEBUG_OBJECT (src, "Closed data stream, device, interface, and library");

  gst_gentlsrc_reset (src);

  return TRUE;
}

static GstCaps *
gst_gentlsrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstGenTlSrc *src = GST_GENTL_SRC (bsrc);
  GstCaps *caps;

  if (src->hDS == NULL) {
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
gst_gentlsrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstGenTlSrc *src = GST_GENTL_SRC (bsrc);
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

static gboolean
gst_gentlsrc_unlock (GstBaseSrc * bsrc)
{
  GstGenTlSrc *src = GST_GENTL_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock");

  src->stop_requested = TRUE;

  return TRUE;
}

static gboolean
gst_gentlsrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstGenTlSrc *src = GST_GENTL_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock_stop");

  src->stop_requested = FALSE;

  return TRUE;
}

static GstStaticCaps unix_reference = GST_STATIC_CAPS ("timestamp/x-unix");

static GstBuffer *
gst_gentlsrc_get_buffer (GstGenTlSrc * src)
{
  GC_ERROR ret;
  EVENT_NEW_BUFFER_DATA new_buffer_data;
  INFO_DATATYPE datatype;
  size_t datasize;
  GstBuffer *buf = NULL;
  size_t payload_type, buffer_size;
  uint64_t frame_id;
  bool8_t buffer_is_incomplete, is_acquiring;
  guint8 *data_ptr;
  GstMapInfo minfo;
  GstClockTime unix_ts;
  uint64_t buf_timestamp_ticks, buf_timestamp_ns;


  /* sometimes we get non-image payloads, try several times for an image */
  for (int i = 0; i < 5; ++i) {
    datasize = sizeof (new_buffer_data);
    ret =
        GTL_EventGetData (src->hNewBufferEvent, &new_buffer_data, &datasize,
        src->timeout);
    HANDLE_GTL_ERROR ("Failed to get New Buffer event within timeout period");

    datasize = sizeof (payload_type);
    ret =
        GTL_DSGetBufferInfo (src->hDS, new_buffer_data.BufferHandle,
        BUFFER_INFO_PAYLOADTYPE, &datatype, &payload_type, &datasize);
    HANDLE_GTL_ERROR ("Failed to get payload type");
    if (payload_type != PAYLOAD_TYPE_IMAGE) {
      GST_WARNING_OBJECT (src, "Non-image payload type, trying again");
      continue;
    } else {
      break;
    }
  }

  if (payload_type != PAYLOAD_TYPE_IMAGE) {
    GST_ELEMENT_ERROR (src, STREAM, TOO_LAZY,
        ("Unsupported payload type: %d", payload_type), (NULL));
    goto error;
  }

  datasize = sizeof (buf_timestamp_ns);
  ret =
      GTL_DSGetBufferInfo (src->hDS, new_buffer_data.BufferHandle,
      BUFFER_INFO_TIMESTAMP_NS, &datatype, &buf_timestamp_ns, &datasize);
  if (ret == GC_ERR_SUCCESS) {
    GST_LOG_OBJECT(src, "Buffer GentTL timestamp: %llu ns", buf_timestamp_ns);
  } else {
    ret =
      GTL_DSGetBufferInfo(src->hDS, new_buffer_data.BufferHandle,
        BUFFER_INFO_TIMESTAMP, &datatype, &buf_timestamp_ticks, &datasize);
    HANDLE_GTL_ERROR("Failed to get buffer timestamp");
    buf_timestamp_ns = (gint64)
      (buf_timestamp_ticks * ((double)GST_SECOND / src->tick_frequency));
    GST_LOG_OBJECT(src, "Buffer GentTL timestamp: %llu ticks, %llu ns", buf_timestamp_ticks, buf_timestamp_ns);
  }

  datasize = sizeof (frame_id);
  ret =
      GTL_DSGetBufferInfo (src->hDS, new_buffer_data.BufferHandle,
      BUFFER_INFO_FRAMEID, &datatype, &frame_id, &datasize);
  HANDLE_GTL_ERROR ("Failed to get frame id");

  datasize = sizeof (buffer_is_incomplete);
  ret =
      GTL_DSGetBufferInfo (src->hDS, new_buffer_data.BufferHandle,
      BUFFER_INFO_IS_INCOMPLETE, &datatype, &buffer_is_incomplete, &datasize);
  HANDLE_GTL_ERROR ("Failed to get complete flag");
  if (buffer_is_incomplete) {
    GST_WARNING_OBJECT (src, "Buffer is incomplete");
  }

  datasize = sizeof (buffer_size);
  ret =
      GTL_DSGetBufferInfo (src->hDS, new_buffer_data.BufferHandle,
      BUFFER_INFO_SIZE, &datatype, &buffer_size, &datasize);
  HANDLE_GTL_ERROR ("Failed to get buffer size");

  datasize = sizeof (data_ptr);
  ret =
      GTL_DSGetBufferInfo (src->hDS, new_buffer_data.BufferHandle,
      BUFFER_INFO_BASE, &datatype, &data_ptr, &datasize);
  HANDLE_GTL_ERROR ("Failed to get buffer pointer");

  // TODO: what if strides aren't same?

  buf = gst_buffer_new_allocate (NULL, buffer_size, NULL);
  if (!buf) {
    GST_ELEMENT_ERROR (src, STREAM, TOO_LAZY,
        ("Failed to allocate buffer"), (NULL));
    goto error;
  }
  // TODO: try to eliminate this memcpy by using gst_buffer_new_wrapped_full
  gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
  orc_memcpy (minfo.data, (void *) data_ptr, minfo.size);
  gst_buffer_unmap (buf, &minfo);

  GTL_DSQueueBuffer (src->hDS, new_buffer_data.BufferHandle);
  HANDLE_GTL_ERROR ("Failed to queue buffer");

  GST_BUFFER_OFFSET (buf) = frame_id;

  if (src->tick_frequency) {
    gint64 nanoseconds_after_latch;

    /* resync system clock and buffer clock periodically */
    if (GST_CLOCK_DIFF (src->unix_latched_ns, get_unix_ns ()) > GST_SECOND) {
      gst_gentlsrc_src_latch_timestamps (src);
    }

    nanoseconds_after_latch = buf_timestamp_ns - src->gentl_latched_ns;
    unix_ts = src->unix_latched_ns + nanoseconds_after_latch;
    GST_LOG_OBJECT (src, "Adding Unix timestamp: %llu", unix_ts);
    gst_buffer_add_reference_timestamp_meta (buf,
        gst_static_caps_get (&unix_reference), unix_ts, GST_CLOCK_TIME_NONE);
  }

  return buf;

error:
  if (buf) {
    gst_buffer_unref (buf);
  }
  return NULL;
}

static GstFlowReturn
gst_gentlsrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstGenTlSrc *src = GST_GENTL_SRC (psrc);
  guint32 dropped_frames = 0;
  GstClock *clock;
  GstClockTime clock_time;

  GST_LOG_OBJECT (src, "create");

  gst_gentlsrc_set_attributes (src);

  *buf = gst_gentlsrc_get_buffer (src);
  if (!*buf) {
    return GST_FLOW_ERROR;
  }

  clock = gst_element_get_clock (GST_ELEMENT (src));
  clock_time = gst_clock_get_time (clock);
  gst_object_unref (clock);

  /* check for dropped frames and disrupted signal */
  //dropped_frames = (circ_handle.FrameCount - src->last_frame_count) - 1;
  if (dropped_frames > 0) {
    src->total_dropped_frames += dropped_frames;
    GST_WARNING_OBJECT (src, "Dropped %d frames (%d total)", dropped_frames,
        src->total_dropped_frames);
  } else if (dropped_frames < 0) {
    GST_WARNING_OBJECT (src, "Frame count non-monotonic, signal disrupted?");
  }
  //src->last_frame_count = circ_handle.FrameCount;

  /* create GstBuffer then release circ buffer back to acquisition */
  //*buf = gst_gentlsrc_create_buffer_from_circ_handle (src, &circ_handle);
  //ret =
  //    BiCirStatusSet (src->board, &src->buffer_array, circ_handle, BIAVAILABLE);
  //if (ret != BI_OK) {
  //  GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
  //      ("Failed to release buffer: %s", gst_gentlsrc_get_error_string (src,
  //              ret)), (NULL));
  //  return GST_FLOW_ERROR;
  //}

  /* TODO: understand why timestamps for circ_handle are sometimes 0 */
  //GST_BUFFER_TIMESTAMP (*buf) =
  //    GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
  //    src->acq_start_time + circ_handle.HiResTimeStamp.totalSec * GST_SECOND);
  GST_BUFFER_TIMESTAMP (*buf) =
      GST_CLOCK_DIFF (gst_element_get_base_time (GST_ELEMENT (src)),
      clock_time);

  if (src->stop_requested) {
    if (*buf != NULL) {
      gst_buffer_unref (*buf);
      *buf = NULL;
    }
    return GST_FLOW_FLUSHING;
  }

  return GST_FLOW_OK;

error:
  return GST_FLOW_ERROR;
}

gchar *
gst_gentlsrc_get_error_string (GstGenTlSrc * src)
{
  size_t error_string_size = MAX_ERROR_STRING_LEN;
  GC_ERROR error_code;
  GTL_GCGetLastError (&error_code, src->error_string, &error_string_size);
  return src->error_string;
}


static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_gentlsrc_debug, "gentlsrc", 0,
      "debug category for gentlsrc element");
  gst_element_register (plugin, "gentlsrc", GST_RANK_NONE,
      gst_gentlsrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gentl,
    "GenTL frame grabber source",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN);
