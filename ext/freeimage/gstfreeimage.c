/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *
 * Filter:
 * Copyright (C) 2000 Donald A. Graft
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gst/gst.h>

#include "gstfreeimagedec.h"
#include "gstfreeimageenc.h"


GST_DEBUG_CATEGORY (freeimagedec_debug);
GST_DEBUG_CATEGORY (freeimageenc_debug);

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (freeimagedec_debug, "freeimagedec", 0,
      "FreeImage image decoder");
  GST_DEBUG_CATEGORY_INIT (freeimageenc_debug, "freeimageenc", 0,
      "FreeImage image encoder");

  if (!gst_freeimagedec_register_plugins (plugin))
    return FALSE;

  if (!gst_freeimageenc_register_plugins (plugin))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "freeimage",
    "FreeImage plugin library", plugin_init, VERSION, "LGPL", PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
