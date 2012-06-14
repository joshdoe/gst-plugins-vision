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
 * SECTION:element-gstiotechdaqx
 *
 * The iotechdaqx element is a source for data acquisition devices supported by the IOtechDaqX driver.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v iotechdaqx ! FIXME
 * ]|
 * FIXME from the default IOtechDaqX device
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst-i18n-lib.h>
#include <gst\audio\audio.h>
#include "gstiotechdaqx.h"

#define NULL 0

GST_DEBUG_CATEGORY_STATIC (gst_iotechdaqx_debug);
#define GST_CAT_DEFAULT gst_iotechdaqx_debug

/* prototypes */


static void gst_iotechdaqx_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_iotechdaqx_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_iotechdaqx_dispose (GObject * object);
static void gst_iotechdaqx_finalize (GObject * object);

static GstCaps *gst_iotechdaqx_get_caps (GstBaseSrc * src);
static gboolean gst_iotechdaqx_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_iotechdaqx_newsegment (GstBaseSrc * src);
static gboolean gst_iotechdaqx_start (GstBaseSrc * src);
static gboolean gst_iotechdaqx_stop (GstBaseSrc * src);
static void
gst_iotechdaqx_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
static gboolean gst_iotechdaqx_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_iotechdaqx_is_seekable (GstBaseSrc * src);
static gboolean gst_iotechdaqx_query (GstBaseSrc * src, GstQuery * query);
static gboolean gst_iotechdaqx_check_get_range (GstBaseSrc * src);
static void gst_iotechdaqx_fixate (GstBaseSrc * src, GstCaps * caps);
static GstFlowReturn gst_iotechdaqx_create (GstPushSrc * src, GstBuffer ** buf);

enum
{
  PROP_0,
  PROP_BOARD_INDEX
      /* FILL ME */
};

#define DEFAULT_PROP_BOARD_INDEX 0

/* pad templates */

static GstStaticPadTemplate gst_iotechdaqx_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        //GST_AUDIO_INT_PAD_TEMPLATE_CAPS)
        "audio/x-raw-int, "
        "rate = (int) 10000, "
        "channels = (int) 1, "
        "endianness = (int) BYTE_ORDER, "
        "width = (int) 16, " "depth = (int) 16, " "signed = (boolean) true")
    );

/* class initialization */

GST_BOILERPLATE (GstIOtechDaqX, gst_iotechdaqx, GstPushSrc, GST_TYPE_PUSH_SRC);


     char *GetProtocol (DaqProtocol protocol)
{
  /*This function is used to display the protcol name;  since the 
     protcol itself is stored as a number, this function associates 
     the protocol number with a string.
   */
  char *protocolName[64];

  switch (protocol) {
    case DaqProtocolNone:
      *protocolName = "DaqProtocolNone";
      break;
    case DaqProtocol4:
      *protocolName = "DaqProtocol4";
      break;
    case DaqProtocol8:
      *protocolName = "DaqProtocol8";
      break;
    case DaqProtocolSMC666:
      *protocolName = "DaqProtocolSMC666";
      break;
    case DaqProtocolFastEPP:
      *protocolName = "DaqProtocolFastEPP";
      break;
    case DaqProtocolECP:
      *protocolName = "DaqProtocolECP";
      break;
    case DaqProtocol8BitEPP:
      *protocolName = "DaqProtocol8BitEPP";
      break;
    case DaqProtocolTCPIP:
      *protocolName = "DaqProtocolTCPIP";
      break;
    case DaqProtocolISA:
      *protocolName = "DaqProtocolISA";
      break;
    case DaqProtocolPcCard:
      *protocolName = "DaqProtocolPcCard";
      break;
    case DaqProtocolUSB:
      *protocolName = "DaqProtocolUSB";
      break;
    case DaqProtocolPCI:
      *protocolName = "DaqProtocolPCI";
      break;
    case DaqProtocolCPCI:
      *protocolName = "DaqProtocolCPCI";
      break;
    default:
      *protocolName = "Unknown";
      break;
  }
  return *protocolName;
}


static void
gst_iotechdaqx_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_iotechdaqx_src_template));

  gst_element_class_set_details_simple (element_class,
      "IOtechDaqX Data Source", "Source/Audio",
      "IOtechDaqX data source", "Joshua Doe <oss@nvl.army.mil>");
}

static void
gst_iotechdaqx_class_init (GstIOtechDaqXClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_iotechdaqx_set_property;
  gobject_class->get_property = gst_iotechdaqx_get_property;
  gobject_class->dispose = gst_iotechdaqx_dispose;
  gobject_class->finalize = gst_iotechdaqx_finalize;
  base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_iotechdaqx_get_caps);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_iotechdaqx_set_caps);
  base_src_class->newsegment = GST_DEBUG_FUNCPTR (gst_iotechdaqx_newsegment);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_iotechdaqx_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_iotechdaqx_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_iotechdaqx_get_times);
  base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_iotechdaqx_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_iotechdaqx_is_seekable);
  base_src_class->query = GST_DEBUG_FUNCPTR (gst_iotechdaqx_query);
  base_src_class->check_get_range =
      GST_DEBUG_FUNCPTR (gst_iotechdaqx_check_get_range);
  base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_iotechdaqx_fixate);

  push_src_class->create = GST_DEBUG_FUNCPTR (gst_iotechdaqx_create);


  /* Install GObject properties */
  //g_object_class_install_property (gobject_class, PROP_BOARD_INDEX,
  //    g_param_spec_int ("board", "Board", "Index of board connected to camera",
  //        0, 15, DEFAULT_PROP_BOARD_INDEX, G_PARAM_READWRITE));

}

static void
gst_iotechdaqx_init (GstIOtechDaqX * iotechdaqx,
    GstIOtechDaqXClass * iotechdaqx_class)
{
  iotechdaqx->srcpad =
      gst_pad_new_from_static_template (&gst_iotechdaqx_src_template, "src");

  /* set source as live (no preroll) */
  gst_base_src_set_live (GST_BASE_SRC (iotechdaqx), TRUE);

  /* override default of BYTES to operate in time mode */
  gst_base_src_set_format (GST_BASE_SRC (iotechdaqx), GST_FORMAT_TIME);

  /* initialize member variables */
  iotechdaqx->caps = NULL;
  iotechdaqx->handle = -1;
  iotechdaqx->opened = FALSE;

  // FIXME: initialize ALL member variables
}

void
gst_iotechdaqx_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstIOtechDaqX *iotechdaqx;

  g_return_if_fail (GST_IS_IOTECHDAQX (object));
  iotechdaqx = GST_IOTECHDAQX (object);

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_iotechdaqx_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstIOtechDaqX *iotechdaqx;

  g_return_if_fail (GST_IS_IOTECHDAQX (object));
  iotechdaqx = GST_IOTECHDAQX (object);

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_iotechdaqx_dispose (GObject * object)
{
  GstIOtechDaqX *iotechdaqx;

  g_return_if_fail (GST_IS_IOTECHDAQX (object));
  iotechdaqx = GST_IOTECHDAQX (object);

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

void
gst_iotechdaqx_finalize (GObject * object)
{
  GstIOtechDaqX *iotechdaqx;

  g_return_if_fail (GST_IS_IOTECHDAQX (object));
  iotechdaqx = GST_IOTECHDAQX (object);

  /* clean up object here */

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static GstCaps *
gst_iotechdaqx_get_caps (GstBaseSrc * src)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);

  GST_DEBUG_OBJECT (iotechdaqx, "get_caps");

  /* return template caps if we don't know the actual camera caps */
  if (!iotechdaqx->caps) {
    return
        gst_caps_copy (gst_pad_get_pad_template_caps (GST_BASE_SRC_PAD
            (iotechdaqx)));
  }

  return gst_caps_copy (iotechdaqx->caps);

  return NULL;
}

static gboolean
gst_iotechdaqx_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);
  GstStructure *structure;
  gboolean ret;

  GST_DEBUG_OBJECT (iotechdaqx, "set_caps");

  if (iotechdaqx->caps) {
    gst_caps_unref (iotechdaqx->caps);
    iotechdaqx->caps = gst_caps_copy (caps);
  }

  /* parse caps */

  if (gst_caps_get_size (caps) < 1)
    return FALSE;

  structure = gst_caps_get_structure (caps, 0);

  ret = gst_structure_get (structure,
      "width", G_TYPE_INT, &iotechdaqx->width,
      "rate", G_TYPE_INT, &iotechdaqx->rate,
      "channels", G_TYPE_INT, &iotechdaqx->channels, NULL);

  if (!ret) {
    GST_DEBUG ("Failed to retrieve width and height");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_iotechdaqx_newsegment (GstBaseSrc * src)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);

  GST_DEBUG_OBJECT (iotechdaqx, "newsegment");

  return TRUE;
}

static gboolean
gst_iotechdaqx_start (GstBaseSrc * src)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);
  DaqDeviceListT *devList;
  DaqDevicePropsT devProps;
  DWORD devCount, deviceIndex;

  GST_DEBUG_OBJECT (iotechdaqx, "start");


  // Find out how many devices are installed and allocate memory for device list
  daqGetDeviceCount (&devCount);
  devList = (DaqDeviceListT *) malloc (sizeof (DaqDeviceListT) * devCount);

  GST_DEBUG ("Found %d devices", devCount);

  // Get the names of all installed devices and the device count
  daqGetDeviceList (devList, &devCount);

  deviceIndex = 0;
  do {
    // Get the device properties for each device
    daqGetDeviceProperties (devList[deviceIndex].daqName, &devProps);

    GST_DEBUG ("Device %i: %s", deviceIndex, devList[deviceIndex].daqName);
    deviceIndex++;

    // Loop until a WaveBook is found or all the devices have been searched
  } while (deviceIndex < devCount);

  // We are done with the device list
  free (devList);

  iotechdaqx->handle = daqOpen (devProps.daqName);
  GST_DEBUG ("Connected to %s on LPT%d\n", devProps.daqName,
      devProps.basePortAddress + 1);
  GST_DEBUG ("Protocol: %s \n", GetProtocol (devProps.protocol));

  //FIXME check for errors!

  return TRUE;
}

static gboolean
gst_iotechdaqx_stop (GstBaseSrc * src)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);

  GST_DEBUG_OBJECT (iotechdaqx, "stop");

  /* Stop the acquisition */
  daqAdcTransferStop (iotechdaqx->handle);
  daqClose (iotechdaqx->handle);

  gst_caps_unref (iotechdaqx->caps);
  iotechdaqx->caps = NULL;

  iotechdaqx->opened = FALSE;

  return TRUE;
}

static void
gst_iotechdaqx_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);

  //GST_DEBUG_OBJECT (iotechdaqx, "get_times");
}

static gboolean
gst_iotechdaqx_get_size (GstBaseSrc * src, guint64 * size)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);

  GST_DEBUG_OBJECT (iotechdaqx, "get_size");

  return TRUE;
}

static gboolean
gst_iotechdaqx_is_seekable (GstBaseSrc * src)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);

  GST_DEBUG_OBJECT (iotechdaqx, "is_seekable");

  return FALSE;
}

static gboolean
gst_iotechdaqx_query (GstBaseSrc * src, GstQuery * query)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);

  GST_DEBUG_OBJECT (iotechdaqx, "query");

  return TRUE;
}

static gboolean
gst_iotechdaqx_check_get_range (GstBaseSrc * src)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);

  GST_DEBUG_OBJECT (iotechdaqx, "get_range");

  return FALSE;
}

static void
gst_iotechdaqx_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);

  GST_DEBUG_OBJECT (iotechdaqx, "fixate");
}

static GstFlowReturn
gst_iotechdaqx_create (GstPushSrc * src, GstBuffer ** buf)
{
  GstIOtechDaqX *iotechdaqx = GST_IOTECHDAQX (src);
  DWORD retCount;

  //GST_DEBUG ("create (handle=%d)", iotechdaqx->handle);

  if (!iotechdaqx->opened) {
    DWORD channels[1] = { 1 };
    DaqAdcGain gains[1] = { DgainX1 };
    DWORD flags[1] = { DafAnalog | DafBipolar };

    GST_DEBUG ("Setting up acquisition: rate=%d", iotechdaqx->rate);

    daqAdcSetScan (iotechdaqx->handle, channels, gains, flags, 1);
    daqAdcSetAcq (iotechdaqx->handle, DaamInfinitePost, 0, 0);
    daqSetTriggerEvent (iotechdaqx->handle, DatsSoftware, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, DaqStartEvent);
    //daqSetTriggerEvent (iotechdaqx->handle, DatsSoftware, NULL, NULL, NULL, NULL, NULL, NULL, NULL, DaqStopEvent);
    daqAdcSetFreq (iotechdaqx->handle, (float) iotechdaqx->rate);
    daqAdcTransferSetBuffer (iotechdaqx->handle, NULL, 44100,
        DatmCycleOn | DatmDriverBuf);

    daqAdcTransferStart (iotechdaqx->handle);
    daqAdcArm (iotechdaqx->handle);
    daqAdcSoftTrig (iotechdaqx->handle);        //FIXME: put in create() and use has_started flag

    iotechdaqx->opened = TRUE;
  }

  gst_pad_alloc_buffer_and_set_caps (GST_BASE_SRC_PAD (GST_BASE_SRC (src)), 0,
      2048 * 2, iotechdaqx->caps, buf);

  daqAdcTransferBufData (iotechdaqx->handle, GST_BUFFER_DATA (*buf), 2048,
      DabtmOldest | DabtmWait, &retCount);

  //GST_DEBUG ("Asked for %d samples, got %d", 2048, retCount);

  //memcpy(GST_BUFFER_DATA (*buf), tmp, 16*sizeof(SHORT));

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_iotechdaqx_debug, "iotechdaqx", 0,
      "debug category for iotechdaqx element");
  gst_element_register (plugin, "iotechdaqx", GST_RANK_NONE,
      gst_iotechdaqx_get_type ());

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "iotechdaqx",
    "IOtechDaqX source",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
