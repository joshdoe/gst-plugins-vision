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

GstStreamingChannelSource::GstStreamingChannelSource ()
:  mAcquisitionBuffer (NULL), mBufferCount (0), mBufferValid (FALSE)
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
  uint32_t lRequiredChunkSize = 0;
  PvImage *lImage = aBuffer->GetImage ();
  if ((lImage->GetWidth () != mWidth) ||
      (lImage->GetHeight () != mHeight) ||
      (lImage->GetPixelType () != mPixelType) ||
      (lImage->GetMaximumChunkLength () != lRequiredChunkSize)) {
    lImage->Alloc (mWidth, mHeight, mPixelType, 0, 0, lRequiredChunkSize);
  }
}

void
GstStreamingChannelSource::SetBuffer (GstBuffer * buf)
{
  GST_ERROR ("Set buffer");

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

  /* TODO: avoid memcpy (when strides align) by attaching to PvBuffer */
  GstMapInfo minfo;
  gst_buffer_map (buf, &minfo, GST_MAP_READ);

  ResizeBufferIfNeeded (mAcquisitionBuffer);

  guint8 *dst = mAcquisitionBuffer->GetDataPointer ();
  if (!dst) {
    GST_ERROR ("Have buffer to fill, but data pointer is invalid");
    g_mutex_unlock (&mSink->mutex);
    return;
  }
  g_assert (mAcquisitionBuffer->GetSize () >= minfo.size);
  /* TODO: fix stride if needed */
  memcpy (dst, minfo.data, minfo.size);

  gst_buffer_unmap (buf, &minfo);

  mBufferValid = TRUE;
  g_cond_signal (&mSink->cond);

  g_mutex_unlock (&mSink->mutex);
}