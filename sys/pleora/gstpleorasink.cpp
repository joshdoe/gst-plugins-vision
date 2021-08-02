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
 * SECTION:element-gstpleorasink
 *
 * The pleorasink element is a sink for Pleora eBUS SDK to output a GigE Vision video stream.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v videotestsrc ! pleorasink
 * ]|
 * Outputs test pattern using Pleora eBUS SDK GigE Vision Tx.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <PvSampleUtils.h>
#include <PvSoftDeviceGEV.h>
#include <PvBuffer.h>
#include <PvSampleTransmitterConfig.h>

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstpleorasink.h"
#include "streamingchannelsource.h"


/* GObject prototypes */
static void gst_pleorasink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_pleorasink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_pleorasink_dispose (GObject * object);

/* GstBaseSink prototypes */
static gboolean gst_pleorasink_start (GstBaseSink * basesink);
static gboolean gst_pleorasink_stop (GstBaseSink * basesink);
static GstCaps *gst_pleorasink_get_caps (GstBaseSink * basesink,
    GstCaps * filter_caps);
static gboolean gst_pleorasink_set_caps (GstBaseSink * basesink,
    GstCaps * caps);
static GstFlowReturn gst_pleorasink_render (GstBaseSink * basesink,
    GstBuffer * buffer);
static gboolean gst_pleorasink_unlock (GstBaseSink * basesink);
static gboolean gst_pleorasink_unlock_stop (GstBaseSink * basesink);

gboolean gst_pleorasink_start_multicasting (GstPleoraSink * sink);
void gst_pleorasink_stop_multicasting (GstPleoraSink * sink);

enum
{
  PROP_0,
  PROP_NUM_INTERNAL_BUFFERS,
  PROP_ADDRESS,
  PROP_MANUFACTURER,
  PROP_MODEL,
  PROP_VERSION,
  PROP_INFO,
  PROP_SERIAL,
  PROP_MAC,
  PROP_OUTPUT_KLV,
  PROP_AUTO_MULTICAST,
  PROP_MULTICAST_GROUP,
  PROP_MULTICAST_PORT,
  PROP_PACKET_SIZE
};

#define DEFAULT_PROP_NUM_INTERNAL_BUFFERS 3
#define DEFAULT_PROP_ADDRESS      ""
#define DEFAULT_PROP_MANUFACTURER "Pleora"
#define DEFAULT_PROP_MODEL        "eBUS GStreamer"
#define DEFAULT_PROP_VERSION      "0.1"
#define DEFAULT_PROP_INFO         "Pleora eBUS GStreamer Sink"
#define DEFAULT_PROP_SERIAL       "0001"
#define DEFAULT_PROP_MAC          ""
#define DEFAULT_PROP_OUTPUT_KLV   FALSE
#define DEFAULT_PROP_AUTO_MULTICAST FALSE
#define DEFAULT_PROP_MULTICAST_GROUP "239.192.1.1"
#define DEFAULT_PROP_MULTICAST_PORT 1042
#define DEFAULT_PROP_PACKET_SIZE  1492

/* pad templates */

static GstStaticPadTemplate gst_pleorasink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
        ("{ GRAY8, GRAY16_LE, RGB, RGBA, BGR, BGRA }"))
    );

/* class initialization */

/* setup debug */
GST_DEBUG_CATEGORY (pleorasink_debug);
#define GST_CAT_DEFAULT pleorasink_debug

G_DEFINE_TYPE (GstPleoraSink, gst_pleorasink, GST_TYPE_BASE_SINK);

static void
gst_pleorasink_class_init (GstPleoraSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "pleorasink", 0,
      "Pleora eBUS SDK sink");

  gobject_class->set_property = gst_pleorasink_set_property;
  gobject_class->get_property = gst_pleorasink_get_property;
  gobject_class->dispose = gst_pleorasink_dispose;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_pleorasink_sink_template));

  gst_element_class_set_details_simple (gstelement_class,
      "Pleora eBUS GEV Tx Sink", "Sink/Video",
      "Pleora eBUS SDK sink to output GigE Vision video",
      "Joshua M. Doe <oss@nvl.army.mil>");

  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_pleorasink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_pleorasink_stop);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_pleorasink_set_caps);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_pleorasink_render);
  gstbasesink_class->unlock = GST_DEBUG_FUNCPTR (gst_pleorasink_unlock);
  gstbasesink_class->unlock_stop =
      GST_DEBUG_FUNCPTR (gst_pleorasink_unlock_stop);

  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_NUM_INTERNAL_BUFFERS, g_param_spec_int ("num-internal-buffers",
          "Number of internal buffers",
          "Number of buffers for the internal queue", 0, 64,
          DEFAULT_PROP_NUM_INTERNAL_BUFFERS,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
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
          "MAC address of the network interface to bind to (default is first found)",
          DEFAULT_PROP_MAC,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
#ifdef GST_PLUGINS_VISION_ENABLE_KLV
  g_object_class_install_property (gobject_class, PROP_OUTPUT_KLV,
      g_param_spec_boolean ("output-klv", "Output KLV",
          "Whether to output KLV as chunk data according to MISB ST1608",
          DEFAULT_PROP_OUTPUT_KLV,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
#endif
  g_object_class_install_property (gobject_class, PROP_AUTO_MULTICAST,
      g_param_spec_boolean ("auto-multicast", "Auto multicast",
          "Automatically multicast video, removing the need for a controller",
          DEFAULT_PROP_AUTO_MULTICAST,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (gobject_class, PROP_MULTICAST_GROUP,
      g_param_spec_string ("multicast-group", "Multicast group IP address",
          "The address of the multicast group to stream video (if auto-multicast is TRUE)",
          DEFAULT_PROP_MULTICAST_GROUP,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_MULTICAST_PORT,
      g_param_spec_int ("port", "Multicast port",
          "The port of the multicast group to stream video (if auto-multicast is TRUE)",
          0, 65535, DEFAULT_PROP_MULTICAST_PORT,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PACKET_SIZE,
      g_param_spec_int ("packet-size", "Packet size",
          "Packet size (if auto-multicast is TRUE)", 576, 65535,
          DEFAULT_PROP_PACKET_SIZE,
          (GParamFlags) (G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE)));
}

static void
gst_pleorasink_init (GstPleoraSink * sink)
{
  /* properties */
  sink->num_internal_buffers = DEFAULT_PROP_NUM_INTERNAL_BUFFERS;
  sink->address = g_strdup (DEFAULT_PROP_ADDRESS);
  sink->manufacturer = g_strdup (DEFAULT_PROP_MANUFACTURER);
  sink->model = g_strdup (DEFAULT_PROP_MODEL);
  sink->version = g_strdup (DEFAULT_PROP_VERSION);
  sink->info = g_strdup (DEFAULT_PROP_INFO);
  sink->serial = g_strdup (DEFAULT_PROP_SERIAL);
  sink->mac = g_strdup (DEFAULT_PROP_MAC);
  sink->output_klv = DEFAULT_PROP_OUTPUT_KLV;
  sink->auto_multicast = DEFAULT_PROP_AUTO_MULTICAST;
  sink->multicast_group = g_strdup (DEFAULT_PROP_MULTICAST_GROUP);
  sink->multicast_port = DEFAULT_PROP_MULTICAST_PORT;
  sink->packet_size = DEFAULT_PROP_PACKET_SIZE;

  sink->camera_connected = FALSE;

  sink->acquisition_started = FALSE;
  sink->stop_requested = FALSE;

  sink->source = new GstStreamingChannelSource ();
  sink->source->SetSink (sink);
  sink->device = new PvSoftDeviceGEV ();
}

void
gst_pleorasink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPleoraSink *sink;

  g_return_if_fail (GST_IS_PLEORASINK (object));
  sink = GST_PLEORASINK (object);

  switch (property_id) {
    case PROP_NUM_INTERNAL_BUFFERS:
      sink->num_internal_buffers = g_value_get_int (value);
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
    case PROP_OUTPUT_KLV:
      sink->output_klv = g_value_get_boolean (value);
      sink->source->SetKlvEnabled ((bool) sink->output_klv);
      break;
    case PROP_AUTO_MULTICAST:
      sink->auto_multicast = g_value_get_boolean (value);
      break;
    case PROP_MULTICAST_GROUP:
      g_free (sink->multicast_group);
      sink->multicast_group = g_strdup (g_value_get_string (value));
    case PROP_MULTICAST_PORT:
      sink->multicast_port = g_value_get_int (value);
      break;
    case PROP_PACKET_SIZE:
      sink->packet_size = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_pleorasink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstPleoraSink *sink;

  g_return_if_fail (GST_IS_PLEORASINK (object));
  sink = GST_PLEORASINK (object);

  switch (property_id) {
    case PROP_NUM_INTERNAL_BUFFERS:
      g_value_set_int (value, sink->num_internal_buffers);
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
    case PROP_OUTPUT_KLV:
      g_value_set_boolean (value, sink->output_klv);
      break;
    case PROP_AUTO_MULTICAST:
      g_value_set_boolean (value, sink->auto_multicast);
      break;
    case PROP_MULTICAST_GROUP:
      g_value_set_string (value, sink->multicast_group);
      break;
    case PROP_MULTICAST_PORT:
      g_value_set_int (value, sink->multicast_port);
      break;
    case PROP_PACKET_SIZE:
      g_value_set_int (value, sink->packet_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_pleorasink_dispose (GObject * object)
{
  GstPleoraSink *sink;

  g_return_if_fail (GST_IS_PLEORASINK (object));
  sink = GST_PLEORASINK (object);

  /* clean up as possible.  may be called multiple times */
  if (sink->device) {
    delete sink->device;
    sink->device = NULL;
  }
  if (sink->source) {
    delete sink->source;
    sink->source = NULL;
  }

  g_free (sink->address);

  G_OBJECT_CLASS (gst_pleorasink_parent_class)->dispose (object);
}

gboolean
gst_pleorasink_select_interface (GstPleoraSink * sink)
{
  PvSystem lSystem;
  const PvNetworkAdapter *selected_nic = NULL;
  guint iface_count;
  gchar *desired_mac = NULL;
  gchar *found_mac = NULL;

  /* we'll compare uppercase version of MAC */
  desired_mac = g_ascii_strup (sink->mac, -1);

  iface_count = lSystem.GetInterfaceCount ();

  GST_DEBUG_OBJECT (sink, "Found %d interface(s)", iface_count);

  for (guint32 i = 0; i < iface_count; i++) {
    const PvNetworkAdapter *lNIC = NULL;
    lNIC = dynamic_cast < const PvNetworkAdapter *>(lSystem.GetInterface (i));

    GST_DEBUG_OBJECT (sink,
        "Found network interface '%s', MAC: %s, IP: %s, Subnet: %s",
        lNIC->GetDescription ().GetAscii (),
        lNIC->GetMACAddress ().GetAscii (),
        lNIC->GetIPAddress (0).GetAscii (),
        lNIC->GetSubnetMask (0).GetAscii ());

    if ((lNIC == NULL) ||
        (lNIC->GetIPAddressCount () == 0) ||
        (lNIC->GetIPAddress (0) == "0.0.0.0")) {
      GST_DEBUG_OBJECT (sink, "Interface %d has no valid IP address", i);
      continue;
    }

    /* we'll compare uppercase version of MAC */
    found_mac = g_ascii_strup (lNIC->GetMACAddress ().GetAscii (), -1);
    if (g_strcmp0 (sink->mac, "") == 0 && g_strcmp0 (sink->address, "") == 0) {
      /* no MAC or IP set, use first found */
      GST_DEBUG_OBJECT (sink, "Selecting first interface we found");
      selected_nic = lNIC;

      /* set properties */
      g_free (sink->mac);
      sink->mac = g_strdup (lNIC->GetMACAddress ().GetAscii ());
      g_free (sink->address);
      sink->address = g_strdup (lNIC->GetIPAddress (0).GetAscii ());
    } else if (g_strcmp0 (desired_mac, found_mac) == 0) {
      GST_DEBUG_OBJECT (sink, "Selecting interface from MAC '%s'", sink->mac);
      selected_nic = lNIC;

      /* set properties */
      g_free (sink->address);
      sink->address = g_strdup (lNIC->GetIPAddress (0).GetAscii ());
    } else {
      guint32 num_ips = lNIC->GetIPAddressCount ();
      for (guint32 i = 0; i < num_ips; i++) {
        if (g_strcmp0 (sink->address, lNIC->GetIPAddress (i).GetAscii ()) == 0) {
          GST_DEBUG_OBJECT (sink, "Selecting interface from IP '%s'",
              sink->address);
          selected_nic = lNIC;

          /* set properties */
          g_free (sink->mac);
          sink->mac = g_strdup (lNIC->GetMACAddress ().GetAscii ());

          break;
        }
      }
    }
    g_free (found_mac);

    if (selected_nic) {
      break;
    }
  }
  g_free (desired_mac);

  if (selected_nic == NULL) {
    if (g_strcmp0 (sink->mac, "") != 0) {
      GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
          ("Failed to find network interface by MAC address '%s'", sink->mac),
          (NULL));
    } else if (g_strcmp0 (sink->address, "") != 0) {
      GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
          ("Failed to find network interface by IP address '%s'",
              sink->address), (NULL));
    } else {
      GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
          ("Failed to find any network interfaces"), (NULL));
    }
    return FALSE;
  } else {
    GST_DEBUG_OBJECT (sink,
        "Selecting network interface '%s', MAC: %s, IP: %s, Subnet: %s",
        selected_nic->GetDescription ().GetAscii (),
        selected_nic->GetMACAddress ().GetAscii (),
        selected_nic->GetIPAddress (0).GetAscii (),
        selected_nic->GetSubnetMask (0).GetAscii ());
    return TRUE;
  }
}

gboolean
gst_pleorasink_start (GstBaseSink * basesink)
{
  GstPleoraSink *sink = GST_PLEORASINK (basesink);

  IPvSoftDeviceGEVInfo *info = sink->device->GetInfo ();
  if (info) {
    info->SetManufacturerName (sink->manufacturer);
    info->SetModelName (sink->model);
    info->SetDeviceVersion (sink->version);
    info->SetManufacturerInformation (sink->info);
    info->SetSerialNumber (sink->serial);
  }

  return TRUE;
}

gboolean
gst_pleorasink_stop (GstBaseSink * basesink)
{
  GstPleoraSink *sink = GST_PLEORASINK (basesink);

  gst_pleorasink_stop_multicasting (sink);

  sink->device->Stop ();

  sink->camera_connected = FALSE;

  sink->acquisition_started = FALSE;
  sink->stop_requested = FALSE;

  return TRUE;
}

gboolean
gst_pleorasink_find_registers (GstPleoraSink * sink)
{
  sink->register_SCDA0 = NULL;
  sink->register_SCPS0 = NULL;
  sink->register_SCP0 = NULL;
  sink->register_AcquisitionStart0 = NULL;
  sink->register_AcquisitionStop0 = NULL;

  IPvRegisterMap *regmap = sink->device->GetRegisterMap ();
  size_t regcount = regmap->GetRegisterCount ();
  for (size_t i = 0; i < regcount; i++) {
    IPvRegister *reg = regmap->GetRegisterByIndex (i);
    const char *reg_name = reg->GetName ().GetAscii ();

    if (g_strcmp0 ("SCDA0", reg_name) == 0) {
      sink->register_SCDA0 = reg;
    } else if (g_strcmp0 ("SCPS0", reg_name) == 0) {
      sink->register_SCPS0 = reg;
    } else if (g_strcmp0 ("SCP0", reg_name) == 0) {
      sink->register_SCP0 = reg;
    } else if (g_strcmp0 ("AcquisitionStart0", reg_name) == 0) {
      sink->register_AcquisitionStart0 = reg;
    } else if (g_strcmp0 ("AcquisitionStop0", reg_name) == 0) {
      sink->register_AcquisitionStop0 = reg;
    }
  }

  if (!sink->register_SCDA0 || !sink->register_SCPS0 || !sink->register_SCP0
      || !sink->register_AcquisitionStart0
      || !sink->register_AcquisitionStop0) {
    GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
        ("Failed to find registers necessary to start auto multicasting"),
        (NULL));
    return FALSE;
  }

  return TRUE;
}

gboolean
gst_pleorasink_start_multicasting (GstPleoraSink * sink)
{
  PvResult pvRes;

  if (!gst_pleorasink_find_registers (sink))
    return FALSE;
  gchar **addr_elems = g_strsplit (sink->multicast_group, ".", 4);
  if (g_strv_length (addr_elems) != 4) {
    GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
        ("Multicast-group is not a valid IP address: %s",
            sink->multicast_group), (NULL));
    return FALSE;
  }

  guint8 multiaddr[4] =
      { atoi (addr_elems[3]), atoi (addr_elems[2]), atoi (addr_elems[1]),
    atoi (addr_elems[0])
  };
  pvRes = sink->register_SCDA0->Write (multiaddr, 4);
  if (!pvRes.IsOK ()) {
    GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
        ("Failed to set multicast-group address %s as 0x%08x",
            sink->multicast_group, multiaddr), (NULL));
    return FALSE;
  }

  pvRes = sink->register_SCPS0->Write (0x40000000 | sink->packet_size);
  if (!pvRes.IsOK ()) {
    GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
        ("Failed to set packet size %d", sink->packet_size), (NULL));
    return FALSE;
  }

  pvRes = sink->register_SCP0->Write (sink->multicast_port);
  if (!pvRes.IsOK ()) {
    GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
        ("Failed to set multicast-port %d", sink->multicast_port), (NULL));
    return FALSE;
  }

  pvRes = sink->register_AcquisitionStart0->Write (1);
  if (!pvRes.IsOK ()) {
    GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
        ("Failed to set AcquisitionStart0 register"), (NULL));
    return FALSE;
  }

  return TRUE;
}

void
gst_pleorasink_stop_multicasting (GstPleoraSink * sink)
{
  PvResult pvRes;
  if (sink->register_AcquisitionStop0) {
    pvRes = sink->register_AcquisitionStop0->Write (1);
    if (!pvRes.IsOK ()) {
      GST_ERROR_OBJECT (sink, "Failed to set AcquisitionStop0 register");
    }
  }
}

gboolean
gst_pleorasink_set_caps (GstBaseSink * basesink, GstCaps * caps)
{
  GstPleoraSink *sink = GST_PLEORASINK (basesink);
  PvResult pvRes;

  GST_DEBUG_OBJECT (sink, "Caps being set");

  gst_video_info_from_caps (&sink->vinfo, caps);

  sink->source->SetCaps (caps);

  GST_DEBUG_OBJECT (sink, "Adding stream to device");
  pvRes = sink->device->AddStream (sink->source);
  if (!pvRes.IsOK ()) {
    GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
        ("Failed to add stream to device (%s)",
            pvRes.GetDescription ().GetAscii ()), (NULL));
    return FALSE;
  }

  GST_DEBUG_OBJECT (sink, "Searching for interface");
  if (!gst_pleorasink_select_interface (sink)) {
    /* error already sent */
    return FALSE;
  }

  GST_DEBUG_OBJECT (sink, "Starting device");
  pvRes = sink->device->Start (sink->mac);
  if (!pvRes.IsOK ()) {
    GST_ELEMENT_ERROR (sink, RESOURCE, SETTINGS,
        ("Failed to start device (%s)",
            pvRes.GetDescription ().GetAscii ()), (NULL));
    return FALSE;
  }

  sink->acquisition_started = TRUE;
  sink->stop_requested = FALSE;

  if (sink->auto_multicast) {
    if (!gst_pleorasink_start_multicasting (sink)) {
      return FALSE;
    }
  }

  return TRUE;
}

GstFlowReturn
gst_pleorasink_render (GstBaseSink * basesink, GstBuffer * buffer)
{
  GstPleoraSink *sink = GST_PLEORASINK (basesink);

  if (sink->stop_requested) {
    GST_DEBUG_OBJECT (sink, "stop requested, flushing");
    return GST_FLOW_FLUSHING;
  }

  /* TODO: should we ever error out? */
  sink->source->SetBuffer (buffer);

  return GST_FLOW_OK;
}

gboolean
gst_pleorasink_unlock (GstBaseSink * basesink)
{
  GstPleoraSink *sink = GST_PLEORASINK (basesink);

  sink->stop_requested = TRUE;

  return TRUE;
}

gboolean
gst_pleorasink_unlock_stop (GstBaseSink * basesink)
{
  GstPleoraSink *sink = GST_PLEORASINK (basesink);

  sink->stop_requested = FALSE;

  return TRUE;
}
