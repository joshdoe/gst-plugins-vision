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

#include "streamingchannelsource.h"
#include "klv.h"

/* setup debug */
GST_DEBUG_CATEGORY_EXTERN (pleorasink_debug);
#define GST_CAT_DEFAULT pleorasink_debug

/* these seem to be arbitrary */
#define CHUNKLAYOUTID 0xABCD
#define KLV_CHUNKID 0xFEDC

GstStreamingChannelSource::GstStreamingChannelSource ()
:  mAcquisitionBuffer (NULL), mBufferCount (0), mBufferValid (FALSE),
    mChunkModeActive(TRUE), mChunkKlvEnabled(TRUE), mKlvChunkSize(0)
{

}

void
GstStreamingChannelSource::GetWidthInfo (uint32_t & aMin, uint32_t & aMax,
    uint32_t & aInc) const
{
  aMin = mWidth;
  aMax = aMin;
  aInc = 2;
}

void
GstStreamingChannelSource::GetHeightInfo (uint32_t & aMin, uint32_t & aMax,
    uint32_t & aInc) const
{
  aMin = mHeight;
  aMax = aMin;
  aInc = 2;
}

PvResult GstStreamingChannelSource::GetSupportedPixelType (int aIndex,
    PvPixelType & aPixelType) const
{
  if (aIndex != 0) {
    return PvResult::Code::INVALID_PARAMETER;
  }

  aPixelType = mPixelType;
  return PvResult::Code::OK;
}

PvResult GstStreamingChannelSource::GetSupportedChunk (int aIndex, uint32_t &aID, PvString &aName) const
{
    switch (aIndex) {
    case 0:
        aID = KLV_CHUNKID;
        aName = "KLV";
        return PvResult::Code::OK;
    default:
        break;
    }

    return PvResult::Code::INVALID_PARAMETER;
}

bool GstStreamingChannelSource::GetChunkEnable (uint32_t aChunkID) const
{
    switch (aChunkID) {
    case KLV_CHUNKID:
        return mChunkKlvEnabled;
    default:
        break;
    }

    return false;
}

PvResult GstStreamingChannelSource::SetChunkEnable (uint32_t aChunkID, bool aEnabled)
{
    switch (aChunkID) {
    case KLV_CHUNKID:
        mChunkKlvEnabled = aEnabled;
        mSink->output_klv = mChunkKlvEnabled;
        return PvResult::Code::OK;
    default:
        break;
    }

    return PvResult::Code::INVALID_PARAMETER;
}

uint32_t GstStreamingChannelSource::GetRequiredChunkSize() const
{
    if (mChunkModeActive && mChunkKlvEnabled) {
        /* chunk data must be multiple of 4 bytes, and 16 bytes extra seem
           to be needed for chunk ID and length */
        return GST_ROUND_UP_4 (mKlvChunkSize) + 16;
    } else {
        return 0;
    }
}

void GstStreamingChannelSource::SetKlvEnabled (bool enable)
{
    SetChunkEnable (KLV_CHUNKID, enable);
}

gboolean GstStreamingChannelSource::GetKlvEnabled()
{
    return GetChunkEnable (KLV_CHUNKID);
}

PvBuffer * GstStreamingChannelSource::AllocBuffer ()
{
  if (mBufferCount < mSink->num_internal_buffers) {
    mBufferCount++;
    return new PvBuffer;
  }
  return NULL;
}

void GstStreamingChannelSource::FreeBuffer (PvBuffer * aBuffer)
{
  delete aBuffer;
  mBufferCount--;
}

PvResult GstStreamingChannelSource::QueueBuffer (PvBuffer * aBuffer)
{
  g_mutex_lock (&mSink->mutex);
  if (mAcquisitionBuffer == NULL) {
    // No buffer queued, accept it
    mAcquisitionBuffer = aBuffer;
    mBufferValid = FALSE;
    g_mutex_unlock (&mSink->mutex);
    return PvResult::Code::OK;
  }
  g_mutex_unlock (&mSink->mutex);
  return PvResult::Code::BUSY;
}

PvResult GstStreamingChannelSource::RetrieveBuffer (PvBuffer ** aBuffer)
{
  gint64 end_time;

  g_mutex_lock (&mSink->mutex);
  // WAIT for buffer
  end_time = g_get_monotonic_time () + 50 * G_TIME_SPAN_MILLISECOND;
  while ((mAcquisitionBuffer == NULL || !mBufferValid)
      && !mSink->stop_requested) {
    if (!g_cond_wait_until (&mSink->cond, &mSink->mutex, end_time)) {
      // No buffer queued for acquisition
      g_mutex_unlock (&mSink->mutex);
      return PvResult::Code::NO_AVAILABLE_DATA;
    }
  }
  // Remove buffer from 1-deep pipeline
  *aBuffer = mAcquisitionBuffer;
  mAcquisitionBuffer = NULL;
  mBufferValid = FALSE;
  g_mutex_unlock (&mSink->mutex);

  return PvResult::Code::OK;
}

void
GstStreamingChannelSource::SetSink (GstPleoraSink * sink)
{
  mSink = sink;
}

void
GstStreamingChannelSource::SetCaps (GstCaps * caps)
{
  GstVideoInfo vinfo;
  gst_video_info_from_caps (&vinfo, caps);

  switch (GST_VIDEO_INFO_FORMAT (&vinfo)) {
    case GST_VIDEO_FORMAT_GRAY8:
      mPixelType = PvPixelMono8;
      break;
    case GST_VIDEO_FORMAT_GRAY16_LE:
      mPixelType = PvPixelMono16;
      break;
    case GST_VIDEO_FORMAT_RGB:
      mPixelType = PvPixelRGB8;
      break;
    case GST_VIDEO_FORMAT_RGBA:
      mPixelType = PvPixelRGBa8;
      break;
    case GST_VIDEO_FORMAT_BGR:
      mPixelType = PvPixelBGR8;
      break;
    case GST_VIDEO_FORMAT_BGRA:
      mPixelType = PvPixelBGRa8;
      break;
    default:
      mPixelType = PvPixelUndefined;
      break;
  }

  mWidth = GST_VIDEO_INFO_WIDTH (&vinfo);
  mHeight = GST_VIDEO_INFO_HEIGHT (&vinfo);
}

void
GstStreamingChannelSource::ResizeBufferIfNeeded (PvBuffer * aBuffer)
{
  uint32_t lRequiredChunkSize = GetRequiredChunkSize();
  PvImage *lImage = aBuffer->GetImage ();
  if ((lImage->GetWidth () != mWidth) ||
      (lImage->GetHeight () != mHeight) ||
      (lImage->GetPixelType () != mPixelType) ||
      (lImage->GetMaximumChunkLength () != lRequiredChunkSize)) {
    GST_LOG_OBJECT (mSink, "Width=%d, Height=%d, PixelType=%d, and/or ChunkLength=%d changed, reallocating buffer", mWidth, mHeight, mPixelType, lRequiredChunkSize);
    lImage->Alloc (mWidth, mHeight, mPixelType, 0, 0, lRequiredChunkSize);
  }
}

void
GstStreamingChannelSource::SetBuffer (GstBuffer * buf)
{
  GByteArray * klv_byte_array = NULL;

  GST_LOG_OBJECT (mSink, "SetBuffer");

  g_mutex_lock (&mSink->mutex);

  if (mAcquisitionBuffer == NULL) {
    GST_WARNING_OBJECT (mSink, "No PvBuffer available to fill, dropping frame");
    g_mutex_unlock (&mSink->mutex);
    return;
  }

  if (mBufferValid) {
    GST_WARNING_OBJECT (mSink,
        "Buffer already filled, dropping incoming frame");
    g_mutex_unlock (&mSink->mutex);
    return;
  }

  if (mChunkKlvEnabled) {
      klv_byte_array = GetKlvByteArray (buf);
      if (klv_byte_array) {
          mKlvChunkSize = klv_byte_array->len;
      } else {
          mKlvChunkSize = 0;
      }
  }

  ResizeBufferIfNeeded (mAcquisitionBuffer);

  /* TODO: avoid memcpy (when strides align) by attaching to PvBuffer */
  GstMapInfo minfo;
  gst_buffer_map (buf, &minfo, GST_MAP_READ);


  guint8 *dst = mAcquisitionBuffer->GetDataPointer ();
  if (!dst) {
    GST_ERROR_OBJECT (mSink, "Have buffer to fill, but data pointer is invalid");
    g_mutex_unlock (&mSink->mutex);
    return;
  }
  g_assert (mAcquisitionBuffer->GetSize () >= minfo.size);
  /* TODO: fix stride if needed */
  memcpy (dst, minfo.data, minfo.size);

  gst_buffer_unmap (buf, &minfo);

  mAcquisitionBuffer->ResetChunks();
  mAcquisitionBuffer->SetChunkLayoutID(CHUNKLAYOUTID);

  if (mChunkKlvEnabled && klv_byte_array && klv_byte_array->len > 0) {
    PvResult pvRes;
    pvRes = mAcquisitionBuffer->AddChunk (KLV_CHUNKID, (uint8_t*)klv_byte_array->data, klv_byte_array->len);
    if (pvRes.IsOK ()) {
        GST_LOG_OBJECT (mSink, "Added KLV as chunk data (len=%d)", klv_byte_array->len);
    } else {
        GST_WARNING_OBJECT (mSink, "Failed to add KLV as chunk data (len=%d): %s",
            klv_byte_array->len,
            pvRes.GetDescription ().GetAscii ());
    }
  }

  if (klv_byte_array) {
      g_byte_array_unref (klv_byte_array);
  }

  mBufferValid = TRUE;
  g_cond_signal (&mSink->cond);

  g_mutex_unlock (&mSink->mutex);
}

GByteArray * GstStreamingChannelSource::GetKlvByteArray (GstBuffer * buf)
{
      GstKLVMeta *klv_meta;
      gpointer iter = NULL;
      GByteArray *byte_array;

      /* spec says KLV can all be in one chunk, or multiple chunks, we do one chunk */
      byte_array = g_byte_array_new ();
      while ((klv_meta = (GstKLVMeta *) gst_buffer_iterate_meta_filtered (buf,
          &iter, GST_KLV_META_API_TYPE))) {
              gsize klv_size;
              const guint8 *klv_data;
              klv_data = gst_klv_meta_get_data (klv_meta, &klv_size);
              if (!klv_data) {
                  GST_WARNING_OBJECT (mSink, "Failed to get KLV data from meta");
                  break;
              }

              g_byte_array_append (byte_array, klv_data, (guint)klv_size);
      }

      /* chunk length must be multiple of 4 bytes */
      if (byte_array->len % 4 != 0) {
          const guint8 padding[4] = {0};
          const guint padding_len = GST_ROUND_UP_4 (byte_array->len) - byte_array->len;
          g_byte_array_append (byte_array, padding, padding_len);
      }

      return byte_array;
}