/* GStreamer KLV Metadata Support Library
 * Copyright (C) 2016-2019 Tim-Philipp Müller <tim@centricular.com>
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

#ifndef __GST_TAG_KLV_H__
#define __GST_TAG_KLV_H__

#include <gst/gst.h>

// FIXME: include this for now until gst-plugins-base MR124 is accepted
//#define GST_API_EXPORT __declspec(dllexport) extern
//#define GST_TAG_API GST_API_EXPORT
#if defined (_MSC_VER)
  #define GST_KLV_EXPORT __declspec(dllexport)
  #define GST_KLV_IMPORT __declspec(dllimport)
#elif defined (__GNUC__)
  #define GST_KLV_EXPORT __attribute__((visibility("default")))
  #define GST_KLV_IMPORT
#else
  #define GST_KLV_EXPORT
  #define GST_KLV_IMPORT
#endif

#ifdef BUILDING_GST_KLV
#define GST_KLV_API GST_KLV_EXPORT
#else
#define GST_KLV_API GST_KLV_IMPORT
#endif
#define GST_TAG_API GST_KLV_API

G_BEGIN_DECLS

/**
 * GstKLVMeta:
 *
 * An opaque #GstMeta structure representing a self-contained KLV metadata
 * block that can be attached to buffers.
 *
 * Since: 1.16
 */
typedef struct {
  /*< private >*/
  GstMeta meta;
} GstKLVMeta;

GST_TAG_API
GType               gst_klv_meta_get_type (void);

#define GST_KLV_META_API_TYPE  (gst_klv_meta_api_get_type())
#define GST_KLV_META_INFO      (gst_klv_meta_get_info())

GST_TAG_API
GType               gst_klv_meta_api_get_type (void);

GST_TAG_API
const GstMetaInfo * gst_klv_meta_get_info (void);

/* Add KLV meta data to a buffer */

GST_TAG_API
GstKLVMeta        * gst_buffer_add_klv_meta_from_data (GstBuffer * buffer, const guint8 * data, gsize size);

GST_TAG_API
GstKLVMeta        * gst_buffer_add_klv_meta_take_data (GstBuffer * buffer, guint8 * data, gsize size);

GST_TAG_API
GstKLVMeta        * gst_buffer_add_klv_meta_from_bytes (GstBuffer * buffer, GBytes * bytes);

GST_TAG_API
GstKLVMeta        * gst_buffer_add_klv_meta_take_bytes (GstBuffer * buffer, GBytes * bytes);

/* Get KLV meta data from a buffer */

GST_TAG_API
GstKLVMeta        * gst_buffer_get_klv_meta (GstBuffer * buffer);

GST_TAG_API
const guint8      * gst_klv_meta_get_data (GstKLVMeta * klv_meta, gsize * size);

GST_TAG_API
GBytes            * gst_klv_meta_get_bytes (GstKLVMeta * klv_meta);

G_END_DECLS

#endif /* __GST_TAG_KLV_H__ */
