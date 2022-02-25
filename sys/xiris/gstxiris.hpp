/* GStreamer
 * Copyright (C) 2022 MinhQuan Tran <minhquan.tran@adlinktech.com>
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

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include "WeldSDK/WeldSDK.h"
#include "XirisCommon/XImage.h"
#include "XImageLib/Image/CXImage.h"

using namespace WeldSDK;
using XirisCommon::XImage;
using XImageLib::CXImage;

GST_DEBUG_CATEGORY_STATIC (gst_xirissrc_debug_category);
#define GST_CAT_DEFAULT gst_xirissrc_debug_category

bool gCameraReady = false;
bool gBufferReady = false;
bool gIsConnected = false;
bool gIsMatched = true;
std::string gSerialNumber;

class DemoCameraEventSink : public WeldCamera, public CameraEventSink
{
  private:
    std::string _macAddr;
  public:
    std::string mSerialNumber;
    ShutterModes mShutterMode;
    float mGlobalExposure;
    float mGlobalFrameRateLimit;
    bool mGlobalFrameRateLimitEnabled;
    double mRollingFrameRate;
    PixelDepths mPixelDepth;
    XImage mCapturedImage;

    DemoCameraEventSink(std::string mac):_macAddr(mac)
    {
      AttachEventSink(this);
    }

    virtual void OnCameraReady(CameraReadyEventArgs args) override
    {
      if (args.IsReady && gIsMatched)
      {
        gCameraReady = true;

        GST_DEBUG("Setting Shutter Mode: %d", mShutterMode);
        setShutterMode(mShutterMode);
        GST_DEBUG("Setting Exposure: %f", mGlobalExposure);
        setExposureTimeGlobal(mGlobalExposure);
        GST_DEBUG("Setting Global Frame Rate Limit Enabled: %d", mGlobalFrameRateLimitEnabled);
        setGSFrameRateLimitEnabled(mGlobalFrameRateLimitEnabled);
        GST_DEBUG("Setting Global Frame Rate Limit: %f", mGlobalFrameRateLimit);
        setGSFrameRateLimit(mGlobalFrameRateLimit);
        GST_DEBUG("Setting Pixel Depth: %d", mPixelDepth);
        setPixelDepth(mPixelDepth);
        GST_DEBUG("Setting Rolling Frame Rate: %f", mRollingFrameRate);
        setRollingFrameRate(mRollingFrameRate);

        GST_DEBUG("Camera started streaming...");

        if (!Start())
        {
          GST_DEBUG("Camera failed to start!");
        };
      }
    }

    virtual void OnBufferReady(BufferReadyEventArgs args) override
    {
      gBufferReady = true;
      mCapturedImage = *args.Image;
    }
};

DemoCameraEventSink* gWeldCamera = nullptr;

void ConnectToCamera(std::string macAddr, std::string ipAddr, WeldSDK::CameraClass cameraClass)
{
  gWeldCamera = new DemoCameraEventSink(macAddr);
  
  gWeldCamera->Connect(ipAddr, cameraClass);
  if (gSerialNumber.compare(gWeldCamera->getSerialNumber()) != 0)
  {
    gIsMatched = false;
    GST_DEBUG("Failed to match serial number '%s' with a camera!", gSerialNumber.c_str());
  }
}

class DemoCameraDetectorEventSink : public CameraDetectorEventSink
{
  public:
    virtual void OnCameraDetected(CameraEventArgs args) override
    {
      GST_DEBUG("Detected camera - Name: %s, IP: %s, MAC: %s", args.CameraTypeName().c_str(), args.CameraIPAddress.c_str(), args.CameraMACAddress.c_str());

      if (args.CanConnect)
      {
        ConnectToCamera(args.CameraMACAddress, args.CameraIPAddress, args.CameraType);
      }
    }

    virtual void OnLogMessage(LogMessageArgs args)
    {
      // Only output critical messages
      if (args.Level == 0)
      {
        GST_DEBUG("%s", args.Message.c_str());
      }
    }
};