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
 * SECTION:element-freeimagedec
 *
 * Decodes image types supported by FreeImage. If there is no framerate set on sink caps, it sends EOS
 * after the first picture.
 *
 * Due to the nature of FreeImage, these decoders can only be used with
 * filesrc (for a single image) or multifilesrc (for multiple images). This
 * of course means it cannot be used with streaming sources such as HTTP.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstfreeimagedec.h"

#include <stdlib.h>
#include <string.h>
#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC (freeimagedec_debug);
#define GST_CAT_DEFAULT freeimagedec_debug

static void gst_freeimagedec_base_init (gpointer g_class);
static void gst_freeimagedec_class_init (GstFreeImageDecClass * klass);
static void gst_freeimagedec_init (GstFreeImageDec * freeimagedec);

static GstStateChangeReturn gst_freeimagedec_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_freeimagedec_sink_activate_push (GstPad * sinkpad,
    gboolean active);
static gboolean gst_freeimagedec_sink_activate_pull (GstPad * sinkpad,
    gboolean active);
static gboolean gst_freeimagedec_sink_activate (GstPad * sinkpad);
static GstFlowReturn gst_freeimagedec_chain (GstPad * pad, GstBuffer * buffer);
static gboolean gst_freeimagedec_sink_event (GstPad * pad, GstEvent * event);
static gboolean gst_freeimagedec_sink_setcaps (GstPad * pad, GstCaps * caps);

static void gst_freeimagedec_task (GstPad * pad);

static gboolean gst_freeimagedec_freeimage_init (GstFreeImageDec * freeimagedec);
static gboolean gst_freeimagedec_freeimage_clear (GstFreeImageDec * freeimagedec);
static GstFlowReturn gst_freeimagedec_caps_create_and_set (GstFreeImageDec * freeimagedec);
static GstFlowReturn gst_freeimagedec_push_dib (GstFreeImageDec * freeimagedec);

static GstElementClass *parent_class = NULL;

GType
gst_freeimagedec_get_type (void)
{
  static GType freeimagedec_type = 0;

  if (!freeimagedec_type) {
    static const GTypeInfo freeimagedec_info = {
      sizeof (GstFreeImageDecClass),
      gst_freeimagedec_base_init,
      NULL,
      (GClassInitFunc) gst_freeimagedec_class_init,
      NULL,
      NULL,
      sizeof (GstFreeImageDec),
      0,
      (GInstanceInitFunc) gst_freeimagedec_init,
    };

    freeimagedec_type = g_type_register_static (GST_TYPE_ELEMENT, "GstFreeImageDec",
        &freeimagedec_info, 0);
  }
  return freeimagedec_type;
}

static GstStaticPadTemplate gst_freeimagedec_src_pad_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
      GST_VIDEO_CAPS_RGB_15 ";"
      GST_VIDEO_CAPS_RGB_16 ";"
      GST_VIDEO_CAPS_RGB ";"
      GST_VIDEO_CAPS_BGR ";"
      GST_VIDEO_CAPS_RGBA ";"
      GST_VIDEO_CAPS_BGRA ";"
      "video/x-raw-gray, "                                            \
      "bpp = (int) 16, "                                              \
      "depth = (int) 16, "                                            \
      "endianness = (int) BIG_ENDIAN, "                           \
      "width = " GST_VIDEO_SIZE_RANGE ", "                            \
      "height = " GST_VIDEO_SIZE_RANGE ", "                           \
      "framerate = " GST_VIDEO_FPS_RANGE ";"
      "video/x-raw-gray, "                                            \
      "bpp = (int) 16, "                                              \
      "depth = (int) 16, "                                            \
      "endianness = (int) LITTLE_ENDIAN, "                           \
      "width = " GST_VIDEO_SIZE_RANGE ", "                            \
      "height = " GST_VIDEO_SIZE_RANGE ", "                           \
      "framerate = " GST_VIDEO_FPS_RANGE ";")
    );

static GstStaticPadTemplate gst_freeimagedec_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/tiff")
    );

void DLL_CALLCONV
user_error (FREE_IMAGE_FORMAT fif, const char *message)
{
  GST_ERROR ("%s", message);
}

static int DLL_CALLCONV
user_seek (fi_handle handle, long offset, int origin)
{
  GstFreeImageDec *freeimagedec = GST_FREEIMAGEDEC (handle);
  
  switch (origin) {
    case SEEK_SET:
      freeimagedec->offset = offset;
      break;
    case SEEK_CUR:
      freeimagedec->offset += offset;
      break;
    case SEEK_END:
      freeimagedec->offset = freeimagedec->length + offset;
      break;
  }
  return 0;
}

static long DLL_CALLCONV
user_tell (fi_handle handle)
{
  GstFreeImageDec *freeimagedec = GST_FREEIMAGEDEC (handle);

  return freeimagedec->offset;
}

unsigned DLL_CALLCONV
user_read (void *data, unsigned elsize, unsigned elcount, fi_handle handle)
{
  GstFreeImageDec *freeimagedec;
  GstBuffer *buffer;
  GstFlowReturn ret = GST_FLOW_OK;
  guint size;
  guint length = elsize * elcount;

  freeimagedec = GST_FREEIMAGEDEC (handle);

  GST_LOG ("reading %" G_GSIZE_FORMAT " bytes of data at offset %d", length,
      freeimagedec->offset);

  ret = gst_pad_pull_range (freeimagedec->sinkpad, freeimagedec->offset, length, &buffer);
  if (ret != GST_FLOW_OK)
    goto pause;

  size = GST_BUFFER_SIZE (buffer);

  if (size != length)
    goto short_buffer;

  memcpy (data, GST_BUFFER_DATA (buffer), size);

  gst_buffer_unref (buffer);

  freeimagedec->offset += length;

  return size;

  /* ERRORS */
pause:
  {
    GST_INFO_OBJECT (freeimagedec, "pausing task, reason %s",
        gst_flow_get_name (ret));
    gst_pad_pause_task (freeimagedec->sinkpad);
    if (GST_FLOW_IS_FATAL (ret) || ret == GST_FLOW_NOT_LINKED) {
      GST_ELEMENT_ERROR (freeimagedec, STREAM, FAILED,
          (("Internal data stream error.")),
          ("stream stopped, reason %s", gst_flow_get_name (ret)));
      gst_pad_push_event (freeimagedec->srcpad, gst_event_new_eos ());
    }
    return 0;
  }
short_buffer:
  {
    gst_buffer_unref (buffer);
    GST_ELEMENT_ERROR (freeimagedec, STREAM, FAILED,
        (("Internal data stream error.")),
        ("Read %u, needed %" G_GSIZE_FORMAT "bytes", size, length));
    ret = GST_FLOW_ERROR;
    goto pause;
  }
}

static void
gst_freeimagedec_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_add_pad_template (element_class,
    gst_static_pad_template_get (&gst_freeimagedec_src_pad_template));
  gst_element_class_add_pad_template (element_class,
    gst_static_pad_template_get (&gst_freeimagedec_sink_pad_template));
  gst_element_class_set_details_simple (element_class, "FreeImage image decoder",
    "Codec/Decoder/Image",
    "Decode a FreeImage supported video frame to a raw image",
    "Joshua M. Doe <oss@nvl.army.mil>");
}

static void
gst_freeimagedec_class_init (GstFreeImageDecClass * klass)
{
  GstElementClass *gstelement_class;

  gstelement_class = (GstElementClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gstelement_class->change_state = gst_freeimagedec_change_state;

  GST_DEBUG_CATEGORY_INIT (freeimagedec_debug, "freeimagedec", 0, "PNG image decoder");
}

static void
gst_freeimagedec_init (GstFreeImageDec * freeimagedec)
{
  freeimagedec->sinkpad =
      gst_pad_new_from_static_template (&gst_freeimagedec_sink_pad_template, "sink");
  gst_pad_set_activate_function (freeimagedec->sinkpad, gst_freeimagedec_sink_activate);
  gst_pad_set_activatepull_function (freeimagedec->sinkpad,
      gst_freeimagedec_sink_activate_pull);
  gst_pad_set_activatepush_function (freeimagedec->sinkpad,
      gst_freeimagedec_sink_activate_push);
  gst_pad_set_chain_function (freeimagedec->sinkpad, gst_freeimagedec_chain);
  gst_pad_set_event_function (freeimagedec->sinkpad, gst_freeimagedec_sink_event);
  gst_pad_set_setcaps_function (freeimagedec->sinkpad, gst_freeimagedec_sink_setcaps);
  gst_element_add_pad (GST_ELEMENT (freeimagedec), freeimagedec->sinkpad);

  freeimagedec->srcpad =
    gst_pad_new_from_static_template (&gst_freeimagedec_src_pad_template, "src");
  gst_pad_use_fixed_caps (freeimagedec->srcpad);
  gst_element_add_pad (GST_ELEMENT (freeimagedec), freeimagedec->srcpad);

  freeimagedec->setup = FALSE;

  freeimagedec->in_timestamp = GST_CLOCK_TIME_NONE;
  freeimagedec->in_duration = GST_CLOCK_TIME_NONE;

  freeimagedec->width = -1;
  freeimagedec->height = -1;
  freeimagedec->bpp = -1;
  freeimagedec->fps_n = 0;
  freeimagedec->fps_d = 1;

  gst_segment_init (&freeimagedec->segment, GST_FORMAT_UNDEFINED);

  /* Set user IO functions to FreeImageIO struct */
  freeimagedec->fiio.read_proc = user_read;
  freeimagedec->fiio.write_proc = NULL;
  freeimagedec->fiio.seek_proc = user_seek;
  freeimagedec->fiio.tell_proc = user_tell;
}

static GstFlowReturn
gst_freeimagedec_caps_create_and_set (GstFreeImageDec * freeimagedec)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstCaps *caps = NULL, *res = NULL;
  GstPadTemplate *templ = NULL;
  gint image_type, video_format = -1, endianness;

  g_return_val_if_fail (GST_IS_FREEIMAGEDEC (freeimagedec), GST_FLOW_ERROR);

  /* Get bits per channel */
  freeimagedec->bpp = FreeImage_GetBPP (freeimagedec->dib);

  /* Get image type */
  image_type = FreeImage_GetImageType (freeimagedec->dib);

  /* Get width and height */
  freeimagedec->width = FreeImage_GetWidth (freeimagedec->dib);
  freeimagedec->height = FreeImage_GetHeight (freeimagedec->dib);

  GST_LOG ("Image_type=%d, %dx%dx%d", image_type, freeimagedec->width, freeimagedec->height, freeimagedec->bpp);

  switch (image_type) {
    case FIT_BITMAP:
      if (freeimagedec->bpp < 16) {
        GST_ELEMENT_ERROR (freeimagedec, STREAM, NOT_IMPLEMENTED, (NULL),
            ("freeimagedec does not support palletised bitmaps yet"));
      }
      else if (freeimagedec->bpp == 24) {
        if (FreeImage_GetRedMask (freeimagedec->dib) == GST_VIDEO_BYTE1_MASK_24_INT &&
            FreeImage_GetGreenMask (freeimagedec->dib) == GST_VIDEO_BYTE2_MASK_24_INT &&
            FreeImage_GetBlueMask (freeimagedec->dib) == GST_VIDEO_BYTE3_MASK_24_INT) {
          video_format = GST_VIDEO_FORMAT_RGB;
        }
        else if (FreeImage_GetRedMask (freeimagedec->dib) == GST_VIDEO_BYTE3_MASK_24_INT &&
            FreeImage_GetGreenMask (freeimagedec->dib) == GST_VIDEO_BYTE2_MASK_24_INT &&
            FreeImage_GetBlueMask (freeimagedec->dib) == GST_VIDEO_BYTE1_MASK_24_INT) {
          video_format = GST_VIDEO_FORMAT_BGR;
        }
        else {
          GST_ELEMENT_ERROR (freeimagedec, STREAM, NOT_IMPLEMENTED, (NULL),
              ("freeimagedec only supports RGB or BGR 24-bit bitmaps"));
        }
      }
      else if (freeimagedec->bpp == 32) {
        if (FreeImage_GetRedMask (freeimagedec->dib) == GST_VIDEO_BYTE1_MASK_32_INT &&
            FreeImage_GetGreenMask (freeimagedec->dib) == GST_VIDEO_BYTE2_MASK_32_INT &&
            FreeImage_GetBlueMask (freeimagedec->dib) == GST_VIDEO_BYTE3_MASK_32_INT) {
            video_format = GST_VIDEO_FORMAT_RGBA;
        }
        else if (FreeImage_GetRedMask (freeimagedec->dib) == GST_VIDEO_BYTE3_MASK_32_INT &&
            FreeImage_GetGreenMask (freeimagedec->dib) == GST_VIDEO_BYTE2_MASK_32_INT &&
            FreeImage_GetBlueMask (freeimagedec->dib) == GST_VIDEO_BYTE1_MASK_32_INT) {
            video_format = GST_VIDEO_FORMAT_BGRA;
        }
        else {
          GST_ELEMENT_ERROR (freeimagedec, STREAM, NOT_IMPLEMENTED, (NULL),
            ("freeimagedec only supports RGBA or BGRA 32-bit bitmaps"));
        }
      }
      else {
        GST_ELEMENT_ERROR (freeimagedec, STREAM, NOT_IMPLEMENTED, (NULL),
            ("freeimagedec does not support this image type"));
      }

      /* We could not find a supported format */
      if (video_format == -1) {
        ret = GST_FLOW_NOT_SUPPORTED;
        goto beach;
      }

      caps = gst_video_format_new_caps (video_format, freeimagedec->width,
          freeimagedec->height, freeimagedec->fps_n, freeimagedec->fps_d, 1, 1);
      break;
    case FIT_UINT16:
      if (FreeImage_IsLittleEndian ())
        endianness = 1234;
      else
        endianness = 4321;
      caps = gst_caps_new_simple ("video/x-raw-gray",
          "width", G_TYPE_INT, freeimagedec->width,
          "height", G_TYPE_INT, freeimagedec->height,
          "bpp", G_TYPE_INT, 16,
          "depth", G_TYPE_INT, 16,
          "endianness", G_TYPE_INT, endianness,
          "framerate", GST_TYPE_FRACTION, freeimagedec->fps_n, freeimagedec->fps_d,
          NULL);
      break;
    case FIT_INT16:
      if (FreeImage_IsLittleEndian ())
        endianness = 1234;
      else
        endianness = 4321;
      caps = gst_caps_new_simple ("video/x-raw-gray",
          "width", G_TYPE_INT, freeimagedec->width,
          "height", G_TYPE_INT, freeimagedec->height,
          "bpp", G_TYPE_INT, 16,
          "depth", G_TYPE_INT, 16,
          "endianness", G_TYPE_INT, endianness,
          "framerate", GST_TYPE_FRACTION, freeimagedec->fps_n, freeimagedec->fps_d,
          "signed", G_TYPE_BOOLEAN, TRUE,
          NULL);
      break;
    default:
      GST_ELEMENT_ERROR (freeimagedec, STREAM, NOT_IMPLEMENTED, (NULL),
          ("freeimagedec does not support this image type"));
      ret = GST_FLOW_NOT_SUPPORTED;
      goto beach;
  }

  if (!gst_pad_set_caps (freeimagedec->srcpad, caps))
    ret = GST_FLOW_NOT_NEGOTIATED;

  gst_caps_unref (caps);

  /* Push a newsegment event */
  if (freeimagedec->need_newsegment) {
    gst_pad_push_event (freeimagedec->srcpad,
        gst_event_new_new_segment (FALSE, 1.0, GST_FORMAT_TIME, 0, -1, 0));
    freeimagedec->need_newsegment = FALSE;
  }

beach:
  return ret;
}

static void
gst_freeimagedec_task (GstPad * pad)
{
  GstFreeImageDec *freeimagedec;
  GstFlowReturn ret = GST_FLOW_OK;
  FREE_IMAGE_FORMAT imagetype;
  GstFormat format;

  freeimagedec = GST_FREEIMAGEDEC (GST_OBJECT_PARENT (pad));

  GST_LOG_OBJECT (freeimagedec, "read frame");

  /* Query length of file for use by user_seek (SEEK_END) */
  format = GST_FORMAT_BYTES;
  gst_pad_query_peer_duration (pad, &format, &freeimagedec->length);

  imagetype = FreeImage_GetFileTypeFromHandle (&freeimagedec->fiio, freeimagedec, 0);
  freeimagedec->dib = FreeImage_LoadFromHandle (imagetype, &freeimagedec->fiio,
      freeimagedec, 0);

  ret = gst_freeimagedec_push_dib (freeimagedec);
  if (ret != GST_FLOW_OK)
    goto pause;

  /* And we are done */
  gst_pad_pause_task (freeimagedec->sinkpad);
  gst_pad_push_event (freeimagedec->srcpad, gst_event_new_eos ());
  return;

pause:
  {
    GST_INFO_OBJECT (freeimagedec, "pausing task, reason %s",
      gst_flow_get_name (ret));
    gst_pad_pause_task (freeimagedec->sinkpad);
    if (GST_FLOW_IS_FATAL (ret) || ret == GST_FLOW_NOT_LINKED) {
      GST_ELEMENT_ERROR (freeimagedec, STREAM, FAILED,
        ("Internal data stream error."),
        ("stream stopped, reason %s", gst_flow_get_name (ret)));
      gst_pad_push_event (freeimagedec->srcpad, gst_event_new_eos ());
    }
  }
}

static GstFlowReturn
gst_freeimagedec_chain (GstPad * pad, GstBuffer * buffer)
{
  GstFreeImageDec *freeimagedec;
  GstFlowReturn ret = GST_FLOW_OK;
  FIMEMORY * fimem;
  FREE_IMAGE_FORMAT format;

  freeimagedec = GST_FREEIMAGEDEC (gst_pad_get_parent (pad));

  GST_LOG_OBJECT (freeimagedec, "Got buffer, size=%u", GST_BUFFER_SIZE (buffer));

  if (G_UNLIKELY (!freeimagedec->setup))
    goto not_configured;

  /* Return if we have bad flow conditions */
  ret = freeimagedec->ret;
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    GST_WARNING_OBJECT (freeimagedec, "we have a pending return code of %d", ret);
    goto beach;
  }

  /* Decode image to DIB */
  fimem = FreeImage_OpenMemory (GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));
  format = FreeImage_GetFileTypeFromMemory (fimem, 0);
  freeimagedec->dib = FreeImage_LoadFromMemory (format, fimem, 0);

  freeimagedec->in_timestamp = GST_BUFFER_TIMESTAMP (buffer);
  freeimagedec->in_duration = GST_BUFFER_DURATION (buffer);

  ret = gst_freeimagedec_push_dib (freeimagedec);
  if (ret != GST_FLOW_OK)
    goto beach;

  if (freeimagedec->framed) {
    /* Reset ourselves for the next frame */
    gst_freeimagedec_freeimage_clear (freeimagedec);
    gst_freeimagedec_freeimage_init (freeimagedec);
  } else {
    GST_LOG_OBJECT (freeimagedec, "sending EOS");
    freeimagedec->ret = gst_pad_push_event (freeimagedec->srcpad, gst_event_new_eos ());
  }

  /* grab new return code */
  ret = freeimagedec->ret;

  /* And release the buffer */
  gst_buffer_unref (buffer);

beach:
  gst_object_unref (freeimagedec);

  return ret;

  /* ERRORS */
not_configured:
  {
    GST_LOG_OBJECT (freeimagedec, "we are not configured yet");
    ret = GST_FLOW_WRONG_STATE;
    goto beach;
  }
}

static gboolean
gst_freeimagedec_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstStructure *s;
  GstFreeImageDec *freeimagedec;
  gint num, denom;

  freeimagedec = GST_FREEIMAGEDEC (gst_pad_get_parent (pad));

  s = gst_caps_get_structure (caps, 0);
  if (gst_structure_get_fraction (s, "framerate", &num, &denom)) {
    GST_DEBUG_OBJECT (freeimagedec, "framed input");
    freeimagedec->framed = TRUE;
    freeimagedec->fps_n = num;
    freeimagedec->fps_d = denom;
  } else {
    GST_DEBUG_OBJECT (freeimagedec, "single picture input");
    freeimagedec->framed = FALSE;
    freeimagedec->fps_n = 0;
    freeimagedec->fps_d = 1;
  }

  gst_object_unref (freeimagedec);
  return TRUE;
}

static gboolean
gst_freeimagedec_sink_event (GstPad * pad, GstEvent * event)
{
  GstFreeImageDec *freeimagedec;
  gboolean res;

  freeimagedec = GST_FREEIMAGEDEC (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:{
      gdouble rate, arate;
      gboolean update;
      gint64 start, stop, position;
      GstFormat fmt;

      gst_event_parse_new_segment_full (event, &update, &rate, &arate, &fmt,
          &start, &stop, &position);

      gst_segment_set_newsegment_full (&freeimagedec->segment, update, rate, arate,
          fmt, start, stop, position);

      GST_LOG_OBJECT (freeimagedec, "NEWSEGMENT (%s)", gst_format_get_name (fmt));

      if (fmt == GST_FORMAT_TIME) {
        freeimagedec->need_newsegment = FALSE;
        res = gst_pad_push_event (freeimagedec->srcpad, event);
      } else {
        gst_event_unref (event);
        res = TRUE;
      }
      break;
    }
    case GST_EVENT_FLUSH_STOP:
    {
      gst_freeimagedec_freeimage_clear (freeimagedec);
      gst_freeimagedec_freeimage_init (freeimagedec);
      freeimagedec->ret = GST_FLOW_OK;

      gst_segment_init (&freeimagedec->segment, GST_FORMAT_UNDEFINED);
      res = gst_pad_push_event (freeimagedec->srcpad, event);
      break;
    }
    case GST_EVENT_EOS:
    {
      GST_LOG_OBJECT (freeimagedec, "EOS");
      gst_freeimagedec_freeimage_clear (freeimagedec);
      freeimagedec->ret = GST_FLOW_UNEXPECTED;
      res = gst_pad_push_event (freeimagedec->srcpad, event);
      break;
    }
    default:
      res = gst_pad_push_event (freeimagedec->srcpad, event);
      break;
  }

  gst_object_unref (freeimagedec);
  return res;
}


/* Clean up the freeimage structures */
static gboolean
gst_freeimagedec_freeimage_clear (GstFreeImageDec * freeimagedec)
{
  g_return_val_if_fail (GST_IS_FREEIMAGEDEC (freeimagedec), FALSE);

  GST_LOG ("cleaning up freeimage structures");

  if (freeimagedec->dib) {
    FreeImage_Unload (freeimagedec->dib);
    freeimagedec->dib = NULL;
  }

  freeimagedec->in_timestamp = GST_CLOCK_TIME_NONE;
  freeimagedec->in_duration = GST_CLOCK_TIME_NONE;

  freeimagedec->bpp = freeimagedec->height = freeimagedec->width = -1;

  freeimagedec->setup = FALSE;

  return TRUE;
}

static gboolean
gst_freeimagedec_freeimage_init (GstFreeImageDec * freeimagedec)
{
  g_return_val_if_fail (GST_IS_FREEIMAGEDEC (freeimagedec), FALSE);

  if (freeimagedec->setup)
    return TRUE;

  GST_LOG ("init freeimage");

  freeimagedec->setup = TRUE;

  return TRUE;
}

static GstStateChangeReturn
gst_freeimagedec_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstFreeImageDec *freeimagedec;

  freeimagedec = GST_FREEIMAGEDEC (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_freeimagedec_freeimage_init (freeimagedec);
      freeimagedec->need_newsegment = TRUE;
      freeimagedec->framed = FALSE;
      freeimagedec->ret = GST_FLOW_OK;
      gst_segment_init (&freeimagedec->segment, GST_FORMAT_UNDEFINED);
      break;
    default:
      break;
  }

  ret = parent_class->change_state (element, transition);
  if (ret != GST_STATE_CHANGE_SUCCESS)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_freeimagedec_freeimage_clear (freeimagedec);
      break;
    default:
      break;
  }

  return ret;
}

/* this function gets called when we activate ourselves in push mode. */
static gboolean
gst_freeimagedec_sink_activate_push (GstPad * sinkpad, gboolean active)
{
  GstFreeImageDec *freeimagedec;

  freeimagedec = GST_FREEIMAGEDEC (GST_OBJECT_PARENT (sinkpad));

  freeimagedec->ret = GST_FLOW_OK;

  return TRUE;
}

/* this function gets called when we activate ourselves in pull mode.
 * We can perform random access to the resource and we start a task
 * to start reading */
static gboolean
gst_freeimagedec_sink_activate_pull (GstPad * sinkpad, gboolean active)
{
  if (active) {
    return gst_pad_start_task (sinkpad, (GstTaskFunction) gst_freeimagedec_task,
        sinkpad);
  } else {
    return gst_pad_stop_task (sinkpad);
  }
}

/* this function is called when the pad is activated and should start
 * processing data.
 *
 */
static gboolean
gst_freeimagedec_sink_activate (GstPad * sinkpad)
{
  if (gst_pad_check_pull_range (sinkpad)) {
    return gst_pad_activate_pull (sinkpad, TRUE);
  } else {
    return gst_pad_activate_push (sinkpad, TRUE);
  }
}


static GstFlowReturn
gst_freeimagedec_push_dib (GstFreeImageDec * freeimagedec)
{
  GstFlowReturn ret;
  GstBuffer *buffer = NULL;
  size_t buffer_size = 0;
  guint pitch;
  gint i;

  if (freeimagedec->dib == NULL)
    return GST_FLOW_UNEXPECTED;

  /* Generate the caps and configure */
  ret = gst_freeimagedec_caps_create_and_set (freeimagedec);
  if (ret != GST_FLOW_OK) {
    return ret;
  }

  /* Allocate output buffer */
  pitch = FreeImage_GetPitch (freeimagedec->dib);
  buffer_size = pitch * freeimagedec->height;

  GST_LOG ("Buffer size must be %d", buffer_size);

  ret = gst_pad_alloc_buffer_and_set_caps (freeimagedec->srcpad,
    GST_BUFFER_OFFSET_NONE, buffer_size,
    GST_PAD_CAPS (freeimagedec->srcpad), &buffer);
  if (ret != GST_FLOW_OK)
    return ret;

  /* flip image and copy to buffer */
  for (i = 0; i < freeimagedec->height; i++) {
    memcpy (GST_BUFFER_DATA (buffer) + i * pitch,
        FreeImage_GetBits (freeimagedec->dib) + (freeimagedec->height - i - 1) * pitch, pitch);
  }
  

  if (GST_CLOCK_TIME_IS_VALID (freeimagedec->in_timestamp))
    GST_BUFFER_TIMESTAMP (buffer) = freeimagedec->in_timestamp;
  if (GST_CLOCK_TIME_IS_VALID (freeimagedec->in_duration))
    GST_BUFFER_DURATION (buffer) = freeimagedec->in_duration;

  /* Push the raw frame */
  ret = gst_pad_push (freeimagedec->srcpad, buffer);

  return ret;
}