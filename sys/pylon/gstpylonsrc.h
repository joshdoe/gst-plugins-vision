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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_PYLONSRC_H_
#define _GST_PYLONSRC_H_

#include <gst/base/gstpushsrc.h>
#include "pylonc/PylonC.h"

// pylonsrc plugin calls PylonInitialize when first plugin is created
// and PylonTerminate when the last plugin is finalized.
// Static variable is used to keep count of existing plugins
// These functions can be used to increase or decrease this counter,
// if pylon environment is needed beyond lifetime of plugins.
// On success return value is the new value of counter
// On failure return value is negative
int gst_pylonsrc_ref_pylon_environment ();
int gst_pylonsrc_unref_pylon_environment ();

enum
{
  GST_PYLONSRC_NUM_CAPTURE_BUFFERS = 10,
  GST_PYLONSRC_NUM_AUTO_FEATURES = 3,
  GST_PYLONSRC_NUM_LIMITED_FEATURES = 2,
  GST_PYLONSRC_NUM_PROPS = 75
};

typedef enum _GST_PYLONSRC_PROPERTY_STATE
{
  GST_PYLONSRC_PROPST_DEFAULT,
  GST_PYLONSRC_PROPST_NOT_SET,
  GST_PYLONSRC_PROPST_SET
} GST_PYLONSRC_PROPERTY_STATE;

typedef struct _GstPylonSrcLimitedFeature
{
  double lower;
  double upper;
  double manual;
} GstPylonSrcLimitedFeature;

G_BEGIN_DECLS
#define GST_TYPE_PYLONSRC   (gst_pylonsrc_get_type())
#define GST_PYLONSRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PYLONSRC,GstPylonSrc))
#define GST_PYLONSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PYLONSRC,GstPylonSrcClass))
#define GST_IS_PYLONSRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PYLONSRC))
#define GST_IS_PYLONSRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PYLONSRC))
typedef struct _GstPylonSrc GstPylonSrc;
typedef struct _GstPylonSrcClass GstPylonSrcClass;

struct _GstPylonSrc
{
  GstPushSrc base_pylonsrc;

  GstCaps *caps;

  gint cameraId;
  PYLON_DEVICE_HANDLE deviceHandle;     // Handle for the camera.
  PYLON_STREAMGRABBER_HANDLE streamGrabber;     // Handler for camera's streams.
  PYLON_WAITOBJECT_HANDLE waitObject;   // Handles timing out in the main loop.
  gboolean deviceConnected;
  gboolean acquisition_configured;

  unsigned char *buffers[GST_PYLONSRC_NUM_CAPTURE_BUFFERS];
  PYLON_STREAMBUFFER_HANDLE bufferHandle[GST_PYLONSRC_NUM_CAPTURE_BUFFERS];

  int32_t frameSize;            // Size of a frame in bytes.
  int32_t payloadSize;          // Size of a frame in bytes.
  guint64 frameNumber;          // Fun note: At 120fps it will take around 4 billion years to overflow this variable.
  gint failedFrames;            // Count of concecutive frames that have failed.

  // Plugin parameters
  _Bool setFPS, continuousMode, limitBandwidth, demosaicing, colorAdjustment;
  _Bool center[2];
  _Bool flip[2];
  _Bool ignoreDefaults;
  double fps, blacklevel, gamma, sharpnessenhancement, noisereduction,
      brightnesstarget;
  double balance[3];
  double hue[6];
  double saturation[6];
  double transformation[3][3];

  GstPylonSrcLimitedFeature limitedFeature[GST_PYLONSRC_NUM_LIMITED_FEATURES];

  gint maxBandwidth, testImage, frameDropLimit, grabtimeout, packetSize,
      interPacketDelay, frameTransDelay, bandwidthReserve, bandwidthReserveAcc;
  gint size[2];
  gint binning[2];
  gint maxSize[2];
  gint offset[2];
  gchar *pixel_format, *sensorMode, *lightsource, *reset, *autoprofile,
      *transformationselector, *userid, *testImageSource;
  gchar *autoFeature[GST_PYLONSRC_NUM_AUTO_FEATURES];
  gchar *configFile;
  GST_PYLONSRC_PROPERTY_STATE propFlags[GST_PYLONSRC_NUM_PROPS];
};

struct _GstPylonSrcClass
{
  GstPushSrcClass base_pylonsrc_class;
};

GType gst_pylonsrc_get_type (void);

G_END_DECLS
#endif
