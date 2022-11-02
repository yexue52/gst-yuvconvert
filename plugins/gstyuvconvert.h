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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_YUVCONVERT_H_
#define _GST_YUVCONVERT_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_YUVCONVERT   (gst_yuvconvert_get_type())
#define GST_YUVCONVERT(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_YUVCONVERT,GstYuvconvert))
#define GST_YUVCONVERT_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_YUVCONVERT,GstYuvconvertClass))
#define GST_IS_YUVCONVERT(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_YUVCONVERT))
#define GST_IS_YUVCONVERT_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_YUVCONVERT))

typedef struct _GstYuvconvert GstYuvconvert;
typedef struct _GstYuvconvertClass GstYuvconvertClass;

struct _GstYuvconvert
{
  GstVideoFilter base_yuvconvert;

};

struct _GstYuvconvertClass
{
  GstVideoFilterClass base_yuvconvert_class;
};

GType gst_yuvconvert_get_type (void);

G_END_DECLS

#endif
