/*
 * GStreamer
 * Copyright (C) 2010 Thiago Santos <thiago.sousa.santos@collabora.co.uk>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstopencvutils.h"
#include "gstsensorfx3dnoise.h"

GST_DEBUG_CATEGORY_STATIC (gst_sfx3dnoise_debug);
#define GST_CAT_DEFAULT gst_sfx3dnoise_debug
#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_sfx3dnoise_debug, "sfx3dnoise", 0, "ARF 3D-noise sensor effects");

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};
enum
{
  PROP_0,
  PROP_SIGMA_T,
  PROP_SIGMA_V,
  PROP_SIGMA_H,
  PROP_SIGMA_TV,
  PROP_SIGMA_TH,
  PROP_SIGMA_VH,
  PROP_SIGMA_TVH
};

#define DEFAULT_SIGMA_T 0.0
#define DEFAULT_SIGMA_V 0.0
#define DEFAULT_SIGMA_H 0.0
#define DEFAULT_SIGMA_TV 0.0
#define DEFAULT_SIGMA_TH 0.0
#define DEFAULT_SIGMA_VH 0.0
#define DEFAULT_SIGMA_TVH 0.0

GST_BOILERPLATE_FULL (GstSfx3DNoise, gst_sfx3dnoise, GstOpencvBaseTransform,
    GST_TYPE_OPENCV_BASE_TRANSFORM, DEBUG_INIT);

static void gst_sfx3dnoise_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_sfx3dnoise_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_sfx3dnoise_cv_transform (GstOpencvBaseTransform *
    filter, GstBuffer * buf, IplImage * img, GstBuffer * outbuf,
    IplImage * outimg);
static gboolean gst_sfx3dnoise_cv_set_caps (GstOpencvBaseTransform * trans,
    gint in_width, gint in_height, gint in_depth, gint in_channels,
    gint out_width, gint out_height, gint out_depth, gint out_channels);

void gst_sfx3dnoise_create_fixed_noise (GstSfx3DNoise * filter, CvMat * arr);
void gst_sfx3dnoise_add_sigma_t (GstSfx3DNoise * filter, CvMat * arr,
    double sigma);
void gst_sfx3dnoise_add_sigma_tv (GstSfx3DNoise * filter, CvMat * arr,
    double sigma);
void gst_sfx3dnoise_add_sigma_th (GstSfx3DNoise * filter, CvMat * arr,
    double sigma);
void gst_sfx3dnoise_add_sigma_tvh (GstSfx3DNoise * filter, CvMat * arr,
    double sigma);

/* Clean up */
static void
gst_sfx3dnoise_finalize (GObject * obj)
{
  GstSfx3DNoise *filter = GST_SFX3DNOISE (obj);

  if (filter->fixed_noise)
    cvReleaseMat (&filter->fixed_noise);
  filter->fixed_noise = NULL;

  if (filter->intermediary)
    cvReleaseMat (&filter->intermediary);
  filter->intermediary = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/* GObject vmethod implementations */

static void
gst_sfx3dnoise_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);
  GstCaps *caps;
  GstPadTemplate *templ;

  /* add sink and source pad templates */
  caps = gst_caps_from_string (GST_VIDEO_CAPS_GRAY16 ("1234"));
  templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      gst_caps_ref (caps));
  gst_element_class_add_pad_template (element_class, templ);
  templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps);
  gst_element_class_add_pad_template (element_class, templ);

  gst_element_class_set_details_simple (element_class,
      "sfx3dnoise",
      "Transform/Effect/Video",
      "Add 3D noise to video", "Joshua M. Doe <oss@nvl.army.mil>");
}

static void
gst_sfx3dnoise_class_init (GstSfx3DNoiseClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *gstbasetransform_class;
  GstOpencvBaseTransformClass *gstopencvbasefilter_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;
  gstopencvbasefilter_class = (GstOpencvBaseTransformClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_sfx3dnoise_finalize);
  gobject_class->set_property = gst_sfx3dnoise_set_property;
  gobject_class->get_property = gst_sfx3dnoise_get_property;

  gstopencvbasefilter_class->cv_trans_func = gst_sfx3dnoise_cv_transform;
  gstopencvbasefilter_class->cv_set_caps = gst_sfx3dnoise_cv_set_caps;

  g_object_class_install_property (gobject_class, PROP_SIGMA_T,
      g_param_spec_double ("sigma-t", "sigma-t",
          "Adds	frame to frame noise or bounce (flicker)",
          0.0, 1.0, DEFAULT_SIGMA_T, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  g_object_class_install_property (gobject_class, PROP_SIGMA_V,
      g_param_spec_double ("sigma-v", "sigma-v",
          "Adds fixed row noise (horizontal lines)",
          0.0, 1.0, DEFAULT_SIGMA_T, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  g_object_class_install_property (gobject_class, PROP_SIGMA_H,
      g_param_spec_double ("sigma-h", "sigma-h",
          "Adds fixed column noise (vertical lines)",
          0.0, 1.0, DEFAULT_SIGMA_T, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  g_object_class_install_property (gobject_class, PROP_SIGMA_TV,
      g_param_spec_double ("sigma-tv", "sigma-tv",
          "Adds temporal row bounce (random horizontal lines)",
          0.0, 1.0, DEFAULT_SIGMA_T, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  g_object_class_install_property (gobject_class, PROP_SIGMA_TH,
      g_param_spec_double ("sigma-th", "sigma-th",
          "Adds temporal column bounce (random vertical lines)",
          0.0, 1.0, DEFAULT_SIGMA_T, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  g_object_class_install_property (gobject_class, PROP_SIGMA_VH,
      g_param_spec_double ("sigma-vh", "sigma-vh",
          "Adds random time-independent spatial noise (fixed pattern noise)",
          0.0, 1.0, DEFAULT_SIGMA_T, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );

  g_object_class_install_property (gobject_class, PROP_SIGMA_TVH,
      g_param_spec_double ("sigma-tvh", "sigma-tvh",
          "Adds random spatio-temporal noise",
          0.0, 1.0, DEFAULT_SIGMA_T, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
}

static void
gst_sfx3dnoise_init (GstSfx3DNoise * filter, GstSfx3DNoiseClass * gclass)
{
  GST_DEBUG ("Initializing");

  filter->sigma_t = DEFAULT_SIGMA_T;
  filter->sigma_v = filter->sigma_v_old = DEFAULT_SIGMA_V;
  filter->sigma_h = filter->sigma_h_old = DEFAULT_SIGMA_H;
  filter->sigma_tv = DEFAULT_SIGMA_TV;
  filter->sigma_th = DEFAULT_SIGMA_TH;
  filter->sigma_vh = filter->sigma_vh_old = DEFAULT_SIGMA_VH;
  filter->sigma_tvh = DEFAULT_SIGMA_TVH;

  filter->rng = cvRNG (-1);
  filter->fixed_noise = NULL;
  filter->intermediary = NULL;

  filter->width = 0;
  filter->height = 0;

  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (filter), TRUE);
}

static void
gst_sfx3dnoise_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSfx3DNoise *filter = GST_SFX3DNOISE (object);

  switch (prop_id) {
    case PROP_SIGMA_T:
      filter->sigma_t = g_value_get_double (value);
      break;
    case PROP_SIGMA_V:
      filter->sigma_v = g_value_get_double (value);
      break;
    case PROP_SIGMA_H:
      filter->sigma_h = g_value_get_double (value);
      break;
    case PROP_SIGMA_TV:
      filter->sigma_tv = g_value_get_double (value);
      break;
    case PROP_SIGMA_TH:
      filter->sigma_th = g_value_get_double (value);
      break;
    case PROP_SIGMA_VH:
      filter->sigma_vh = g_value_get_double (value);
      break;
    case PROP_SIGMA_TVH:
      filter->sigma_tvh = g_value_get_double (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_sfx3dnoise_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSfx3DNoise *filter = GST_SFX3DNOISE (object);

  switch (prop_id) {
    case PROP_SIGMA_T:
      g_value_set_double (value, filter->sigma_t);
      break;
    case PROP_SIGMA_V:
      g_value_set_double (value, filter->sigma_v);
      break;
    case PROP_SIGMA_H:
      g_value_set_double (value, filter->sigma_h);
      break;
    case PROP_SIGMA_TV:
      g_value_set_double (value, filter->sigma_tv);
      break;
    case PROP_SIGMA_TH:
      g_value_set_double (value, filter->sigma_th);
      break;
    case PROP_SIGMA_VH:
      g_value_set_double (value, filter->sigma_vh);
      break;
    case PROP_SIGMA_TVH:
      g_value_set_double (value, filter->sigma_tvh);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_sfx3dnoise_cv_transform (GstOpencvBaseTransform * base, GstBuffer * buf,
    IplImage * img, GstBuffer * outbuf, IplImage * outimg)
{
  GstSfx3DNoise *filter = GST_SFX3DNOISE (base);

  GST_DEBUG ("Transforming");

  cvConvert (img, filter->intermediary);

  if (filter->sigma_h != filter->sigma_h_old ||
      filter->sigma_v != filter->sigma_v_old ||
      filter->sigma_vh != filter->sigma_vh_old) {
    GST_DEBUG ("Creating new fixed pattern noise image");
    gst_sfx3dnoise_create_fixed_noise (filter, filter->fixed_noise);

    filter->sigma_h_old = filter->sigma_h;
    filter->sigma_v_old = filter->sigma_v;
    filter->sigma_vh_old = filter->sigma_vh;
  }

  if (filter->sigma_h != 0.0 || filter->sigma_v != 0.0
      || filter->sigma_vh != 0.0) {
    cvAdd (filter->intermediary, filter->fixed_noise, filter->intermediary, 0);
  }

  if (filter->sigma_tvh > 0.0) {
    gst_sfx3dnoise_add_sigma_tvh (filter, filter->intermediary,
        filter->sigma_tvh);
  }

  if (filter->sigma_tv > 0.0) {
    gst_sfx3dnoise_add_sigma_tv (filter, filter->intermediary,
        filter->sigma_tv);
  }

  if (filter->sigma_th > 0.0) {
    gst_sfx3dnoise_add_sigma_th (filter, filter->intermediary,
        filter->sigma_th);
  }

  if (filter->sigma_t > 0.0) {
    gst_sfx3dnoise_add_sigma_t (filter, filter->intermediary, filter->sigma_t);
  }

  cvConvert (filter->intermediary, outimg);

  return GST_FLOW_OK;
}

static gboolean
gst_sfx3dnoise_cv_set_caps (GstOpencvBaseTransform * trans, gint in_width,
    gint in_height, gint in_depth, gint in_channels, gint out_width,
    gint out_height, gint out_depth, gint out_channels)
{
  GstSfx3DNoise *filter = GST_SFX3DNOISE (trans);

  GST_DEBUG ("Caps have been set");

  filter->width = in_width;
  filter->height = in_height;

  if (filter->intermediary) {
    cvReleaseMat (&filter->intermediary);
    filter->intermediary = NULL;
  }

  if (out_depth != IPL_DEPTH_32F) {
    filter->intermediary = cvCreateMat (out_height, out_width, CV_32FC1);
  }

  if (filter->fixed_noise) {
    cvReleaseMat (&filter->fixed_noise);
    filter->fixed_noise = NULL;
  }
  filter->fixed_noise = cvCreateMat (out_height, out_width, CV_32FC1);
  gst_sfx3dnoise_create_fixed_noise (filter, filter->fixed_noise);



  /* TODO create fixed noise here */

  return TRUE;
}

gboolean
gst_sfx3dnoise_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG ("Initializing plugin sfx3dnoise");

  return gst_element_register (plugin, "sfx3dnoise", GST_RANK_NONE,
      GST_TYPE_SFX3DNOISE);
}

void
gst_sfx3dnoise_create_fixed_noise (GstSfx3DNoise * filter, CvMat * arr)
{
  cvZero (arr);
  gst_sfx3dnoise_add_sigma_tvh (filter, arr, filter->sigma_vh);
  gst_sfx3dnoise_add_sigma_th (filter, arr, filter->sigma_h);
  gst_sfx3dnoise_add_sigma_tv (filter, arr, filter->sigma_v);
}

/* Add sigma-vh or sigma-tvh noise, which adds random noise to every pixel */
void
gst_sfx3dnoise_add_sigma_tvh (GstSfx3DNoise * filter, CvMat * arr, double sigma)
{
  CvMat *noise = cvCreateMat (filter->height, filter->width, CV_32F);

  /* TODO move scaling of sigma somewhere else? */
  cvRandArr (&filter->rng, noise, CV_RAND_NORMAL, cvScalarAll (0),
      cvScalarAll (sigma * (G_MAXUINT16 - 1)));

  cvAdd (arr, noise, arr, 0);

  cvReleaseMat (&noise);
}


/* Add sigma-v or sigma-tv noise, which adds random horizontal lines */
void
gst_sfx3dnoise_add_sigma_tv (GstSfx3DNoise * filter, CvMat * arr, double sigma)
{
  CvMat *noise = cvCreateMat (1, filter->height, CV_32F);
  gfloat *data;
  gfloat *noisedata;
  int step, x, y;

  /* TODO move scaling of sigma somewhere else? */
  cvRandArr (&filter->rng, noise, CV_RAND_NORMAL, cvScalarAll (0),
      cvScalarAll (sigma * (G_MAXUINT16 - 1)));

  cvGetRawData (arr, (uchar **) & data, &step, NULL);
  cvGetRawData (noise, (uchar **) & noisedata, NULL, NULL);

  step /= sizeof (gfloat);

  for (y = 0; y < filter->height; y++) {
    for (x = 0; x < filter->width; x++) {
      data[y * step + x] += noisedata[y];
    }
  }

  cvReleaseMat (&noise);
}


/* Add sigma-h and sigma-th noise, which adds random vertical lines */
void
gst_sfx3dnoise_add_sigma_th (GstSfx3DNoise * filter, CvMat * arr, double sigma)
{
  CvMat *noise = cvCreateMat (1, filter->width, CV_32F);
  gfloat *data;
  gfloat *noisedata;
  int step, x, y;

  /* TODO move scaling of sigma somewhere else? */
  cvRandArr (&filter->rng, noise, CV_RAND_NORMAL, cvScalarAll (0),
      cvScalarAll (sigma * (G_MAXUINT16 - 1)));

  cvGetRawData (arr, (uchar **) & data, &step, NULL);
  cvGetRawData (noise, (uchar **) & noisedata, NULL, NULL);

  step /= sizeof (gfloat);

  for (y = 0; y < filter->height; y++) {
    for (x = 0; x < filter->width; x++) {
      data[y * step + x] += noisedata[x];
    }
  }

  cvReleaseMat (&noise);
}

/* Add Sigma_T noise, which creates a flashing effect */
void
gst_sfx3dnoise_add_sigma_t (GstSfx3DNoise * filter, CvMat * arr, double sigma)
{
  CvMat *noise = cvCreateMat (1, 1, CV_32F);
  float *data;

  /* TODO move scaling of sigma somewhere else? */
  cvRandArr (&filter->rng, noise, CV_RAND_NORMAL, cvScalarAll (0),
      cvScalarAll (sigma * (G_MAXUINT16 - 1)));

  cvGetRawData (noise, (uchar **) & data, NULL, NULL);

  cvAddS (arr, cvScalarAll (*data), arr, 0);

  cvReleaseMat (&noise);
}
