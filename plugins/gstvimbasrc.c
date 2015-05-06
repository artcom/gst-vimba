/* GStreamer
 * Copyright (C) 2015 Art+Com AG <info@artcom.de>
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
 * SECTION:element-gstvimbasrc
 *
 * The vimbasrc element supports cameras that use the VIMBA SDK
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v vimbasrc ! "video/x-raw;width=640;height=480" ! ximagesink
 * ]|
 * This will show the current camera output in an xwindow
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video-info.h>
#include "pixelformat.h"
#include "gstvimbasrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_vimba_src_debug_category);
#define GST_CAT_DEFAULT gst_vimba_src_debug_category

/* prototypes */

static void gst_vimba_src_set_property (GObject * object,
        guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_vimba_src_get_property (GObject * object,
        guint property_id, GValue * value, GParamSpec * pspec);
static void gst_vimba_src_dispose (GObject * object);
static void gst_vimba_src_finalize (GObject * object);

static GstCaps *gst_vimba_src_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_vimba_src_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_vimba_src_start (GstBaseSrc * src);
static gboolean gst_vimba_src_stop (GstBaseSrc * src);
static GstFlowReturn gst_vimba_src_create (GstPushSrc * src, GstBuffer **buf);

enum
{
    PROP_0,
    PROP_CAMERA
};

#define VIMBASRC_VIDEO_CAPS GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL) ";" \
  "video/x-bayer, format=(string) { bggr, rggb, grbg, gbrg }, "        \
  "width = " GST_VIDEO_SIZE_RANGE ", "                                 \
  "height = " GST_VIDEO_SIZE_RANGE ", "                                \
  "framerate = " GST_VIDEO_FPS_RANGE

/* pad templates */

static GstStaticPadTemplate gst_vimba_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIMBASRC_VIDEO_CAPS)
);


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (
    GstVimbaSrc,
    gst_vimba_src,
    GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (
        gst_vimba_src_debug_category,
        "vimbasrc",
        0,
        "debug category for vimbasrc element"
    )
);

static void
gst_vimba_src_class_init (GstVimbaSrcClass * klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
    GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
            gst_static_pad_template_get (&gst_vimba_src_src_template));

    gst_element_class_set_static_metadata (
        GST_ELEMENT_CLASS(klass),
        "VIMBA Camera Source",
        "Source",
        "Provides access to cameras supporting the VIMBA SDK",
        "Art+Com AG <info@artcom.de>"
    );

    gobject_class->set_property = gst_vimba_src_set_property;
    gobject_class->get_property = gst_vimba_src_get_property;
    gobject_class->dispose = gst_vimba_src_dispose;
    gobject_class->finalize = gst_vimba_src_finalize;
    base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_vimba_src_get_caps);
    base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_vimba_src_set_caps);
    base_src_class->start = GST_DEBUG_FUNCPTR (gst_vimba_src_start);
    base_src_class->stop = GST_DEBUG_FUNCPTR (gst_vimba_src_stop);
    push_src_class->create = GST_DEBUG_FUNCPTR (gst_vimba_src_create);

    /* define properties */
    g_object_class_install_property(
        gobject_class,
        PROP_CAMERA,
        g_param_spec_string(
            "camera",
            "Camera",
            "The id of the camera",
            " ",
            G_PARAM_READWRITE
        )
    );

}

static void
gst_vimba_src_init (GstVimbaSrc *vimbasrc)
{

    g_mutex_init(&vimbasrc->config_lock);

    g_mutex_lock(&vimbasrc->config_lock);
    vimbasrc->vimba  = vimba_init();
    vimba_discover(vimbasrc->vimba);
    vimbasrc->camera = vimbacamera_init();

    /* Startup the Vimba API */
    g_mutex_unlock(&vimbasrc->config_lock);
    gst_base_src_set_live(GST_BASE_SRC(vimbasrc), TRUE);
    gst_base_src_set_format(GST_BASE_SRC(vimbasrc), GST_FORMAT_TIME);
}

void
gst_vimba_src_set_property (GObject * object, guint property_id,
        const GValue * value, GParamSpec * pspec)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (object);

    GST_DEBUG_OBJECT (vimbasrc, "set_property");

    switch (property_id) {
        case PROP_CAMERA:
            g_mutex_lock(&vimbasrc->config_lock);
            const gchar* camera_id = g_value_get_string(value);
            vimbasrc->camera->camera_id = camera_id;
            /* open the camera */
            if (vimbacamera_open(vimbasrc->camera)) {
                if (vimbacamera_load(vimbasrc->camera)) {
                    g_message(
                        "camera configuration: width: %lu, height: %lu, format: %s",
                        (unsigned long) vimbasrc->camera->width,
                        (unsigned long) vimbasrc->camera->height,
                        vimbasrc->camera->format
                    );
                } else {
                    g_error("cannot fetch initial camera settings");
                }
            }
            g_mutex_unlock(&vimbasrc->config_lock);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

void
gst_vimba_src_get_property (GObject * object, guint property_id,
        GValue * value, GParamSpec * pspec)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (object);

    GST_DEBUG_OBJECT (vimbasrc, "get_property");

    switch (property_id) {
        case PROP_CAMERA:
            g_value_set_string(value, vimbasrc->camera->camera_id);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

void
gst_vimba_src_dispose (GObject * object)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (object);

    GST_DEBUG_OBJECT (vimbasrc, "dispose");

    /* clean up as possible.  may be called multiple times */
    vimbacamera_close(vimbasrc->camera);

    G_OBJECT_CLASS (gst_vimba_src_parent_class)->dispose (object);
}

void
gst_vimba_src_finalize (GObject * object)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (object);

    GST_DEBUG_OBJECT (vimbasrc, "finalize");

    /* clean up object here */
    g_mutex_clear(&vimbasrc->config_lock);
    free(vimbasrc->camera);

    /* Shutdown the Vimba API */
    vimba_destroy(vimbasrc->vimba);

    G_OBJECT_CLASS (gst_vimba_src_parent_class)->finalize (object);
}

/* get caps from subclass */
static GstCaps *
gst_vimba_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
    g_message("gst_vimba_src_get_caps");
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);
    GstCaps *caps;

    g_mutex_lock(&vimbasrc->config_lock);
    caps = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(src));
    caps = gst_caps_make_writable(caps);

    guint size = gst_caps_get_size(caps);
    g_message("num structures: %d", size );

    /* FIXME:
     * Fill both "Bayer" and "Raw" capability structures with available
     * formats from the camera.
     */

    /* FIXME:
     * Get framerate
     */

    GstStructure *bayer, *raw;
    raw   = gst_caps_get_structure(caps, 0);
    bayer = gst_caps_get_structure(caps, 1);

    gst_structure_set(raw,
        "width",  GST_TYPE_INT_RANGE, 1, vimbasrc->camera->max_width,
        "height", GST_TYPE_INT_RANGE, 1, vimbasrc->camera->max_height,
        "format", G_TYPE_STRING, "BGRx",
        "framerate", GST_TYPE_FRACTION, 1, 30,
        NULL
    );
    gst_structure_set(bayer,
        "width",  GST_TYPE_INT_RANGE, 1, vimbasrc->camera->max_width,
        "height", GST_TYPE_INT_RANGE, 1, vimbasrc->camera->max_height,
        "format", G_TYPE_STRING, "grbg",
        NULL
    );

    g_message("caps: %s", gst_caps_to_string(caps));

    g_mutex_unlock(&vimbasrc->config_lock);
    GST_DEBUG_OBJECT (vimbasrc, "get_caps");

    return caps;
}

/* notify the subclass of new caps */
static gboolean
gst_vimba_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
    g_message("gst_vimba_src_set_caps");


    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);
    GstVideoInfo info;
//    GstVideoFormat format;
//    GstStructure *structure;
//
    vimbacamera_stop(vimbasrc->camera);

    g_message("Negotiated Caps: %s", gst_caps_to_string(caps));

    if (!gst_video_info_from_caps(&info, caps)) {
        return FALSE;
    }
//    structure = gst_caps_get_structure(caps, 0);
    g_mutex_lock(&vimbasrc->config_lock);

    vimbasrc->camera->width = info.width;
    vimbasrc->camera->height = info.height;
//    format = info.finfo->format;

    /* also set selected caps on the camera */
    VmbFeatureIntSet(vimbasrc->camera->camera_handle, "Width",
        vimbasrc->camera->width
    );
    VmbFeatureIntSet(vimbasrc->camera->camera_handle, "Height",
        vimbasrc->camera->height
    );
    /* FIXME: Set capability from fomat */
    VmbFeatureEnumSet(vimbasrc->camera->camera_handle, "PixelFormat",
        "BGR8Packed"
    );

    g_mutex_unlock(&vimbasrc->config_lock);
    GST_DEBUG_OBJECT (vimbasrc, "set_caps");

    vimbacamera_start(vimbasrc->camera);

    return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_vimba_src_start (GstBaseSrc * src)
{
    gboolean res = TRUE;
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    vimbacamera_start(vimbasrc->camera);

    GST_DEBUG_OBJECT (vimbasrc, "start");

    return res;
}

static gboolean
gst_vimba_src_stop (GstBaseSrc * src)
{
    gboolean res = TRUE;
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    vimbacamera_stop(vimbasrc->camera);

    GST_DEBUG_OBJECT (vimbasrc, "stop");

    return res;
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn
gst_vimba_src_create (GstPushSrc * src, GstBuffer ** bufp)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);
//    GstClockTime gst_pts = GST_CLOCK_TIME_NONE;
    GstBuffer *buf = NULL;
    GstFlowReturn ret = GST_FLOW_ERROR;
    int i;

    /* FIXME:
     *
     * From the docs:
     * Live source elements must place a timestamp in each buffer that they
     * deliver.  They must choose the timestamps and the values of the SEGMENT
     * event in such a way that the running-time of the buffer matches exactly
     * the running-time of the pipeline clock when the first byte in the buffer
     * was captured.
     *
     * checkout this cam source:
     * https://github.com/thaytan/gst-rpicamsrc
     *
     */

    g_mutex_lock(&vimbasrc->config_lock);

    for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
        if (VmbFrameStatusComplete == vimbasrc->camera->frames[i].receiveStatus)  {
            buf = gst_buffer_new_allocate(NULL, vimbasrc->camera->frames[i].bufferSize, NULL);
            if (buf) {
                /* FIXME: set the correct timestamp! */
                //GST_BUFFER_PTS(buf) = gst_pts;
                gst_buffer_fill(
                    buf, 0,
                    vimbasrc->camera->frames[i].buffer,
                    vimbasrc->camera->frames[i].bufferSize
                );
                ret = GST_FLOW_OK;
            }
            break;
        }
    }

    *bufp = buf;
    g_mutex_unlock(&vimbasrc->config_lock);

    GST_DEBUG_OBJECT (vimbasrc, "create");

    return ret;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

    /* Remember to set the rank if it's an element that is meant
       to be autoplugged by decodebin. */
    return gst_element_register (plugin, "vimbasrc", GST_RANK_NONE,
            GST_TYPE_VIMBA_SRC);
}


/* These are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "gst_vimba_package"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gst-vimba"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://artcom.de"
#endif

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    vimbasrc,
    "Vimba source plugin",
    plugin_init, VERSION,
    "LGPL",
    PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)

