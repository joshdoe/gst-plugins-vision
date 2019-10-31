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

#include <PvStreamingChannelSourceDefault.h>

#include "gstpleorasink.h"

class GstStreamingChannelSource:public PvStreamingChannelSourceDefault
{
public:
    GstStreamingChannelSource ();

    void SetSink (GstPleoraSink * sink);
    void SetCaps (GstCaps * caps);
    void ResizeBufferIfNeeded (PvBuffer * aBuffer);
    void SetBuffer (GstBuffer * buf);

    PvBuffer *AllocBuffer ();
    void FreeBuffer (PvBuffer * aBuffer);

    PvResult QueueBuffer (PvBuffer * aBuffer);
    PvResult RetrieveBuffer (PvBuffer ** aBuffer);

    void GetWidthInfo (uint32_t & aMin, uint32_t & aMax, uint32_t & aInc) const;
    void GetHeightInfo (uint32_t & aMin, uint32_t & aMax, uint32_t & aInc) const;
    PvResult GetSupportedPixelType (int aIndex, PvPixelType & aPixelType) const;

private:
    GstPleoraSink * mSink;
    PvBuffer *mAcquisitionBuffer;
    gboolean mBufferValid;
    gint mBufferCount;

    gint mWidth;
    gint mHeight;
    PvPixelType mPixelType;
};