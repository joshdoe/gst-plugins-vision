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
 * SECTION:element-gstpleorasrc
 *
 * The pleorasrc element is a source for Pleora eBUS
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v pleorasrc ! videoconvert ! autovideosink
 * ]|
 * Shows video from the default Pleora device
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

#include "gstpleorasrc.h"

#include <PvDeviceGEV.h>
#include <PvStreamGEV.h>
#include <PvSystem.h>

GST_DEBUG_CATEGORY_STATIC (gst_pleorasrc_debug);
#define GST_CAT_DEFAULT gst_pleorasrc_debug

/* prototypes */
static void gst_pleorasrc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_pleorasrc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_pleorasrc_dispose (GObject * object);
static void gst_pleorasrc_finalize (GObject * object);

static gboolean gst_pleorasrc_start (GstBaseSrc * src);
static gboolean gst_pleorasrc_stop (GstBaseSrc * src);
static GstCaps *gst_pleorasrc_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_pleorasrc_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_pleorasrc_unlock (GstBaseSrc * src);
static gboolean gst_pleorasrc_unlock_stop (GstBaseSrc * src);

static GstFlowReturn gst_pleorasrc_create (GstPushSrc * src, GstBuffer ** buf);


enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_DEVICE_INDEX,
  PROP_NUM_CAPTURE_BUFFERS,
  PROP_TIMEOUT,
  PROP_DETECTION_TIMEOUT,
  PROP_MULTICAST_GROUP,
  PROP_PORT,
  PROP_RECEIVER_ONLY
};

#define DEFAULT_PROP_DEVICE ""
#define DEFAULT_PROP_DEVICE_INDEX 0
#define DEFAULT_PROP_NUM_CAPTURE_BUFFERS 3
#define DEFAULT_PROP_TIMEOUT 1000
#define DEFAULT_PROP_DETECTION_TIMEOUT 1000
#define DEFAULT_PROP_MULTICAST_GROUP "239.192.1.1"
#define DEFAULT_PROP_PORT 1042
#define DEFAULT_PROP_RECEIVER_ONLY FALSE


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

/* pad templates */
static GstStaticPadTemplate gst_pleorasrc_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS_MAKE_BAYER16 ("{ bggr16, grbg16, rggb16, gbrg16 }") ";"
        VIDEO_CAPS_MAKE_BAYER8 ("{ bggr, grbg, rggb, gbrg }") ";"
        GST_VIDEO_CAPS_MAKE ("{ GRAY16_LE, GRAY16_BE, GRAY8, UYVY, YUY2, RGB }")
    )
    );

/* class initialization */

G_DEFINE_TYPE (GstPleoraSrc, gst_pleorasrc, GST_TYPE_PUSH_SRC);

static void
gst_pleorasrc_class_init (GstPleoraSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_pleorasrc_set_property;
  gobject_class->get_property = gst_pleorasrc_get_property;
  gobject_class->dispose = gst_pleorasrc_dispose;
  gobject_class->finalize = gst_pleorasrc_finalize;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_pleorasrc_src_template));

  gst_element_class_set_static_metadata (gstelement_class,
      "Pleora Video Source", "Source/Video",
      "Pleora eBUS video source", "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_pleorasrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_pleorasrc_stop);
  gstbasesrc_class->get_caps = GST_DEBUG_FUNCPTR (gst_pleorasrc_get_caps);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_pleorasrc_set_caps);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_pleorasrc_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_pleorasrc_unlock_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_pleorasrc_create);

  /* Install GObject properties */
  g_object_class_install_property (gobject_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device ID",
          "Device ID. For GEV use MAC, IP, or user id. For U3V, use GUID or user id.",
          DEFAULT_PROP_DEVICE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_DEVICE_INDEX,
      g_param_spec_int ("device-index", "Device index",
          "Index of device, use -1 to enumerate all and select last", -1,
          G_MAXINT, DEFAULT_PROP_DEVICE_INDEX,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_NUM_CAPTURE_BUFFERS,
      g_param_spec_uint ("num-capture-buffers", "Number of capture buffers",
          "Number of capture buffers", 1, G_MAXUINT,
          DEFAULT_PROP_NUM_CAPTURE_BUFFERS,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_TIMEOUT,
      g_param_spec_int ("timeout", "Timeout (ms)",
          "Timeout in ms (0 to use default)", 0, G_MAXINT, DEFAULT_PROP_TIMEOUT,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DETECTION_TIMEOUT,
      g_param_spec_int ("detection-timeout", "Detection Timeout (ms)",
          "Timeout in ms to detect GigE cameras", 100,
          60000, DEFAULT_PROP_DETECTION_TIMEOUT,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (gobject_class, PROP_MULTICAST_GROUP,
      g_param_spec_string ("multicast-group", "Multicast group IP address",
          "The address of the multicast group to join.",
          DEFAULT_PROP_MULTICAST_GROUP,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PORT,
      g_param_spec_int ("port", "Multicast port",
          "The port of the multicast group.", 0,
          65535, DEFAULT_PROP_PORT,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_RECEIVER_ONLY,
      g_param_spec_boolean ("receiver-only", "Receiver only",
          "Only open video stream, don't open as controller",
          DEFAULT_PROP_RECEIVER_ONLY,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
}

static void
gst_pleorasrc_reset (GstPleoraSrc * src)
{
  src->device = NULL;
  src->stream = NULL;
  src->pipeline = NULL;

  src->last_frame_count = 0;
  src->total_dropped_frames = 0;

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }
}

static void
gst_pleorasrc_init (GstPleoraSrc * src)
{
  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  /* initialize member variables */
  src->device_id = g_strdup (DEFAULT_PROP_DEVICE);
  src->device_index = DEFAULT_PROP_DEVICE_INDEX;
  src->num_capture_buffers = DEFAULT_PROP_NUM_CAPTURE_BUFFERS;
  src->timeout = DEFAULT_PROP_TIMEOUT;
  src->detection_timeout = DEFAULT_PROP_DETECTION_TIMEOUT;
  src->multicast_group= g_strdup (DEFAULT_PROP_MULTICAST_GROUP);
  src->port = DEFAULT_PROP_PORT;
  src->receiver_only = DEFAULT_PROP_RECEIVER_ONLY;

  src->stop_requested = FALSE;
  src->caps = NULL;

  gst_pleorasrc_reset (src);
}

void
gst_pleorasrc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPleoraSrc *src;

  src = GST_PLEORA_SRC (object);

  switch (property_id) {
    case PROP_DEVICE:
      g_free (src->device_id);
      src->device_id = g_strdup (g_value_get_string (value));
      break;
    case PROP_DEVICE_INDEX:
      src->device_index = g_value_get_int (value);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      src->num_capture_buffers = g_value_get_uint (value);
      break;
    case PROP_TIMEOUT:
      src->timeout = g_value_get_int (value);
      break;
    case PROP_DETECTION_TIMEOUT:
      src->detection_timeout = g_value_get_int (value);
      break;
    case PROP_MULTICAST_GROUP:
      g_free (src->multicast_group);
      src->multicast_group = g_strdup (g_value_get_string (value));
      break;
    case PROP_PORT:
      src->port = g_value_get_int (value);
      break;
    case PROP_RECEIVER_ONLY:
      src->receiver_only = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_pleorasrc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstPleoraSrc *src;

  g_return_if_fail (GST_IS_PLEORA_SRC (object));
  src = GST_PLEORA_SRC (object);

  switch (property_id) {
    case PROP_DEVICE:
      g_value_set_string (value, src->device_id);
      break;
    case PROP_DEVICE_INDEX:
      g_value_set_int (value, src->device_index);
      break;
    case PROP_NUM_CAPTURE_BUFFERS:
      g_value_set_uint (value, src->num_capture_buffers);
      break;
    case PROP_TIMEOUT:
      g_value_set_int (value, src->timeout);
      break;
    case PROP_DETECTION_TIMEOUT:
      g_value_set_int (value, src->detection_timeout);
      break;
    case PROP_MULTICAST_GROUP:
      g_value_set_string (value, src->multicast_group);
      break;
    case PROP_PORT:
      g_value_set_int (value, src->port);
      break;
    case PROP_RECEIVER_ONLY:
      g_value_set_boolean (value, src->receiver_only);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_pleorasrc_dispose (GObject * object)
{
  GstPleoraSrc *src;

  g_return_if_fail (GST_IS_PLEORA_SRC (object));
  src = GST_PLEORA_SRC (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_pleorasrc_parent_class)->dispose (object);
}

void
gst_pleorasrc_finalize (GObject * object)
{
  GstPleoraSrc *src;

  g_return_if_fail (GST_IS_PLEORA_SRC (object));
  src = GST_PLEORA_SRC (object);

  /* clean up object here */
  if (src->device) {
    g_free (src->device);
    src->device = NULL;
  }

  if (src->caps) {
    gst_caps_unref (src->caps);
    src->caps = NULL;
  }

  G_OBJECT_CLASS (gst_pleorasrc_parent_class)->finalize (object);
}

static const gchar *
usb_speed_str (PvUSBSpeed speed)
{
  static const gchar *array[] = {
    "Unknown", "Low", "Full", "High", "Super"
  };
  if (speed >= sizeof (array) / sizeof (const gchar *) || speed < 0) {
    speed = PvUSBSpeedUnknown;
  }
  return array[speed];
}

static const void
gst_pleorasrc_print_device_info (GstPleoraSrc * src,
    const PvDeviceInfo * device_info)
{
  GST_DEBUG_OBJECT (src, "Found device '%s'",
      device_info->GetDisplayID ().GetAscii ());

  const PvDeviceInfoGEV *device_info_GEV =
      dynamic_cast < const PvDeviceInfoGEV * >(device_info);
  const PvDeviceInfoU3V *device_info_U3V =
      dynamic_cast < const PvDeviceInfoU3V * >(device_info);
  const PvDeviceInfoUSB *device_info_USB =
      dynamic_cast < const PvDeviceInfoUSB * >(device_info);
  const PvDeviceInfoPleoraProtocol *device_info_pleora =
      dynamic_cast < const PvDeviceInfoPleoraProtocol * >(device_info);

  const PvNetworkAdapter *iface_nic =
      dynamic_cast < const PvNetworkAdapter * >(device_info->GetInterface ());
  const PvUSBHostController *iface_usb =
      dynamic_cast <
      const PvUSBHostController * >(device_info->GetInterface ());

  if (iface_nic != NULL) {
    GST_DEBUG_OBJECT (src,
        "Device found on network interface '%s', MAC: %s, IP: %s, Subnet: %s",
        iface_nic->GetDescription ().GetAscii (),
        iface_nic->GetMACAddress ().GetAscii (),
        iface_nic->GetIPAddress ().GetAscii (),
        iface_nic->GetSubnetMask ().GetAscii ());
  } else if (iface_usb != NULL) {
    GST_DEBUG_OBJECT (src,
        "Device found on USB interface, VEN_%04X&DEV_%04X&SUBSYS_%08X&REV_%02X, '%s', %s Speed",
        iface_usb->GetVendorID (), iface_usb->GetDeviceID (),
        iface_usb->GetSubsystemID (), iface_usb->GetRevision (),
        iface_usb->GetName ().GetAscii (),
        usb_speed_str (iface_usb->GetSpeed ()));
  }

  if (device_info_GEV != NULL) {
    GST_DEBUG_OBJECT (src, "GEV device: MAC: %s, IP: %s, S/N: %s",
        device_info_GEV->GetMACAddress ().GetAscii (),
        device_info_GEV->GetIPAddress ().GetAscii (),
        device_info_GEV->GetSerialNumber ().GetAscii ());
  } else if (device_info_U3V != NULL) {
    GST_DEBUG_OBJECT (src, "U3V device: GUID: %s, S/N: %s",
        device_info_U3V->GetDeviceGUID ().GetAscii (),
        device_info_U3V->GetSerialNumber ().GetAscii (),
        device_info_U3V->GetInterface ());
  } else if (device_info_USB != NULL) {
    GST_DEBUG_OBJECT (src, "Unidentified USB device");
  } else if (device_info_pleora != NULL) {
    GST_DEBUG_OBJECT (src, "Pleora device: MAC: %s, IP: %s, S/N: %s",
        device_info_pleora->GetMACAddress ().GetAscii (),
        device_info_pleora->GetIPAddress ().GetAscii (),
        device_info_pleora->GetSerialNumber ().GetAscii ());
  }
}

static gboolean
gst_pleorasrc_setup_device (GstPleoraSrc * src)
{
  PvResult pvRes;
  static const PvDeviceInfo *device_info = NULL;

  PvSystem lSystem;

  // time allowed to detect GEV cameras
  lSystem.SetDetectionTimeout (src->detection_timeout);

  if (g_strcmp0 (src->device_id, "") != 0) {
    GST_DEBUG_OBJECT (src, "Finding device based on ID: %s", src->device_id);

    pvRes = lSystem.FindDevice (src->device_id, &device_info);

    if (!pvRes.IsOK ()) {
      GST_WARNING_OBJECT (src, "Failed to find device '%s': %s", src->device_id,
          pvRes.GetCodeString ().GetAscii ());
      return NULL;
    }

    gst_pleorasrc_print_device_info (src, device_info);
  } else if (src->device_index >= 0) {
    GST_DEBUG_OBJECT (src, "Finding device based on index: %d",
        src->device_index);

    pvRes = lSystem.Find ();

    if (!pvRes.IsOK ()) {
      GST_WARNING_OBJECT (src, "Error finding devices: %s",
          pvRes.GetCodeString ().GetAscii ());
      return NULL;
    }

    device_info = lSystem.GetDeviceInfo (src->device_index);

    if (device_info == NULL) {
      GST_WARNING_OBJECT (src, "Failed to find device index %d",
          src->device_index);
      return NULL;
    }

    gst_pleorasrc_print_device_info (src, device_info);
  } else {
    guint32 device_count;

    GST_DEBUG_OBJECT (src, "Enumerating devices and choosing last one");

    pvRes = lSystem.Find ();

    if (!pvRes.IsOK ()) {
      GST_WARNING_OBJECT (src, "Error finding devices: %s",
          pvRes.GetCodeString ().GetAscii ());
      return NULL;
    }

    device_count = lSystem.GetDeviceCount ();

    GST_DEBUG_OBJECT (src, "Found a total of %d device(s)", device_count);
    for (uint32_t x = 0; x < device_count; x++) {
      device_info = lSystem.GetDeviceInfo (x);
      gst_pleorasrc_print_device_info (src, device_info);
    }

#if 0
    // iterate through all interfaces on system
    uint32_t lInterfaceCount = lSystem.GetInterfaceCount ();
    for (uint32_t x = 0; x < lInterfaceCount; x++) {
      const PvInterface *lInterface = lSystem.GetInterface (x);

      GST_DEBUG_OBJECT (src, "Found interface %d: '%s', '%s'", x,
          lInterface->GetName ().GetAscii (),
          lInterface->GetDisplayID ().GetAscii ());

      const PvNetworkAdapter *lNIC =
          dynamic_cast < const PvNetworkAdapter * >(lInterface);
      if (lNIC != NULL) {
        GST_DEBUG_OBJECT (src, "MAC: %s, IP: %s, Subnet: %s",
            lNIC->GetMACAddress ().GetAscii (),
            lNIC->GetIPAddress ().GetAscii (),
            lNIC->GetSubnetMask ().GetAscii ());
      }

      const PvUSBHostController *lUSB =
          dynamic_cast < const PvUSBHostController * >(lInterface);
      if (lUSB != NULL) {
        GST_DEBUG_OBJECT (src, "USB '%s'", lUSB->GetName ().GetAscii ());
      }
      // iterate through all devices on interface
      uint32_t lDeviceCount = lInterface->GetDeviceCount ();

      if (lDeviceCount == 0) {
        GST_DEBUG_OBJECT (src, "No devices found on this interface");
      }
      for (uint32_t y = 0; y < lDeviceCount; y++) {
        const PvDeviceInfo *devinfo = lInterface->GetDeviceInfo (y);

        gst_pleorasrc_print_device_info (src, devinfo);

        // select last device
        device_info = devinfo;
      }
    }
#endif
  }

  if (device_info == NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND, ("No device found"), (NULL));
    return FALSE;
  }

  /* open as controller by connecting to device */
  if (!src->receiver_only) {
      GST_DEBUG_OBJECT (src, "Trying to connect to device '%s'",
          device_info->GetDisplayID ().GetAscii ());

      src->device = PvDevice::CreateAndConnect (device_info, &pvRes);
      if (src->device == NULL) {
          GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
              ("Unable to create and connect to device"), (NULL));
          return FALSE;
      }
      GST_DEBUG_OBJECT (src, "Connected to device");
  }

  if (device_info->GetType() == PvDeviceInfoTypeGEV ||
      device_info->GetType() == PvDeviceInfoTypePleoraProtocol) {
      GST_DEBUG_OBJECT (src, "Opening multicast stream");
      PvStreamGEV *stream = new PvStreamGEV;
      // FIXME: need to add prop for enabling multicast
      stream->Open(device_info->GetConnectionID(), src->multicast_group, src->port);
      src->stream = stream;
  } else {
      src->stream =
          PvStream::CreateAndOpen (device_info->GetConnectionID (), &pvRes);
  }

  if (src->stream == NULL) {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
          ("Unable to create and connect to device"), (NULL));
      return FALSE;
  }
  GST_DEBUG_OBJECT (src, "Stream created for device");

  /* if acting as a GigE controller configure stream */
  PvDeviceGEV *lDeviceGEV = dynamic_cast < PvDeviceGEV * >(src->device);
  if (!src->receiver_only && lDeviceGEV != NULL) {
    PvStreamGEV *lStreamGEV = static_cast < PvStreamGEV * >(src->stream);

    // Negotiate packet size
    lDeviceGEV->NegotiatePacketSize ();

    // Configure device streaming destination
    lDeviceGEV->SetStreamDestination (lStreamGEV->GetLocalIPAddress (),
        lStreamGEV->GetLocalPort ());
  }

  src->pipeline = new PvPipeline (src->stream);
  if (src->pipeline == NULL) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Unable to create pipeline"), (NULL));
    return FALSE;
  }

  src->pipeline->SetBufferCount (src->num_capture_buffers);

  return TRUE;
}

// borrowed from Aravis, arvmisc.c
#define MAKE_FOURCC(a,b,c,d)        ((guint32)((a)|(b)<<8|(c)<<16|(d)<<24))

typedef struct
{
  PvPixelType pixel_type;
  const char *gst_caps_string;
  const char *name;
  const char *format;
  int bpp;
  int depth;
  guint32 fourcc;
} GstPleoraCapsInfos;

GstPleoraCapsInfos gst_caps_infos[] = {
  {
        PvPixelMono8,
        "video/x-raw, format=(string)GRAY8",
        "video/x-raw", "GRAY8",
      8, 8, 0},
  {
        PvPixelMono10,
        "video/x-raw, format=(string)GRAY16_LE, bpp=(int)10",
        "video/x-raw", "GRAY16_LE",
      10, 16, 0},
  {
        PvPixelMono12,
        "video/x-raw, format=(string)GRAY16_LE, bpp=(int)12",
        "video/x-raw", "GRAY16_LE",
      12, 16, 0},
  {
        PvPixelMono14,
        "video/x-raw, format=(string)GRAY16_LE, bpp=(int)14",
        "video/x-raw", "GRAY16_LE",
      14, 16, 0},
  {
        PvPixelMono16,
        "video/x-raw, format=(string)GRAY16_LE",
        "video/x-raw", "GRAY16_LE",
      16, 16, 0},
  {
        PvPixelBayerGR8,
        "video/x-bayer, format=(string)grbg",
        "video/x-bayer", "grbg",
        8, 8, MAKE_FOURCC ('g', 'r', 'b', 'g')
      },
  {
        PvPixelBayerRG8,
        "video/x-bayer, format=(string)rggb",
        "video/x-bayer", "rggb",
        8, 8, MAKE_FOURCC ('r', 'g', 'g', 'b')
      },
  {
        PvPixelBayerGB8,
        "video/x-bayer, format=(string)gbrg",
        "video/x-bayer", "gbrg",
        8, 8, MAKE_FOURCC ('g', 'b', 'r', 'g')
      },
  {
        PvPixelBayerBG8,
        "video/x-bayer, format=(string)bggr",
        "video/x-bayer", "bggr",
        8, 8, MAKE_FOURCC ('b', 'g', 'g', 'r')
      },

  /* The caps for non 8-bit bayer formats has not been agreed upon yet.
   * This feature is discussed in bug https://bugzilla.gnome.org/show_bug.cgi?id=693666 .*/
  {
        PvPixelBayerGR10,
        "video/x-bayer, format=(string)grbg16, bpp=(int)10",
        "video/x-bayer", "grbg",
      10, 16, 0},
  {
        PvPixelBayerRG10,
        "video/x-bayer, format=(string)rggb16, bpp=(int)10",
        "video/x-bayer", "rggb",
      10, 16, 0},
  {
        PvPixelBayerGB10,
        "video/x-bayer, format=(string)gbrg16, bpp=(int)10",
        "video/x-bayer", "gbrg",
      10, 16, 0},
  {
        PvPixelBayerBG10,
        "video/x-bayer, format=(string)bggr16, bpp=(int)10",
        "video/x-bayer", "bggr",
      10, 16, 0},
  {
        PvPixelBayerGR12,
        "video/x-bayer, format=(string)grbg16, bpp=(int)12",
        "video/x-bayer", "grbg",
      12, 16, 0},
  {
        PvPixelBayerRG12,
        "video/x-bayer, format=(string)rggb16, bpp=(int)12",
        "video/x-bayer", "rggb",
      12, 16, 0},
  {
        PvPixelBayerGB12,
        "video/x-bayer, format=(string)gbrg16, bpp=(int)12",
        "video/x-bayer", "gbrg",
      12, 16, 0},
  {
        PvPixelBayerBG12,
        "video/x-bayer, format=(string)bggr16, bpp=(int)12",
        "video/x-bayer", "bggr",
      12, 16, 0},
  {
        PvPixelBayerGR16,
        "video/x-bayer, format=(string)grbg16, bpp=(int)16",
        "video/x-bayer", "grbg",
      16, 16, 0},
  {
        PvPixelBayerRG16,
        "video/x-bayer, format=(string)rggb16, bpp=(int)16",
        "video/x-bayer", "rggb",
      16, 16, 0},
  {
        PvPixelBayerGB16,
        "video/x-bayer, format=(string)gbrg16, bpp=(int)16",
        "video/x-bayer", "gbrg",
      16, 16, 0},
  {
        PvPixelBayerBG16,
        "video/x-bayer, format=(string)bggr16, bpp=(int)16",
        "video/x-bayer", "bggr",
      16, 16, 0},

  {
        PvPixelYUV422_8_UYVY,
        "video/x-raw, format=(string)UYVY",
        "video/x-raw", "UYVY",
        0, 0, MAKE_FOURCC ('U', 'Y', 'V', 'Y')
      },
  {
        PvPixelYUV422_8,
        "video/x-raw, format=(string)YUY2",
        "video/x-raw", "YUY2",
        0, 0, MAKE_FOURCC ('Y', 'U', 'Y', '2')
      },
  {
        PvPixelRGB8,
        "video/x-raw, format=(string)RGB",
        "video/x-raw", "RGB",
      24, 24, 0},
};

/**
 * arv_pixel_format_to_gst_caps_string:
 * @pixel_format: a pixel format
 * Return value: a gstreamer caps string describing the given @pixel_format.
 */

const char *
gst_pleorasrc_pixel_type_to_gst_caps_string (PvPixelType pixel_type)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (gst_caps_infos); i++)
    if (gst_caps_infos[i].pixel_type == pixel_type)
      break;

  if (i == G_N_ELEMENTS (gst_caps_infos)) {
    GST_WARNING ("Pixel type not currently supported: %d", pixel_type);
    return NULL;
  }

  GST_LOG ("Matched pixel type %d to caps %s",
      pixel_type, gst_caps_infos[i].gst_caps_string);

  return gst_caps_infos[i].gst_caps_string;
}

PvPixelType
gst_pleorasrc_pixel_type_from_gst_caps (const char *name,
    const char *format, int bpp, int depth)
{
  unsigned int i;

  g_return_val_if_fail (name != NULL, PvPixelUndefined);

  for (i = 0; i < G_N_ELEMENTS (gst_caps_infos); i++) {
    if (strcmp (name, gst_caps_infos[i].name) != 0 ||
        (depth > 0 && depth != gst_caps_infos[i].depth) ||
        (bpp > 0 && bpp != gst_caps_infos[i].bpp))
      continue;

    if (strcmp (name, "video/x-raw") == 0 &&
        strcmp (format, gst_caps_infos[i].format) == 0)
      return gst_caps_infos[i].pixel_type;

    if (strcmp (name, "video/x-bayer") == 0 &&
        strcmp (format, gst_caps_infos[i].format) == 0)
      return gst_caps_infos[i].pixel_type;
  }

  return PvPixelUndefined;
}

static gboolean
gst_pleorasrc_start (GstBaseSrc * bsrc)
{
  GstPleoraSrc *src = GST_PLEORA_SRC (bsrc);
  guint32 width, height, bpp, stride;
  GstVideoInfo vinfo;
  PvResult pvRes;

  GST_DEBUG_OBJECT (src, "start");

  if (!gst_pleorasrc_setup_device (src)) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
        ("Failed to create Pleora pipeline"), (NULL));
    goto error;
  }

  /* Note: the pipeline must be initialized before we start acquisition */
  GST_DEBUG_OBJECT (src, "Starting pipeline");
  pvRes = src->pipeline->Start ();
  if (!pvRes.IsOK ()) {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Failed to start pipeline"),
        (NULL));
    goto error;
  }

  /* command stream to start */
  if (!src->receiver_only) {
      PvGenParameterArray *lDeviceParams = src->device->GetParameters ();
      PvGenCommand *start_cmd =
          dynamic_cast < PvGenCommand * >(lDeviceParams->Get ("AcquisitionStart"));

      if (start_cmd == NULL) {
          GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
              ("Failed to get device AcquisitionStart parameter"), (NULL));
          goto error;
      }
      pvRes = src->device->StreamEnable ();
      if (!pvRes.IsOK ()) {
          GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Failed to enable stream"),
              (NULL));
          goto error;
      }

      pvRes = start_cmd->Execute ();
      if (!pvRes.IsOK ()) {
          GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Failed to start acquisition"),
              (NULL));
          goto error;
      }
  }

  return TRUE;

error:
  if (src->pipeline) {
    delete src->pipeline;
    src->pipeline = NULL;
  }

  if (src->stream) {
    src->stream->Close ();
    PvStream::Free (src->stream);
    src->stream = NULL;
  }

  if (src->device) {
    src->device->Disconnect ();
    PvDevice::Free (src->device);
    src->device = NULL;
  }

  return FALSE;
}

static gboolean
gst_pleorasrc_stop (GstBaseSrc * bsrc)
{
  GstPleoraSrc *src = GST_PLEORA_SRC (bsrc);
  GST_DEBUG_OBJECT (src, "stop");

  if (!src->receiver_only) {
      PvGenParameterArray *lDeviceParams = src->device->GetParameters ();
      PvGenCommand *lStop =
          dynamic_cast < PvGenCommand * >(lDeviceParams->Get ("AcquisitionStop"));
      lStop->Execute ();
      src->device->StreamDisable ();
  }
  src->pipeline->Stop ();

  if (src->pipeline) {
    delete src->pipeline;
    src->pipeline = NULL;
  }

  if (src->stream) {
    src->stream->Close ();
    PvStream::Free (src->stream);
    src->stream = NULL;
  }

  if (src->device) {
    src->device->Disconnect ();
    PvDevice::Free (src->device);
    src->device = NULL;
  }

  gst_pleorasrc_reset (src);

  return TRUE;
}

static GstCaps *
gst_pleorasrc_get_caps (GstBaseSrc * bsrc, GstCaps * filter)
{
  GstPleoraSrc *src = GST_PLEORA_SRC (bsrc);
  GstCaps *caps;

  if (src->caps == NULL) {
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
gst_pleorasrc_set_caps (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstPleoraSrc *src = GST_PLEORA_SRC (bsrc);
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
gst_pleorasrc_unlock (GstBaseSrc * bsrc)
{
  GstPleoraSrc *src = GST_PLEORA_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock");

  src->stop_requested = TRUE;

  return TRUE;
}

static gboolean
gst_pleorasrc_unlock_stop (GstBaseSrc * bsrc)
{
  GstPleoraSrc *src = GST_PLEORA_SRC (bsrc);

  GST_LOG_OBJECT (src, "unlock_stop");

  src->stop_requested = FALSE;

  return TRUE;
}

//static GstBuffer *
//gst_pleorasrc_create_buffer_from_pvimage (GstPleoraSrc * src,
//    PvImage * pvimage)
//{
//  GstMapInfo minfo;
//  GstBuffer *buf;
//
//  /* TODO: use allocator or use from pool */
//  buf = gst_buffer_new_and_alloc (src->height * src->gst_stride);
//
//  /* Copy image to buffer from surface */
//  gst_buffer_map (buf, &minfo, GST_MAP_WRITE);
//  GST_LOG_OBJECT (src,
//      "GstBuffer size=%d, gst_stride=%d, buffer_num=%d, frame_count=%d, num_frames_on_queue=%d",
//      minfo.size, src->gst_stride, circ_handle->BufferNumber,
//      circ_handle->FrameCount, circ_handle->NumItemsOnQueue);
//  GST_LOG_OBJECT (src, "Buffer timestamp %02d:%02d:%02d.%06d",
//      circ_handle->HiResTimeStamp.hour, circ_handle->HiResTimeStamp.min,
//      circ_handle->HiResTimeStamp.sec, circ_handle->HiResTimeStamp.usec);
//
//  /* TODO: use orc_memcpy */
//  if (src->gst_stride == src->bf_stride) {
//    memcpy (minfo.data, ((guint8 *) circ_handle->pBufData), minfo.size);
//  } else {
//    int i;
//    GST_LOG_OBJECT (src, "Image strides not identical, copy will be slower.");
//    for (i = 0; i < src->height; i++) {
//      memcpy (minfo.data + i * src->gst_stride,
//          ((guint8 *) circ_handle->pBufData) +
//          i * src->bf_stride, src->bf_stride);
//    }
//  }
//  gst_buffer_unmap (buf, &minfo);
//
//  return buf;
//}

typedef struct
{
  PvPipeline *pipeline;
  PvBuffer *buffer;
} VideoFrame;

static void
pvbuffer_release (void *data)
{
  VideoFrame *frame = (VideoFrame *) data;
  frame->pipeline->ReleaseBuffer (frame->buffer);
}

static GstFlowReturn
gst_pleorasrc_create (GstPushSrc * psrc, GstBuffer ** buf)
{
  GstPleoraSrc *src = GST_PLEORA_SRC (psrc);
  PvResult pvRes, opRes;
  guint32 dropped_frames;
  GstClock *clock;
  GstClockTime clock_time;
  PvBuffer *pvbuffer;
  PvImage *pvimage;

  GST_LOG_OBJECT (src, "create");

  while (TRUE) {
      pvRes = src->pipeline->RetrieveNextBuffer (&pvbuffer, src->timeout, &opRes);
      if (!pvRes.IsOK()) {
          GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Failed to retrieve buffer in timeout (%d ms): 0x%04x, '%s'", src->timeout, pvRes.GetCode(), pvRes.GetDescription().GetAscii()), (NULL));
          return GST_FLOW_ERROR;
      }

      // continue if we get a bad frame
      if (!opRes.IsOK()) {
          GST_WARNING_OBJECT(src, "Failed to get buffer: 0x%04x, '%s'", opRes.GetCode(), opRes.GetCodeString().GetAscii());
          src->pipeline->ReleaseBuffer(pvbuffer);
          continue;
      }

      if (pvbuffer->GetPayloadType () != PvPayloadTypeImage) {
        /* TODO: are non-image buffers normal? */
        GST_ERROR_OBJECT (src, "Got buffer with non-image data");
        GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
            ("Got buffer with non-image data"), (NULL));
        return GST_FLOW_ERROR;
      }

      pvimage = pvbuffer->GetImage ();

      break;
  }

    const char *caps_string =
          gst_pleorasrc_pixel_type_to_gst_caps_string (pvimage->GetPixelType ());

  /* TODO: cache previous caps_string */
  if (caps_string != NULL) {
    GstStructure *structure;
    GstCaps *caps;

    caps = gst_caps_new_empty ();
    structure = gst_structure_from_string (caps_string, NULL);
    gst_structure_set (structure,
        "width", G_TYPE_INT, pvimage->GetWidth (),
        "height", G_TYPE_INT, pvimage->GetHeight (),
        "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
    gst_caps_append_structure (caps, structure);

    if (src->caps) {
      gst_caps_unref (src->caps);
    }
    src->caps = caps;
    gst_base_src_set_caps (GST_BASE_SRC (src), src->caps);
  } else {
    GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Pixel type not supported"),
        (NULL));
    return GST_FLOW_ERROR;
  }

  VideoFrame *vf = g_new0 (VideoFrame, 1);
  vf->pipeline = src->pipeline;
  vf->buffer = pvbuffer;
  gpointer data = pvimage->GetDataPointer ();
  gsize data_size = pvimage->GetImageSize ();
  *buf =
      gst_buffer_new_wrapped_full ((GstMemoryFlags) GST_MEMORY_FLAG_READONLY,
      (gpointer) data, data_size, 0, data_size, vf,
      (GDestroyNotify) pvbuffer_release);

  clock = gst_element_get_clock (GST_ELEMENT (src));
  clock_time = gst_clock_get_time (clock);
  gst_object_unref (clock);

  /* check for dropped frames and disrupted signal */
  //dropped_frames = (circ_handle.FrameCount - src->last_frame_count) - 1;
  //if (dropped_frames > 0) {
  //  src->total_dropped_frames += dropped_frames;
  //  GST_WARNING_OBJECT (src, "Dropped %d frames (%d total)", dropped_frames,
  //      src->total_dropped_frames);
  //} else if (dropped_frames < 0) {
  //  GST_WARNING_OBJECT (src, "Frame count non-monotonic, signal disrupted?");
  //}
  //src->last_frame_count = circ_handle.FrameCount;

  /* create GstBuffer then release circ buffer back to acquisition */
  //*buf = gst_pleorasrc_create_buffer_from_circ_handle (src, &circ_handle);
  //ret =
  //    BiCirStatusSet (src->board, &src->buffer_array, circ_handle, BIAVAILABLE);
  //if (ret != BI_OK) {
  //  GST_ELEMENT_ERROR (src, RESOURCE, FAILED,
  //      ("Failed to release buffer: %s", gst_pleorasrc_get_error_string (src,
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
  //GST_BUFFER_OFFSET (*buf) = circ_handle.FrameCount - 1;

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
  GST_DEBUG_CATEGORY_INIT (gst_pleorasrc_debug, "pleorasrc", 0,
      "debug category for pleorasrc element");
  gst_element_register (plugin, "pleorasrc", GST_RANK_NONE,
      gst_pleorasrc_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    pleora,
    "Pleora eBUS video source",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
