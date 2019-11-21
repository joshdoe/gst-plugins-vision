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

/**
 * SECTION:gsttagklv
 * @short_description: KLV metadata support
 * @title: KLV metadata support
 *
 * <refsect2>
 * <para>
 * Utility functions around KLV metadata support in GStreamer.
 * </para>
 * <para>
 * See ITU Recommendation BT.1563-1 or SMPTE 336M for the KLV standard;
 * see MISB EG 0902 (MISB Minimum Metadata Set) and other MISB standards
 * and recommended practices for examples of KLV metadata local data sets
 * for a variety of use cases (timestamps, GPS coordinates, speed, etc.).
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/tag/tag.h>
#include "klv.h"

/* We hide the implementation details, so that we have the option to implement
 * different/more efficient storage in future (unfortunately we can't put the
 * data inline with the meta allocation though since it's registered as fixed
 * size, although that could probably be fixed in a backwards compatible way).
 * GBytes means effectively two allocations, one for the GBytes, one for the
 * data. Plus one for the KLV meta struct.
 *
 * For now we also assume that KLV data is always self-contained and one single
 * chunk of data, but in future we may have use cases where we might want to
 * relax that requirement. */
typedef struct
{
  GstKLVMeta klv_meta;
  GBytes *bytes;
} GstKLVMetaImpl;

GType
gst_klv_meta_api_get_type (void)
{
  static volatile GType type;
  static const gchar *tags[] = { NULL };

  if (g_once_init_enter (&type)) {
    GType _type = gst_meta_api_type_register ("GstKLVMetaAPI", tags);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static gboolean
gst_klv_meta_init (GstMeta * meta, gpointer params, GstBuffer * buffer)
{
  GstKLVMetaImpl *impl = (GstKLVMetaImpl *) meta;

  impl->bytes = NULL;
  return TRUE;
}

static void
gst_klv_meta_clear (GstMeta * meta, GstBuffer * buffer)
{
  GstKLVMetaImpl *impl = (GstKLVMetaImpl *) meta;

  if (impl->bytes != NULL)
    g_bytes_unref (impl->bytes);
}

static gboolean
gst_klv_meta_transform (GstBuffer * dest, GstMeta * meta,
    GstBuffer * buffer, GQuark type, gpointer data)
{
  GstKLVMetaImpl *smeta;
  GstKLVMeta *dmeta;

  smeta = (GstKLVMetaImpl *) meta;

  if (GST_META_TRANSFORM_IS_COPY (type)) {
    dmeta = gst_buffer_add_klv_meta_from_bytes (dest, smeta->bytes);
    if (!dmeta)
      return FALSE;
  } else {
    return FALSE;
  }

  return TRUE;
}

const GstMetaInfo *
gst_klv_meta_get_info (void)
{
  static const GstMetaInfo *klv_meta_info = NULL;

  if (g_once_init_enter ((GstMetaInfo **) & klv_meta_info)) {
    const GstMetaInfo *meta =
        gst_meta_register (GST_KLV_META_API_TYPE, "GstKLVMeta",
        sizeof (GstKLVMetaImpl), gst_klv_meta_init, gst_klv_meta_clear,
        gst_klv_meta_transform);
    g_once_init_leave ((GstMetaInfo **) & klv_meta_info, (GstMetaInfo *) meta);
  }
  return klv_meta_info;
}

/* Add KLV meta data to a buffer */

static GstKLVMeta *
gst_buffer_add_klv_meta_internal (GstBuffer * buffer, GBytes * bytes)
{
  GstKLVMetaImpl *impl;
  GstKLVMeta *meta;
  gconstpointer data;
  gsize size;

  /* KLV coding shall use and only use a fixed 16-byte SMPTE-administered
   * Universal Label, according to SMPTE 298M as Key (Rec. ITU R-BT.1653-1) */
  data = g_bytes_get_data (bytes, &size);
  if (size < 16 || GST_READ_UINT32_BE (data) != 0x060E2B34) {
    GST_ERROR ("Trying to attach a invalid KLV meta data to buffer");
    g_bytes_unref (bytes);
    return NULL;
  }

  meta = (GstKLVMeta *) gst_buffer_add_meta (buffer, GST_KLV_META_INFO, NULL);

  GST_TRACE ("Adding %u bytes of KLV data to buffer %p", (guint) size, buffer);

  impl = (GstKLVMetaImpl *) meta;
  impl->bytes = bytes;

  return meta;
}

/**
 * gst_buffer_add_klv_meta_from_data: (skip)
 * @buffer: a #GstBuffer
 * @data: (array length=size) (transfer none): KLV data with 16-byte KLV
 *     Universal Label prefix
 * @size: size of @data in bytes
 *
 * Attaches #GstKLVMeta metadata to @buffer with the given parameters,
 * Does not take ownership of @data.
 *
 * Returns: (transfer none): the #GstKLVMeta on @buffer.
 *
 * Since: 1.16
 */
GstKLVMeta *
gst_buffer_add_klv_meta_from_data (GstBuffer * buffer, const guint8 * data,
    gsize size)
{
  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (data != NULL && size > 16, NULL);

  return gst_buffer_add_klv_meta_internal (buffer, g_bytes_new (data, size));
}

/**
 * gst_buffer_add_klv_meta_take_data: (skip)
 * @buffer: a #GstBuffer
 * @data: (array length=size) (transfer full): KLV data with 16-byte KLV
 *     Universal Label prefix
 * @size: size of @data in bytes
 *
 * Attaches #GstKLVMeta metadata to @buffer with the given parameters,
 * Take ownership of @data.
 *
 * Returns: (transfer none): the #GstKLVMeta on @buffer.
 *
 * Since: 1.16
 */
GstKLVMeta *
gst_buffer_add_klv_meta_take_data (GstBuffer * buffer, guint8 * data,
    gsize size)
{
  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (data != NULL && size > 16, NULL);

  return gst_buffer_add_klv_meta_internal (buffer, g_bytes_new_take (data,
          size));
}

/**
 * gst_buffer_add_klv_meta_from_bytes:
 * @buffer: a #GstBuffer
 * @bytes: (transfer none): KLV data with 16-byte KLV Universal Label prefix
 *
 * Attaches #GstKLVMeta metadata to @buffer with the given parameters,
 * Does not take ownership of @bytes, you will need to unref @bytes.
 *
 * Returns: (transfer none): the #GstKLVMeta on @buffer.
 *
 * Since: 1.16
 */
GstKLVMeta *
gst_buffer_add_klv_meta_from_bytes (GstBuffer * buffer, GBytes * bytes)
{
  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  return gst_buffer_add_klv_meta_internal (buffer, g_bytes_ref (bytes));
}

/**
 * gst_buffer_add_klv_meta_take_bytes:
 * @buffer: a #GstBuffer
 * @bytes: (transfer full): KLV data with 16-byte KLV Universal Label prefix
 *
 * Attaches #GstKLVMeta metadata to @buffer with the given parameters,
 * Takes ownership of @bytes.
 *
 * Returns: (transfer none): the #GstKLVMeta on @buffer.
 *
 * Since: 1.16
 */
GstKLVMeta *
gst_buffer_add_klv_meta_take_bytes (GstBuffer * buffer, GBytes * bytes)
{
  g_return_val_if_fail (buffer != NULL, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);

  return gst_buffer_add_klv_meta_internal (buffer, bytes);
}

/* Get KLV meta data from a buffer */

/**
 * gst_buffer_get_klv_meta:
 * @buffer: a #GstBuffer
 *
 * Returns: a #GstKLVMeta on the buffer, or %NULL if the buffer has none.
 *
 * Since: 1.16
 */
GstKLVMeta *
gst_buffer_get_klv_meta (GstBuffer * buffer)
{
  return (GstKLVMeta *) gst_buffer_get_meta (buffer, GST_KLV_META_API_TYPE);
}

/**
 * gst_klv_meta_get_data: (skip)
 * @klv_meta: a #GstKLVMeta
 * @size: the size of the returned data in bytes.
 *
 * Returns: (transfer none): the KLV data
 *
 * Since: 1.16
 */
const guint8 *
gst_klv_meta_get_data (GstKLVMeta * klv_meta, gsize * size)
{
  GstKLVMetaImpl *impl;

  g_return_val_if_fail (klv_meta != NULL, NULL);
  g_return_val_if_fail (size != NULL, NULL);

  impl = (GstKLVMetaImpl *) klv_meta;

  return g_bytes_get_data (impl->bytes, size);
}

/**
 * gst_klv_meta_get_bytes:
 * @klv_meta: a #GstKLVMeta
 *
 * Returns: (transfer none): the KLV data as a #GBytes
 *
 * Since: 1.16
 */
GBytes *
gst_klv_meta_get_bytes (GstKLVMeta * klv_meta)
{
  GstKLVMetaImpl *impl;

  g_return_val_if_fail (klv_meta != NULL, NULL);

  impl = (GstKLVMetaImpl *) klv_meta;

  return impl->bytes;
}

/* Boxed type, so bindings can use the API */

static gpointer
gst_klv_meta_copy_boxed (gpointer boxed)
{
  GstKLVMetaImpl *impl = boxed;
  GstKLVMetaImpl *copy;

  copy = g_new (GstKLVMetaImpl, 1);
  copy->bytes = impl->bytes ? g_bytes_ref (impl->bytes) : NULL;
  return copy;
}

static void
gst_klv_meta_free_boxed (gpointer boxed)
{
  GstKLVMetaImpl *impl = boxed;

  if (impl->bytes)
    g_bytes_unref (impl->bytes);

  g_free (impl);
}

G_DEFINE_BOXED_TYPE (GstKLVMeta, gst_klv_meta, gst_klv_meta_copy_boxed,
    gst_klv_meta_free_boxed);
