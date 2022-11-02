/* GStreamer
 * Copyright (C) 2022 FIXME <fixme@example.com>
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
/**
 * SECTION:element-gstyuvconvert
 *
 * The yuvconvert element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! yuvconvert ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gstyuvconvert.h"
#include "libyuv.h"

GST_DEBUG_CATEGORY_STATIC (gst_yuvconvert_debug_category);
#define GST_CAT_DEFAULT gst_yuvconvert_debug_category

/* prototypes */

static GQuark _colorspace_quark;

#define gst_yuvconvert_parent_class parent_class

static void gst_yuvconvert_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_yuvconvert_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_yuvconvert_dispose (GObject * object);
static void gst_yuvconvert_finalize (GObject * object);

static gboolean gst_yuvconvert_start (GstBaseTransform * trans);
static gboolean gst_yuvconvert_stop (GstBaseTransform * trans);
static gboolean gst_yuvconvert_set_info (GstVideoFilter * filter,
    GstCaps * incaps, GstVideoInfo * in_info, GstCaps * outcaps,
    GstVideoInfo * out_info);
static GstFlowReturn gst_yuvconvert_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe);

static GstCapsFeatures *features_format_interlaced,
					   *features_format_interlaced_sysmem;

enum
{
  PROP_0
};

/* pad templates */

/* FIXME: add/remove formats you can handle */
#define VIDEO_SRC_CAPS \
	GST_VIDEO_CAPS_MAKE("{ I420, BGRx }")

/* FIXME: add/remove formats you can handle */
#define VIDEO_SINK_CAPS \
	GST_VIDEO_CAPS_MAKE("{ I420, NV12 }")

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstYuvconvert, gst_yuvconvert, GST_TYPE_VIDEO_FILTER,
	GST_DEBUG_CATEGORY_INIT (gst_yuvconvert_debug_category, "yuvconvert", 0,
	"debug category for yuvconvert element"));

/* copies the given caps */
static GstCaps *
gst_yuvconvert_caps_remove_format_info (GstCaps * caps)
{
	GstStructure *st;
	gint i, n;
	GstCaps *res;

	res = gst_caps_new_empty ();

	n = gst_caps_get_size (caps);
	for (i = 0; i < n; i++) {
		st = gst_caps_get_structure (caps, i);

		/* If this is already expressed by the existing caps
		 * skip this structure */
		if (i > 0 && gst_caps_is_subset_structure (res, st))
			continue;

		st = gst_structure_copy (st);
		gst_structure_remove_fields (st, "format",
			"colorimetry", "chroma-site", NULL);

		gst_caps_append_structure (res, st);
	}

	return res;
}

	static GstCaps *
gst_yuvconvert_transform_caps (GstBaseTransform * trans,
	GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
	GstCaps *ret;
	GstCaps *tmp;
	GstStructure *structure;
	GstCapsFeatures *features;
	gint i, n;

	GST_DEBUG_OBJECT (trans,
		"Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
		(direction == GST_PAD_SINK) ? "sink" : "src");

	ret = gst_caps_new_empty ();
	n = gst_caps_get_size (caps);
	for (i = 0; i < n; i++) {
		structure = gst_caps_get_structure (caps, i);
		features = gst_caps_get_features (caps, i);

		/* If this is already expressed by the existing caps
		 * skip this structure */
		if (i > 0 && gst_caps_is_subset_structure_full (ret, structure, features))
			continue;

		/* make copy */
		structure = gst_structure_copy (structure);

		/* If the features are non-sysmem we can only do passthrough */
		if (!gst_caps_features_is_any (features)
			&& (gst_caps_features_is_equal (features,
			GST_CAPS_FEATURES_MEMORY_SYSTEM_MEMORY)
			|| gst_caps_features_is_equal (features, features_format_interlaced)
			|| gst_caps_features_is_equal (features,
			features_format_interlaced_sysmem))) {
			gst_structure_set (structure, "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
				"height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);

			/* if pixel aspect ratio, make a range of it */
			if (gst_structure_has_field (structure, "pixel-aspect-ratio")) {
				gst_structure_set (structure, "pixel-aspect-ratio",
					GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1, NULL);
			}
		}
		gst_caps_append_structure_full (ret, structure,
			gst_caps_features_copy (features));
	}

	tmp = gst_yuvconvert_caps_remove_format_info(ret);
	gst_caps_unref(ret);
	ret = tmp;

	if (filter) {
		GstCaps *intersection;

		intersection =
			gst_caps_intersect_full (filter, ret, GST_CAPS_INTERSECT_FIRST);
		gst_caps_unref (ret);
		ret = intersection;
	}

	GST_DEBUG_OBJECT (trans, "returning caps: %" GST_PTR_FORMAT, ret);

	return ret;
}

	static GstCaps *
gst_yuvconvert_fixate_caps (GstBaseTransform * base,
	GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
	GstStructure *ins, *outs;
	const GValue *from_par, *to_par;
	GValue fpar = { 0, };
	GValue tpar = { 0, };

	othercaps = gst_caps_truncate (othercaps);
	othercaps = gst_caps_make_writable (othercaps);

	GST_DEBUG_OBJECT (base, "trying to fixate othercaps %" GST_PTR_FORMAT
		" based on caps %" GST_PTR_FORMAT, othercaps, caps);

	ins = gst_caps_get_structure (caps, 0);
	outs = gst_caps_get_structure (othercaps, 0);

	from_par = gst_structure_get_value (ins, "pixel-aspect-ratio");
	to_par = gst_structure_get_value (outs, "pixel-aspect-ratio");

	/* If we're fixating from the sinkpad we always set the PAR and
	 * assume that missing PAR on the sinkpad means 1/1 and
	 * missing PAR on the srcpad means undefined
	 */
	if (direction == GST_PAD_SINK) {
		if (!from_par) {
			g_value_init (&fpar, GST_TYPE_FRACTION);
			gst_value_set_fraction (&fpar, 1, 1);
			from_par = &fpar;
		}
		if (!to_par) {
			g_value_init (&tpar, GST_TYPE_FRACTION_RANGE);
			gst_value_set_fraction_range_full (&tpar, 1, G_MAXINT, G_MAXINT, 1);
			to_par = &tpar;
		}
	} else {
		if (!to_par) {
			g_value_init (&tpar, GST_TYPE_FRACTION);
			gst_value_set_fraction (&tpar, 1, 1);
			to_par = &tpar;

			gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
				NULL);
		}
		if (!from_par) {
			g_value_init (&fpar, GST_TYPE_FRACTION);
			gst_value_set_fraction (&fpar, 1, 1);
			from_par = &fpar;
		}
	}

	/* we have both PAR but they might not be fixated */
	{
		gint from_w, from_h, from_par_n, from_par_d, to_par_n, to_par_d;
		gint w = 0, h = 0;
		gint from_dar_n, from_dar_d;
		gint num, den;

		/* from_par should be fixed */
		g_return_val_if_fail (gst_value_is_fixed (from_par), othercaps);

		from_par_n = gst_value_get_fraction_numerator (from_par);
		from_par_d = gst_value_get_fraction_denominator (from_par);

		gst_structure_get_int (ins, "width", &from_w);
		gst_structure_get_int (ins, "height", &from_h);

		gst_structure_get_int (outs, "width", &w);
		gst_structure_get_int (outs, "height", &h);

		/* if both width and height are already fixed, we can't do anything
		 * about it anymore */
		if (w && h) {
			guint n, d;

			GST_DEBUG_OBJECT (base, "dimensions already set to %dx%d, not fixating",
				w, h);
			if (!gst_value_is_fixed (to_par)) {
				if (gst_video_calculate_display_ratio (&n, &d, from_w, from_h,
					from_par_n, from_par_d, w, h)) {
					GST_DEBUG_OBJECT (base, "fixating to_par to %dx%d", n, d);
					if (gst_structure_has_field (outs, "pixel-aspect-ratio"))
						gst_structure_fixate_field_nearest_fraction (outs,
							"pixel-aspect-ratio", n, d);
					else if (n != d)
						gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
							n, d, NULL);
				}
			}
			goto done;
		}

		/* Calculate input DAR */
		if (!gst_util_fraction_multiply (from_w, from_h, from_par_n, from_par_d,
			&from_dar_n, &from_dar_d)) {
			GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
				("Error calculating the output scaled size - integer overflow"));
			goto done;
		}

		GST_DEBUG_OBJECT (base, "Input DAR is %d/%d", from_dar_n, from_dar_d);

		/* If either width or height are fixed there's not much we
		 * can do either except choosing a height or width and PAR
		 * that matches the DAR as good as possible
		 */
		if (h) {
			GstStructure *tmp;
			gint set_w, set_par_n, set_par_d;

			GST_DEBUG_OBJECT (base, "height is fixed (%d)", h);

			/* If the PAR is fixed too, there's not much to do
			 * except choosing the width that is nearest to the
			 * width with the same DAR */
			if (gst_value_is_fixed (to_par)) {
				to_par_n = gst_value_get_fraction_numerator (to_par);
				to_par_d = gst_value_get_fraction_denominator (to_par);

				GST_DEBUG_OBJECT (base, "PAR is fixed %d/%d", to_par_n, to_par_d);

				if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
					to_par_n, &num, &den)) {
					GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
						("Error calculating the output scaled size - integer overflow"));
					goto done;
				}

				w = (guint) gst_util_uint64_scale_int_round (h, num, den);
				gst_structure_fixate_field_nearest_int (outs, "width", w);

				goto done;
			}

			/* The PAR is not fixed and it's quite likely that we can set
			 * an arbitrary PAR. */

			/* Check if we can keep the input width */
			tmp = gst_structure_copy (outs);
			gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
			gst_structure_get_int (tmp, "width", &set_w);

			/* Might have failed but try to keep the DAR nonetheless by
			 * adjusting the PAR */
			if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, h, set_w,
				&to_par_n, &to_par_d)) {
				GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
					("Error calculating the output scaled size - integer overflow"));
				gst_structure_free (tmp);
				goto done;
			}

			if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
				gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
			gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
				to_par_n, to_par_d);
			gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
				&set_par_d);
			gst_structure_free (tmp);

			/* Check if the adjusted PAR is accepted */
			if (set_par_n == to_par_n && set_par_d == to_par_d) {
				if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
					set_par_n != set_par_d)
					gst_structure_set (outs, "width", G_TYPE_INT, set_w,
						"pixel-aspect-ratio", GST_TYPE_FRACTION, set_par_n, set_par_d,
						NULL);
				goto done;
			}

			/* Otherwise scale the width to the new PAR and check if the
			 * adjusted with is accepted. If all that fails we can't keep
			 * the DAR */
			if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
				set_par_n, &num, &den)) {
				GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
					("Error calculating the output scaled size - integer overflow"));
				goto done;
			}

			w = (guint) gst_util_uint64_scale_int_round (h, num, den);
			gst_structure_fixate_field_nearest_int (outs, "width", w);
			if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
				set_par_n != set_par_d)
				gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
					set_par_n, set_par_d, NULL);

			goto done;
		} else if (w) {
			GstStructure *tmp;
			gint set_h, set_par_n, set_par_d;

			GST_DEBUG_OBJECT (base, "width is fixed (%d)", w);

			/* If the PAR is fixed too, there's not much to do
			 * except choosing the height that is nearest to the
			 * height with the same DAR */
			if (gst_value_is_fixed (to_par)) {
				to_par_n = gst_value_get_fraction_numerator (to_par);
				to_par_d = gst_value_get_fraction_denominator (to_par);

				GST_DEBUG_OBJECT (base, "PAR is fixed %d/%d", to_par_n, to_par_d);

				if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_d,
					to_par_n, &num, &den)) {
					GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
						("Error calculating the output scaled size - integer overflow"));
					goto done;
				}

				h = (guint) gst_util_uint64_scale_int_round (w, den, num);
				gst_structure_fixate_field_nearest_int (outs, "height", h);

				goto done;
			}

			/* The PAR is not fixed and it's quite likely that we can set
			 * an arbitrary PAR. */

			/* Check if we can keep the input height */
			tmp = gst_structure_copy (outs);
			gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
			gst_structure_get_int (tmp, "height", &set_h);

			/* Might have failed but try to keep the DAR nonetheless by
			 * adjusting the PAR */
			if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_h, w,
				&to_par_n, &to_par_d)) {
				GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
					("Error calculating the output scaled size - integer overflow"));
				gst_structure_free (tmp);
				goto done;
			}
			if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
				gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
			gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
				to_par_n, to_par_d);
			gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
				&set_par_d);
			gst_structure_free (tmp);

			/* Check if the adjusted PAR is accepted */
			if (set_par_n == to_par_n && set_par_d == to_par_d) {
				if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
					set_par_n != set_par_d)
					gst_structure_set (outs, "height", G_TYPE_INT, set_h,
						"pixel-aspect-ratio", GST_TYPE_FRACTION, set_par_n, set_par_d,
						NULL);
				goto done;
			}

			/* Otherwise scale the height to the new PAR and check if the
			 * adjusted with is accepted. If all that fails we can't keep
			 * the DAR */
			if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
				set_par_n, &num, &den)) {
				GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
					("Error calculating the output scaled size - integer overflow"));
				goto done;
			}

			h = (guint) gst_util_uint64_scale_int_round (w, den, num);
			gst_structure_fixate_field_nearest_int (outs, "height", h);
			if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
				set_par_n != set_par_d)
				gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
					set_par_n, set_par_d, NULL);

			goto done;
		} else if (gst_value_is_fixed (to_par)) {
			GstStructure *tmp;
			gint set_h, set_w, f_h, f_w;

			to_par_n = gst_value_get_fraction_numerator (to_par);
			to_par_d = gst_value_get_fraction_denominator (to_par);

			/* Calculate scale factor for the PAR change */
			if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, to_par_n,
				to_par_d, &num, &den)) {
				GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
					("Error calculating the output scaled size - integer overflow"));
				goto done;
			}

			/* Try to keep the input height (because of interlacing) */
			tmp = gst_structure_copy (outs);
			gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
			gst_structure_get_int (tmp, "height", &set_h);

			/* This might have failed but try to scale the width
			 * to keep the DAR nonetheless */
			w = (guint) gst_util_uint64_scale_int_round (set_h, num, den);
			gst_structure_fixate_field_nearest_int (tmp, "width", w);
			gst_structure_get_int (tmp, "width", &set_w);
			gst_structure_free (tmp);

			/* We kept the DAR and the height is nearest to the original height */
			if (set_w == w) {
				gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
					G_TYPE_INT, set_h, NULL);
				goto done;
			}

			f_h = set_h;
			f_w = set_w;

			/* If the former failed, try to keep the input width at least */
			tmp = gst_structure_copy (outs);
			gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
			gst_structure_get_int (tmp, "width", &set_w);

			/* This might have failed but try to scale the width
			 * to keep the DAR nonetheless */
			h = (guint) gst_util_uint64_scale_int_round (set_w, den, num);
			gst_structure_fixate_field_nearest_int (tmp, "height", h);
			gst_structure_get_int (tmp, "height", &set_h);
			gst_structure_free (tmp);

			/* We kept the DAR and the width is nearest to the original width */
			if (set_h == h) {
				gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
					G_TYPE_INT, set_h, NULL);
				goto done;
			}

			/* If all this failed, keep the dimensions with the DAR that was closest
			 * to the correct DAR. This changes the DAR but there's not much else to
			 * do here.
			 */
			if (set_w * ABS (set_h - h) < ABS (f_w - w) * f_h) {
				f_h = set_h;
				f_w = set_w;
			}
			gst_structure_set (outs, "width", G_TYPE_INT, f_w, "height", G_TYPE_INT,
				f_h, NULL);
			goto done;
		} else {
			GstStructure *tmp;
			gint set_h, set_w, set_par_n, set_par_d, tmp2;

			/* width, height and PAR are not fixed but passthrough is not possible */

			/* First try to keep the height and width as good as possible
			 * and scale PAR */
			tmp = gst_structure_copy (outs);
			gst_structure_fixate_field_nearest_int (tmp, "height", from_h);
			gst_structure_get_int (tmp, "height", &set_h);
			gst_structure_fixate_field_nearest_int (tmp, "width", from_w);
			gst_structure_get_int (tmp, "width", &set_w);

			if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_h, set_w,
				&to_par_n, &to_par_d)) {
				GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
					("Error calculating the output scaled size - integer overflow"));
				gst_structure_free (tmp);
				goto done;
			}

			if (!gst_structure_has_field (tmp, "pixel-aspect-ratio"))
				gst_structure_set_value (tmp, "pixel-aspect-ratio", to_par);
			gst_structure_fixate_field_nearest_fraction (tmp, "pixel-aspect-ratio",
				to_par_n, to_par_d);
			gst_structure_get_fraction (tmp, "pixel-aspect-ratio", &set_par_n,
				&set_par_d);
			gst_structure_free (tmp);

			if (set_par_n == to_par_n && set_par_d == to_par_d) {
				gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
					G_TYPE_INT, set_h, NULL);

				if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
					set_par_n != set_par_d)
					gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
						set_par_n, set_par_d, NULL);
				goto done;
			}

			/* Otherwise try to scale width to keep the DAR with the set
			 * PAR and height */
			if (!gst_util_fraction_multiply (from_dar_n, from_dar_d, set_par_d,
				set_par_n, &num, &den)) {
				GST_ELEMENT_ERROR (base, CORE, NEGOTIATION, (NULL),
					("Error calculating the output scaled size - integer overflow"));
				goto done;
			}

			w = (guint) gst_util_uint64_scale_int_round (set_h, num, den);
			tmp = gst_structure_copy (outs);
			gst_structure_fixate_field_nearest_int (tmp, "width", w);
			gst_structure_get_int (tmp, "width", &tmp2);
			gst_structure_free (tmp);

			if (tmp2 == w) {
				gst_structure_set (outs, "width", G_TYPE_INT, tmp2, "height",
					G_TYPE_INT, set_h, NULL);
				if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
					set_par_n != set_par_d)
					gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
						set_par_n, set_par_d, NULL);
				goto done;
			}

			/* ... or try the same with the height */
			h = (guint) gst_util_uint64_scale_int_round (set_w, den, num);
			tmp = gst_structure_copy (outs);
			gst_structure_fixate_field_nearest_int (tmp, "height", h);
			gst_structure_get_int (tmp, "height", &tmp2);
			gst_structure_free (tmp);

			if (tmp2 == h) {
				gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
					G_TYPE_INT, tmp2, NULL);
				if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
					set_par_n != set_par_d)
					gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
						set_par_n, set_par_d, NULL);
				goto done;
			}

			/* If all fails we can't keep the DAR and take the nearest values
			 * for everything from the first try */
			gst_structure_set (outs, "width", G_TYPE_INT, set_w, "height",
				G_TYPE_INT, set_h, NULL);
			if (gst_structure_has_field (outs, "pixel-aspect-ratio") ||
				set_par_n != set_par_d)
				gst_structure_set (outs, "pixel-aspect-ratio", GST_TYPE_FRACTION,
					set_par_n, set_par_d, NULL);
		}
	}

done:
	othercaps = gst_caps_fixate (othercaps);

	GST_DEBUG_OBJECT (base, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);

	if (from_par == &fpar)
		g_value_unset (&fpar);
	if (to_par == &tpar)
		g_value_unset (&tpar);

	return othercaps;
}

	static gboolean
gst_yuvconvert_filter_meta (GstBaseTransform * trans, GstQuery * query,
	GType api, const GstStructure * params)
{
	/* propose all metadata upstream */
	return TRUE;
}

	static gboolean
gst_yuvconvert_transform_meta (GstBaseTransform * trans, GstBuffer * outbuf,
	GstMeta * meta, GstBuffer * inbuf)
{
	const GstMetaInfo *info = meta->info;
	gboolean ret;

	if (gst_meta_api_type_has_tag (info->api, _colorspace_quark)) {
		/* don't copy colorspace specific metadata, FIXME, we need a MetaTransform
		 * for the colorspace metadata. */
		ret = FALSE;
	} else {
		/* copy other metadata */
		ret = TRUE;
	}
	return ret;
}

static gboolean
gst_yuvconvert_src_event (GstBaseTransform * trans, GstEvent * event)
{
  GstVideoFilter *filter = GST_VIDEO_FILTER_CAST (trans);
  gboolean ret;
  gdouble a;
  GstStructure *structure;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NAVIGATION:
      if (filter->in_info.width != filter->out_info.width ||
          filter->in_info.height != filter->out_info.height) {
        event =
            GST_EVENT (gst_mini_object_make_writable (GST_MINI_OBJECT (event)));

        structure = (GstStructure *) gst_event_get_structure (event);
        if (gst_structure_get_double (structure, "pointer_x", &a)) {
          gst_structure_set (structure, "pointer_x", G_TYPE_DOUBLE,
              a * filter->in_info.width / filter->out_info.width, NULL);
        }
        if (gst_structure_get_double (structure, "pointer_y", &a)) {
          gst_structure_set (structure, "pointer_y", G_TYPE_DOUBLE,
              a * filter->in_info.height / filter->out_info.height, NULL);
        }
      }
      break;
    default:
      break;
  }

  ret = GST_BASE_TRANSFORM_CLASS(parent_class)->src_event (trans, event);

  return ret;
}

static void
gst_yuvconvert_class_init (GstYuvconvertClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class =
      GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  GST_DEBUG ("enter class_init");

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SRC_CAPS)));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS (klass),
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          gst_caps_from_string (VIDEO_SINK_CAPS)));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "yuvconvert", "Filter/Converter/Video", "nv12/i420 scaler/convert use libyuv",
      "jiangying <jiangying@phytium.com.cn>");

  gobject_class->set_property = gst_yuvconvert_set_property;
  gobject_class->get_property = gst_yuvconvert_get_property;
  gobject_class->dispose = gst_yuvconvert_dispose;
  gobject_class->finalize = gst_yuvconvert_finalize;

  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_yuvconvert_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_yuvconvert_stop);
  base_transform_class->transform_caps =
	  GST_DEBUG_FUNCPTR (gst_yuvconvert_transform_caps);
  base_transform_class->fixate_caps =
	  GST_DEBUG_FUNCPTR (gst_yuvconvert_fixate_caps);
  base_transform_class->filter_meta =
	  GST_DEBUG_FUNCPTR (gst_yuvconvert_filter_meta);
  base_transform_class->transform_meta =
	  GST_DEBUG_FUNCPTR (gst_yuvconvert_transform_meta);
  base_transform_class->src_event = GST_DEBUG_FUNCPTR (gst_yuvconvert_src_event);
  base_transform_class->passthrough_on_same_caps = TRUE;

  video_filter_class->set_info = GST_DEBUG_FUNCPTR (gst_yuvconvert_set_info);
  video_filter_class->transform_frame =
      GST_DEBUG_FUNCPTR (gst_yuvconvert_transform_frame);
}

static void
gst_yuvconvert_init (GstYuvconvert * yuvconvert)
{
  GST_DEBUG ("enter yuvconvert_init");
}

void
gst_yuvconvert_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstYuvconvert *yuvconvert = GST_YUVCONVERT (object);

  GST_DEBUG_OBJECT (yuvconvert, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_yuvconvert_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstYuvconvert *yuvconvert = GST_YUVCONVERT (object);

  GST_DEBUG_OBJECT (yuvconvert, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_yuvconvert_dispose (GObject * object)
{
  GstYuvconvert *yuvconvert = GST_YUVCONVERT (object);

  GST_DEBUG_OBJECT (yuvconvert, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_yuvconvert_parent_class)->dispose (object);
}

void
gst_yuvconvert_finalize (GObject * object)
{
  GstYuvconvert *yuvconvert = GST_YUVCONVERT (object);

  GST_DEBUG_OBJECT (yuvconvert, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_yuvconvert_parent_class)->finalize (object);
}

static gboolean
gst_yuvconvert_start (GstBaseTransform * trans)
{
  GstYuvconvert *yuvconvert = GST_YUVCONVERT (trans);

  GST_DEBUG_OBJECT (yuvconvert, "start");

  return TRUE;
}

static gboolean
gst_yuvconvert_stop (GstBaseTransform * trans)
{
  GstYuvconvert *yuvconvert = GST_YUVCONVERT (trans);

  GST_DEBUG_OBJECT (yuvconvert, "stop");

  return TRUE;
}

static gboolean
gst_yuvconvert_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstYuvconvert *yuvconvert = GST_YUVCONVERT (filter);

  GST_DEBUG_OBJECT (yuvconvert, "set_info");

  if ((in_info->width == out_info->width) && (in_info->height == out_info->height) &&
	  (in_info->finfo->format == out_info->finfo->format)) {
	  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (filter), TRUE);
  }

  GST_DEBUG ("reconfigured %d %d", GST_VIDEO_INFO_FORMAT (in_info),
	  GST_VIDEO_INFO_FORMAT (out_info));

  return TRUE;
}

static gint align_to(gint orig, gint ali) {
	return (orig + ali - 1) / ali * ali;
}

/* transform */
static GstFlowReturn
gst_yuvconvert_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe)
{
  gint width, height, stride;
  gint ali_width, ali_height, ali_stride;
  gint in_width, in_height;
  gint y_stride, uv_stride;
  gint out_y_stride = 0, out_uv_stride = 0;
  guint32 *out_data;
  guint8 *y_in, *u_in = NULL, *uv_in = NULL, *v_in = NULL;
  guint8 *y_out = NULL, *u_out = NULL, *v_out = NULL;
  gint buf_size = 0;
  guint8 *tmp_buf = NULL;

  GstYuvconvert *yuvconvert = GST_YUVCONVERT (filter);

  GST_DEBUG_OBJECT (yuvconvert, "transform_frame");

  /* in video info */
  y_stride = GST_VIDEO_FRAME_PLANE_STRIDE (inframe, 0);
  uv_stride = GST_VIDEO_FRAME_PLANE_STRIDE (inframe, 1);

  if (GST_VIDEO_FORMAT_I420 == inframe->info.finfo->format) {
	  y_in = (guint8*) GST_VIDEO_FRAME_PLANE_DATA (inframe, 0);
	  u_in = (guint8*) GST_VIDEO_FRAME_PLANE_DATA (inframe, 1);
	  v_in = (guint8*) GST_VIDEO_FRAME_PLANE_DATA (inframe, 2);
  } else {
	  y_in = (guint8*) GST_VIDEO_FRAME_PLANE_DATA (inframe, 0);
	  uv_in = (guint8*) GST_VIDEO_FRAME_PLANE_DATA (inframe, 1);
  }
  in_width = GST_VIDEO_FRAME_WIDTH (inframe);
  in_height = GST_VIDEO_FRAME_HEIGHT (inframe);

  /* out video info */
  width = GST_VIDEO_FRAME_WIDTH (outframe);
  height = GST_VIDEO_FRAME_HEIGHT (outframe);
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (outframe, 0);

  ali_width = align_to(width, 2);
  ali_height = align_to(height, 2);
  ali_stride = ali_width;

  out_data = (guint32*) GST_VIDEO_FRAME_PLANE_DATA (outframe, 0);
  if (GST_VIDEO_FORMAT_I420 == outframe->info.finfo->format) {
	  y_out = (guint8*)out_data;
	  u_out = (guint8*) GST_VIDEO_FRAME_PLANE_DATA (outframe, 1);
	  v_out = (guint8*) GST_VIDEO_FRAME_PLANE_DATA (outframe, 2);
	  out_y_stride = GST_VIDEO_FRAME_PLANE_STRIDE (outframe, 0);
	  out_uv_stride = GST_VIDEO_FRAME_PLANE_STRIDE (outframe, 1);
  }

  /* scaler */
  if ((in_width != width) || (in_height != height)) {
	  if (outframe->info.finfo->format != inframe->info.finfo->format) {
		  buf_size = ali_stride * ali_height * 1.5;
		  tmp_buf = malloc(buf_size);
		  if (!tmp_buf) {
			  GST_ERROR("yuvconvert: malloc failed\n");
			  return GST_FLOW_ERROR;
		  }

		  if (GST_VIDEO_FORMAT_I420 == inframe->info.finfo->format) { //i420 scaler
			  I420Scale(y_in, y_stride, u_in, uv_stride, v_in, uv_stride, in_width, in_height,
				  tmp_buf, ali_stride,
				  tmp_buf + ali_stride * ali_height, ali_stride / 2,
				  tmp_buf + ali_stride * ali_height * 5 / 4, ali_stride / 2,
				  width, height, kFilterBilinear);
			  y_in = tmp_buf;
			  u_in = tmp_buf + ali_stride * ali_height;
			  v_in = tmp_buf + ali_stride * ali_height * 5 / 4;
			  y_stride = ali_stride;
			  uv_stride = ali_stride / 2;
		  } else { //NV12 scaler
			  NV12Scale(y_in, y_stride, uv_in, uv_stride, in_width, in_height,
				  tmp_buf, ali_stride,
				  tmp_buf + ali_stride * ali_height, ali_stride,
				  width, height, kFilterBilinear);
			  y_in = tmp_buf;
			  uv_in = tmp_buf + ali_stride * ali_height;
			  y_stride = ali_stride;
			  uv_stride = ali_stride;
		  }
	  } else if (GST_VIDEO_FORMAT_I420 == inframe->info.finfo->format) { //i420 scaler
		  I420Scale(y_in, y_stride, u_in, uv_stride, v_in, uv_stride, in_width, in_height,
			  y_out, out_y_stride,
			  u_out, out_uv_stride,
			  v_out, out_uv_stride,
			  width, height, kFilterBilinear);
	  }
  }

  /* format convert */
  if (outframe->info.finfo->format != inframe->info.finfo->format) {
	  switch (outframe->info.finfo->format) {
		  case GST_VIDEO_FORMAT_I420:
			  /* NV12 -> I420 */
			  NV12ToI420(y_in, y_stride,
				  uv_in, uv_stride,
				  y_out, out_y_stride,
				  u_out, out_uv_stride,
				  v_out, out_uv_stride,
				  width, height);
			  break;

		  case GST_VIDEO_FORMAT_BGRx:
			  if (GST_VIDEO_FORMAT_I420 == inframe->info.finfo->format) {
				  I420ToARGB(y_in, y_stride,
					  u_in, uv_stride,
					  v_in, uv_stride,
					  (guint8*)out_data, stride,
					  width, height);
			  } else {
				  NV12ToARGB(y_in, y_stride,
					  uv_in, uv_stride,
					  (guint8*)out_data, stride,
					  width, height);
			  }
			  break;

		  default:
			  break;
	  }
  }

  if (tmp_buf) {
	  free(tmp_buf);
  }

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

  /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register (plugin, "yuvconvert", GST_RANK_NONE,
      GST_TYPE_YUVCONVERT);
}

/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "0.1.0"
#endif
#ifndef PACKAGE
#define PACKAGE "yuvconvert"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gst_yuvconvert"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://phytium.com.cn/"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    yuvconvert,
    "nv12/i420 scaler and convert",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
