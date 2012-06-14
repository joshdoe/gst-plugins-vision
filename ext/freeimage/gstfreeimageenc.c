/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
/**
 * SECTION:element-freeimageenc
 *
 * Encodes image types supported by FreeImage.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "gstfreeimageenc.h"
#include "gstfreeimageutils.h"

GST_DEBUG_CATEGORY_EXTERN (freeimageenc_debug);
#define GST_CAT_DEFAULT freeimageenc_debug

typedef struct
{
  FREE_IMAGE_FORMAT fif;
} GstFreeImageEncClassData;

static void gst_freeimageenc_class_init (GstFreeImageEncClass * klass,
    GstFreeImageEncClassData * class_data);
static void gst_freeimageenc_init (GstFreeImageEnc * freeimageenc);

static gboolean gst_freeimageenc_sink_activate_push (GstPad * sinkpad,
    gboolean active);
static gboolean gst_freeimageenc_sink_activate_pull (GstPad * sinkpad,
    gboolean active);
static gboolean gst_freeimageenc_sink_activate (GstPad * sinkpad);
static GstFlowReturn gst_freeimageenc_chain (GstPad * pad, GstBuffer * buffer);
static gboolean gst_freeimageenc_sink_event (GstPad * pad, GstEvent * event);
static gboolean gst_freeimageenc_sink_setcaps (GstPad * pad, GstCaps * caps);

static void gst_freeimageenc_task (GstPad * pad);

static gboolean gst_freeimageenc_freeimage_init (GstFreeImageEnc *
    freeimageenc);
static gboolean gst_freeimageenc_freeimage_clear (GstFreeImageEnc *
    freeimageenc);
static GstFlowReturn gst_freeimageenc_push_dib (GstFreeImageEnc * freeimageenc);

static GstElementClass *parent_class = NULL;

void DLL_CALLCONV
gst_freeimageenc_user_error (FREE_IMAGE_FORMAT fif, const char *message)
{
  GST_ERROR ("%s", message);
}

static int DLL_CALLCONV
gst_freeimageenc_user_seek (fi_handle handle, long offset, int origin)
{
  GstFreeImageEnc *freeimageenc = GST_FREEIMAGEENC (handle);

  switch (origin) {
    case SEEK_SET:
      freeimageenc->offset = offset;
      break;
    case SEEK_CUR:
      freeimageenc->offset += offset;
      break;
    case SEEK_END:
      freeimageenc->offset = freeimageenc->length + offset;
      break;
  }
  return 0;
}

static long DLL_CALLCONV
gst_freeimageenc_user_tell (fi_handle handle)
{
  GstFreeImageEnc *freeimageenc = GST_FREEIMAGEENC (handle);

  return freeimageenc->offset;
}

static void
gst_freeimageenc_class_init (GstFreeImageEncClass * klass,
    GstFreeImageEncClassData * class_data)
{
  GstElementClass *gstelement_class;
  GstCaps *caps;
  GstPadTemplate *templ;
  const gchar *mimetype;
  const gchar *format;
  const gchar *format_description;
  const gchar *extensions;
  gchar *description;
  gchar *longname;

  klass->fif = class_data->fif;

  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  mimetype = FreeImage_GetFIFMimeType (klass->fif);
  format = FreeImage_GetFormatFromFIF (klass->fif);
  format_description = FreeImage_GetFIFDescription (klass->fif);
  extensions = FreeImage_GetFIFExtensionList (klass->fif);

  /* add src pad template from FIF mimetype */
  if (mimetype)
    caps = gst_caps_new_simple (mimetype, NULL);
  else
    caps = gst_caps_new_simple ("image/freeimage-unknown", NULL);
  templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  gst_element_class_add_pad_template (gstelement_class, templ);

  /* add sink pad template */
  caps = gst_freeimageutils_caps_from_freeimage_format (klass->fif);
  templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);
  gst_element_class_add_pad_template (gstelement_class, templ);

  /* set details */
  longname = g_strdup_printf ("FreeImage %s image encoder", format);
  description = g_strdup_printf ("Encode %s (%s) images",
      format_description, extensions);
  gst_element_class_set_details_simple (gstelement_class, longname,
      "Codec/Encoder/Image",
      description, "Joshua M. Doe <oss@nvl.army.mil>");
  g_free (longname);
  g_free (description);
}

static void
gst_freeimageenc_init (GstFreeImageEnc * freeimageenc)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (freeimageenc);

  freeimageenc->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "sink"), "sink");
  gst_pad_set_chain_function (freeimageenc->sinkpad, gst_freeimageenc_chain);
  gst_pad_set_setcaps_function (freeimageenc->sinkpad,
      gst_freeimageenc_sink_setcaps);
  gst_element_add_pad (GST_ELEMENT (freeimageenc), freeimageenc->sinkpad);

  freeimageenc->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "src"), "src");
  gst_pad_use_fixed_caps (freeimageenc->srcpad);
  gst_element_add_pad (GST_ELEMENT (freeimageenc), freeimageenc->srcpad);

  freeimageenc->setup = FALSE;

  freeimageenc->in_timestamp = GST_CLOCK_TIME_NONE;
  freeimageenc->in_duration = GST_CLOCK_TIME_NONE;

  freeimageenc->fps_n = 0;
  freeimageenc->fps_d = 1;

  /* Set user IO functions to FreeImageIO struct */
  freeimageenc->fiio.read_proc = NULL;
  freeimageenc->fiio.write_proc = NULL;
  freeimageenc->fiio.seek_proc = gst_freeimageenc_user_seek;
  freeimageenc->fiio.tell_proc = gst_freeimageenc_user_tell;
}

static GstFlowReturn
gst_freeimageenc_chain (GstPad * pad, GstBuffer * buffer)
{
  GstFreeImageEnc *freeimageenc;
  GstFreeImageEncClass *klass;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer_out;
  FIMEMORY *hmem = NULL;
  gint srcPitch, dstPitch;
  guint8 *pSrc, *pDst;
  gint width, height, bpp;
  size_t y;
  BYTE *mem_buffer;
  DWORD size_in_bytes;

  freeimageenc = GST_FREEIMAGEENC (gst_pad_get_parent (pad));
  klass = GST_FREEIMAGEENC_GET_CLASS (freeimageenc);

  GST_LOG_OBJECT (freeimageenc, "Got buffer, size=%u",
      GST_BUFFER_SIZE (buffer));

  /* convert raw buffer to FIBITMAP */
  width = FreeImage_GetWidth (freeimageenc->dib);
  height = FreeImage_GetHeight (freeimageenc->dib);
  bpp = FreeImage_GetBPP (freeimageenc->dib);

  dstPitch = FreeImage_GetPitch (freeimageenc->dib);
  srcPitch = GST_ROUND_UP_4 (width * bpp / 8);

  /* Copy data, invert scanlines and respect FreeImage pitch */
  pDst = FreeImage_GetBits (freeimageenc->dib);
  for (y = 0; y < height; ++y) {
    pSrc = GST_BUFFER_DATA (buffer) + (height - y - 1) * srcPitch;
    memcpy (pDst, pSrc, srcPitch);
    pDst += dstPitch;
  }

  /* open memory stream */
  hmem = FreeImage_OpenMemory (0, 0);

  /* encode raw image to memory */
  if (!FreeImage_SaveToMemory (klass->fif, freeimageenc->dib, hmem, 0)) {
    GST_ERROR ("Failed to encode image");
    FreeImage_CloseMemory (hmem);
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  if (!FreeImage_AcquireMemory (hmem, &mem_buffer, &size_in_bytes)) {
    GST_ERROR ("Failed to acquire encoded image");
    FreeImage_CloseMemory (hmem);
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  buffer_out = gst_buffer_new_and_alloc (size_in_bytes);

  /* copy compressed image to buffer */
  memcpy (GST_BUFFER_DATA (buffer_out), mem_buffer, size_in_bytes);

  FreeImage_CloseMemory (hmem);

  gst_buffer_copy_metadata (buffer_out, buffer, GST_BUFFER_COPY_TIMESTAMPS);
  gst_buffer_unref (buffer);
  gst_buffer_set_caps (buffer, GST_PAD_CAPS (freeimageenc->srcpad));

  if ((ret = gst_pad_push (freeimageenc->srcpad, buffer_out)) != GST_FLOW_OK)
    goto done;

  //if (pngenc->snapshot) {
  //  GstEvent *event;

  //  GST_DEBUG_OBJECT (pngenc, "snapshot mode, sending EOS");
  //  /* send EOS event, since a frame has been pushed out */
  //  event = gst_event_new_eos ();

  //  gst_pad_push_event (pngenc->srcpad, event);
  //  ret = GST_FLOW_UNEXPECTED;
  //}

done:
  GST_DEBUG_OBJECT (freeimageenc, "END, ret:%d", ret);

  if (buffer_out != NULL) {
    gst_buffer_unref (buffer_out);
    buffer_out = NULL;
  }

  gst_object_unref (freeimageenc);
  return ret;
}

static gboolean
gst_freeimageenc_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstFreeImageEnc *freeimageenc;
  FREE_IMAGE_TYPE type;
  gint width, height, bpp;
  gint red_mask, green_mask, blue_mask;

  freeimageenc = GST_FREEIMAGEENC (gst_pad_get_parent (pad));

  if (gst_freeimageutils_parse_caps (caps, &type, &width, &height, &bpp,
          &red_mask, &green_mask, &blue_mask) == FALSE) {
    GST_DEBUG ("Failed to parse caps");
    return FALSE;
  }

  if (freeimageenc->dib) {
    FreeImage_Unload (freeimageenc->dib);
    freeimageenc->dib = NULL;
  }

  freeimageenc->dib = FreeImage_AllocateT (type, width, height, bpp,
      red_mask, green_mask, blue_mask);

  if (freeimageenc == NULL) {
    GST_DEBUG ("Failed to allocate memory for DIB");
    return FALSE;
  }

  gst_object_unref (freeimageenc);
  return TRUE;
}

/* Clean up the freeimage structures */
static gboolean
gst_freeimageenc_freeimage_clear (GstFreeImageEnc * freeimageenc)
{
  GST_LOG ("cleaning up freeimage structures");

  if (freeimageenc->dib) {
    FreeImage_Unload (freeimageenc->dib);
    freeimageenc->dib = NULL;
  }

  freeimageenc->in_timestamp = GST_CLOCK_TIME_NONE;
  freeimageenc->in_duration = GST_CLOCK_TIME_NONE;

  freeimageenc->setup = FALSE;

  return TRUE;
}

static gboolean
gst_freeimageenc_freeimage_init (GstFreeImageEnc * freeimageenc)
{
  if (freeimageenc->setup)
    return TRUE;

  GST_LOG ("init freeimage");

  freeimageenc->setup = TRUE;

  return TRUE;
}

gboolean
gst_freeimageenc_register_plugin (GstPlugin * plugin, FREE_IMAGE_FORMAT fif)
{
  GTypeInfo typeinfo = {
    sizeof (GstFreeImageEncClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_freeimageenc_class_init,
    NULL,
    NULL,
    sizeof (GstFreeImageEnc),
    0,
    (GInstanceInitFunc) gst_freeimageenc_init
  };
  GType type;
  gchar *type_name, *tmp;
  GstFreeImageEncClassData *class_data;
  gboolean ret = FALSE;

  const gchar *format = FreeImage_GetFormatFromFIF (fif);
  if (format == NULL) {
    GST_WARNING ("Specified format not supported by FreeImage");
    return FALSE;
  }

  tmp = g_strdup_printf ("fienc_%s", format);
  type_name = g_ascii_strdown (tmp, -1);
  g_free (tmp);
  g_strcanon (type_name, G_CSET_A_2_Z G_CSET_a_2_z G_CSET_DIGITS "-+_", '-');

  GST_LOG ("Trying to use name %s", type_name);

  if (g_type_from_name (type_name)) {
    GST_WARNING ("Type '%s' already exists", type_name);
    return FALSE;
  }

  class_data = g_new0 (GstFreeImageEncClassData, 1);
  class_data->fif = fif;
  typeinfo.class_data = class_data;

  type = g_type_register_static (GST_TYPE_ELEMENT, type_name, &typeinfo, 0);
  ret = gst_element_register (plugin, type_name, GST_RANK_NONE, type);

  g_free (type_name);
  return ret;
}

gboolean
gst_freeimageenc_register_plugins (GstPlugin * plugin)
{
  gint i;
  gint nloaded = 0;

  GST_LOG ("FreeImage indicates there are %d formats supported",
      FreeImage_GetFIFCount ());

  for (i = 0; i < FreeImage_GetFIFCount (); i++) {
    if (FreeImage_FIFSupportsWriting ((FREE_IMAGE_FORMAT) i)) {
      if (gst_freeimageenc_register_plugin (plugin,
              (FREE_IMAGE_FORMAT) i) == TRUE)
        nloaded += 1;
    }
  }

  if (nloaded)
    return TRUE;
  else
    return FALSE;
}
