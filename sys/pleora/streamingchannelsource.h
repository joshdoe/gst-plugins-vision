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

    PvResult GetSupportedChunk (int aIndex, uint32_t &aID, PvString &aName) const;
    bool GetChunkEnable (uint32_t aChunkID) const;
    PvResult SetChunkEnable (uint32_t aChunkID, bool aEnabled);
    bool GetChunkModeActive() const { return mChunkModeActive; }
    PvResult SetChunkModeActive( bool aEnabled ) { mChunkModeActive = aEnabled; return PvResult::Code::OK; }
    uint32_t GetChunksSize() const { return GetRequiredChunkSize(); }

    uint32_t GetRequiredChunkSize () const;
    void SetKlvEnabled (bool enable = true);
    gboolean GetKlvEnabled ();
    GByteArray * GetKlvByteArray (GstBuffer * buf);

private:
    GstPleoraSink * mSink;
    PvBuffer *mAcquisitionBuffer;
    gboolean mBufferValid;
    gint mBufferCount;

    gint mWidth;
    gint mHeight;
    PvPixelType mPixelType;

    bool mChunkModeActive;
    bool mChunkKlvEnabled;

    gint mKlvChunkSize;
};