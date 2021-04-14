/* GStreamer
 * Copyright (C) 2016 William Manley <will@williammanley.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstklvinject.h"
#include "gstklvinspect.h"
#include "gstklvtimestamp.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  return
      gst_element_register (plugin, "klvinspect",
      GST_RANK_NONE, GST_TYPE_KLVINSPECT) &&
      gst_element_register (plugin, "klvinject",
      GST_RANK_NONE, GST_TYPE_KLVINJECT) &&
      gst_element_register (plugin, "klvtimestamp",
      GST_RANK_NONE, GST_TYPE_KLVTIMESTAMP);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    klv,
    "Elements for working with KLV data",
    plugin_init, GST_PACKAGE_VERSION, GST_PACKAGE_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN);
