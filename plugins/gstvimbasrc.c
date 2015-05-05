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
static gboolean gst_vimba_src_negotiate (GstBaseSrc * src);
static GstCaps *gst_vimba_src_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_vimba_src_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_vimba_src_decide_allocation (GstBaseSrc * src,
        GstQuery * query);
static gboolean gst_vimba_src_start (GstBaseSrc * src);
static gboolean gst_vimba_src_stop (GstBaseSrc * src);
static void gst_vimba_src_get_times (GstBaseSrc * src, GstBuffer * buffer,
        GstClockTime * start, GstClockTime * end);
static gboolean gst_vimba_src_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_vimba_src_unlock (GstBaseSrc * src);
static gboolean gst_vimba_src_unlock_stop (GstBaseSrc * src);
static gboolean gst_vimba_src_query (GstBaseSrc * src, GstQuery * query);
static gboolean gst_vimba_src_event (GstBaseSrc * src, GstEvent * event);
static GstFlowReturn gst_vimba_src_create (GstPushSrc * src, GstBuffer **buf);
static GstFlowReturn gst_vimba_src_alloc (GstPushSrc * src, GstBuffer **buf);
static GstFlowReturn gst_vimba_src_fill (GstPushSrc * src, GstBuffer *buf);

gboolean gst_vimba_src_discover (GstVimbaSrc * src);
gboolean gst_vimba_src_open_camera (GstVimbaSrc * src);
gboolean gst_vimba_src_close_camera (GstVimbaSrc * src);
gboolean gst_vimba_src_init_config(GstVimbaSrc * src);

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
//    base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_vimba_src_negotiate);
//    base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_vimba_src_fixate);
    base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_vimba_src_set_caps);
//    base_src_class->decide_allocation = GST_DEBUG_FUNCPTR (gst_vimba_src_decide_allocation);
    base_src_class->start = GST_DEBUG_FUNCPTR (gst_vimba_src_start);
    base_src_class->stop = GST_DEBUG_FUNCPTR (gst_vimba_src_stop);
//    base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_vimba_src_get_times);
//    base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_vimba_src_get_size);
//    base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_vimba_src_unlock);
//    base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_vimba_src_unlock_stop);
//    base_src_class->query = GST_DEBUG_FUNCPTR (gst_vimba_src_query);
//    base_src_class->event = GST_DEBUG_FUNCPTR (gst_vimba_src_event);
    push_src_class->create = GST_DEBUG_FUNCPTR (gst_vimba_src_create);
//    push_src_class->alloc = GST_DEBUG_FUNCPTR (gst_vimba_src_alloc);
//    push_src_class->fill = GST_DEBUG_FUNCPTR (gst_vimba_src_fill);

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

    //vimbasrc->config_lock = *g_mutex_new();
    g_mutex_init(&vimbasrc->config_lock);

    g_mutex_lock(&vimbasrc->config_lock);
    vimbasrc->config = malloc(sizeof(VimbaConfig));
    g_mutex_unlock(&vimbasrc->config_lock);

    g_mutex_lock(&vimbasrc->config_lock);
    /* Startup the Vimba API */
    VmbError_t err = VmbStartup();
    if (VmbErrorInternalFault == err) {
        g_error("Error initializing VIMBA");
        exit(EXIT_FAILURE);
    }
    g_mutex_unlock(&vimbasrc->config_lock);
    /* Discover cameras */
    gst_vimba_src_discover(vimbasrc);
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
            vimbasrc->config->camera_id = camera_id;
            //vimbasrc->config->camera_id = g_value_get_string(value);
            g_mutex_unlock(&vimbasrc->config_lock);
            /* open the camera */
            gst_vimba_src_open_camera(vimbasrc);
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
            g_value_set_string(value, vimbasrc->config->camera_id);
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
    gst_vimba_src_close_camera(vimbasrc);

    G_OBJECT_CLASS (gst_vimba_src_parent_class)->dispose (object);
}

void
gst_vimba_src_finalize (GObject * object)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (object);

    GST_DEBUG_OBJECT (vimbasrc, "finalize");

    /* clean up object here */
    g_mutex_clear(&vimbasrc->config_lock);
    free(vimbasrc->config);

    /* Shutdown the Vimba API */
    VmbShutdown();

    G_OBJECT_CLASS (gst_vimba_src_parent_class)->finalize (object);
}

/* get caps from subclass */
static GstCaps *
gst_vimba_src_get_caps (GstBaseSrc * src, GstCaps * filter)
{
    g_message("gst_vimba_src_get_caps");
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);
    GstCaps *caps;
    gst_vimba_src_init_config(vimbasrc);

    g_mutex_lock(&vimbasrc->config_lock);
    caps = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(src));
    caps = gst_caps_make_writable(caps);

    guint size = gst_caps_get_size(caps);
    g_message("num structures: %d", size );

    /* FIXME:
     * Fill both "Bayer" and "Raw" capability structures with available
     * formats from the camera.
     */

    GstStructure *bayer, *raw;
    raw   = gst_caps_get_structure(caps, 0);
    bayer = gst_caps_get_structure(caps, 1);

    gst_structure_set(raw,
        "width",  GST_TYPE_INT_RANGE, 1, vimbasrc->config->max_width,
        "height", GST_TYPE_INT_RANGE, 1, vimbasrc->config->max_height,
        "format", G_TYPE_STRING, "BGRx",
        NULL
    );
    gst_structure_set(bayer,
        "width",  GST_TYPE_INT_RANGE, 1, vimbasrc->config->max_width,
        "height", GST_TYPE_INT_RANGE, 1, vimbasrc->config->max_height,
        "format", G_TYPE_STRING, "grbg",
        NULL
    );

    g_message("caps: %s", gst_caps_to_string(caps));

    g_mutex_unlock(&vimbasrc->config_lock);
    GST_DEBUG_OBJECT (vimbasrc, "get_caps");

    return caps;
}

/* decide on caps */
static gboolean
gst_vimba_src_negotiate (GstBaseSrc * src)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "negotiate");

    return TRUE;
}

/* called if, in negotiation, caps need fixating */
static GstCaps *
gst_vimba_src_fixate (GstBaseSrc * src, GstCaps * caps)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "fixate");

    return NULL;
}

/* notify the subclass of new caps */
static gboolean
gst_vimba_src_set_caps (GstBaseSrc * src, GstCaps * caps)
{
    g_message("gst_vimba_src_set_caps");

    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);
    GstVideoInfo info;
    GstVideoFormat format;
    GstStructure *structure;

    g_message("Negotiated Caps: %s", gst_caps_to_string(caps));

    if (!gst_video_info_from_caps(&info, caps)) {
        return FALSE;
    }
    structure = gst_caps_get_structure(caps, 0);
    g_mutex_lock(&vimbasrc->config_lock);

    vimbasrc->config->width = info.width;
    vimbasrc->config->height = info.height;
    format = info.finfo->format;

    /* also set selected caps on the camera */
    VmbFeatureIntSet(vimbasrc->config->camera_handle, "Width",
        vimbasrc->config->width
    );
    VmbFeatureIntSet(vimbasrc->config->camera_handle, "Height",
        vimbasrc->config->height
    );
    /* FIXME: Set capability from fomat */
    VmbFeatureEnumSet(vimbasrc->config->camera_handle, "PixelFormat",
        "BGR8Packed"
    );

    g_mutex_unlock(&vimbasrc->config_lock);
    GST_DEBUG_OBJECT (vimbasrc, "set_caps");

    return TRUE;
}

/* setup allocation query */
static gboolean
gst_vimba_src_decide_allocation (GstBaseSrc * src, GstQuery * query)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "decide_allocation");

    return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_vimba_src_start (GstBaseSrc * src)
{
    gboolean res = TRUE;
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    g_mutex_lock(&vimbasrc->config_lock);
    VmbCaptureStart(vimbasrc->config->camera_handle);
    if (!VmbFeatureCommandRun(
            vimbasrc->config->camera_handle,
            "AquisitionStart"
        )
    ) {
        g_error("Failed to start aquisition");
        res = FALSE;
    } else {
        g_message("Aquisition started");
    }
    g_mutex_unlock(&vimbasrc->config_lock);

    GST_DEBUG_OBJECT (vimbasrc, "start");

    return res;
}

static gboolean
gst_vimba_src_stop (GstBaseSrc * src)
{
    gboolean res = TRUE;
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    g_mutex_lock(&vimbasrc->config_lock);
    if (!VmbFeatureCommandRun(
            vimbasrc->config->camera_handle,
            "AquisitionStop"
        )
    ) {
        g_error("Failed to stop aquisition");
        res = FALSE;
    } else {
        g_message("Aquisition stopped");
    }
    VmbCaptureEnd(vimbasrc->config->camera_handle);
    g_mutex_unlock(&vimbasrc->config_lock);

    GST_DEBUG_OBJECT (vimbasrc, "stop");

    return res;
}

/* given a buffer, return start and stop time when it should be pushed
 * out. The base class will sync on the clock using these times. */
static void
gst_vimba_src_get_times (GstBaseSrc * src, GstBuffer * buffer,
        GstClockTime * start, GstClockTime * end)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "get_times");

}

/* get the total size of the resource in bytes */
static gboolean
gst_vimba_src_get_size (GstBaseSrc * src, guint64 * size)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "get_size");

    return TRUE;
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
static gboolean
gst_vimba_src_unlock (GstBaseSrc * src)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "unlock");

    return TRUE;
}

/* Clear any pending unlock request, as we succeeded in unlocking */
static gboolean
gst_vimba_src_unlock_stop (GstBaseSrc * src)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "unlock_stop");

    return TRUE;
}

/* notify subclasses of a query */
static gboolean
gst_vimba_src_query (GstBaseSrc * src, GstQuery * query)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "query");

    return TRUE;
}

/* notify subclasses of an event */
static gboolean
gst_vimba_src_event (GstBaseSrc * src, GstEvent * event)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "event");

    return TRUE;
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn
gst_vimba_src_create (GstPushSrc * src, GstBuffer ** bufp)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);
    VmbError_t err;
    GstClockTime gst_pts = GST_CLOCK_TIME_NONE;
    GstBuffer *buf;
    GstFlowReturn ret = GST_FLOW_ERROR;

    g_mutex_lock(&vimbasrc->config_lock);

    /** capture a frame synchronously */
//    err = VmbCaptureFrameQueue(
//        vimbasrc->config->camera_handle,
//        &vimbasrc->config->frame,
//        NULL
//    );
    if (VmbErrorSuccess == err) {
        err = VmbCaptureFrameWait(
            vimbasrc->config->camera_handle,
            &vimbasrc->config->frame,
            5000
        );

        g_mutex_unlock(&vimbasrc->config_lock);
        if (VmbErrorSuccess == err) {
            g_mutex_lock(&vimbasrc->config_lock);

            buf = gst_buffer_new_allocate(NULL, vimbasrc->config->frame.bufferSize, NULL);

            if (buf) {
                GST_BUFFER_PTS(buf) = gst_pts;
                gst_buffer_fill(
                    buf, 0,
                    vimbasrc->config->frame.buffer,
                    vimbasrc->config->frame.bufferSize
                );
                *bufp = buf;
                ret = GST_FLOW_OK;
            }

            g_mutex_unlock(&vimbasrc->config_lock);
        } else if (VmbErrorTimeout == err) {
            g_error("capture timeout");
            ret = GST_FLOW_ERROR;
        } else if (VmbErrorBadHandle == err) {
            g_error("bad camera handle");
            ret = GST_FLOW_ERROR;
        }
    } else if (VmbErrorBadHandle == err) {
        g_error("queued invalid frame");
        ret = GST_FLOW_ERROR;
    } else if (VmbErrorStructSize == err) {
        g_error("invalid struct size");
        ret = GST_FLOW_ERROR;
    }

    err = VmbCaptureQueueFlush(vimbasrc->config->camera_handle);

    GST_DEBUG_OBJECT (vimbasrc, "create");

    return ret;
}

/* ask the subclass to allocate an output buffer. The default implementation
 * will use the negotiated allocator. */
static GstFlowReturn
gst_vimba_src_alloc (GstPushSrc * src, GstBuffer ** buf)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "alloc");

    return GST_FLOW_OK;
}

/* ask the subclass to fill the buffer with data from offset and size */
static GstFlowReturn
gst_vimba_src_fill (GstPushSrc * src, GstBuffer * buf)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC (src);

    GST_DEBUG_OBJECT (vimbasrc, "fill");

    return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

    /* Remember to set the rank if it's an element that is meant
       to be autoplugged by decodebin. */
    return gst_element_register (plugin, "vimbasrc", GST_RANK_NONE,
            GST_TYPE_VIMBA_SRC);
}

gboolean
gst_vimba_src_discover(GstVimbaSrc * src) {
    /* Look for any attached cameras */
    VmbBool_t gigE;
    VmbUint32_t count, i;
    VmbCameraInfo_t *cameras;
    g_mutex_lock(&src->config_lock);
    VmbError_t err = VmbFeatureBoolGet(gVimbaHandle, "GeVTLIsPresent", &gigE);
    if (VmbErrorSuccess == err) {
        if (VmbBoolTrue == gigE) {
            err = VmbFeatureCommandRun(gVimbaHandle, "GeVDiscoveryAllOnce");
        }
    }
    if (VmbErrorSuccess == err) {
        err = VmbCamerasList(NULL, 0, &count, sizeof (*cameras) );
        g_message("Found %d cameras", count);
        if (VmbErrorSuccess == err) {
            cameras = (VmbCameraInfo_t*) malloc(count * sizeof (*cameras) );
            err = VmbCamerasList(cameras, count, &count, sizeof (*cameras) );
            g_message("Found the following cameras:\n");
            for (i = 0; i < count; ++i) {
                g_message("\t%s\n", cameras[i].cameraIdString);
            }
            free(cameras);
        }
    }
    g_mutex_unlock(&src->config_lock);
    return TRUE;
}

gboolean
gst_vimba_src_init_config(GstVimbaSrc * src) {
    gboolean res = TRUE;
    g_mutex_lock(&src->config_lock);
    if (!VmbFeatureIntGet(
        src->config->camera_handle,
        "HeightMax",
        &(src->config->max_height)
    )) {
        res = FALSE;
    };
    if (!VmbFeatureIntGet(
        src->config->camera_handle,
        "WidthMax",
        &(src->config->max_width)
    )) {
        res = FALSE;
    };
    if (!VmbFeatureIntGet(
        src->config->camera_handle,
        "Width",
        &(src->config->width)
    )) {
        res = FALSE;
    };
    if (!VmbFeatureIntGet(
        src->config->camera_handle,
        "Height",
        &(src->config->height)
    )) {
        res = FALSE;
    };
    if (!VmbFeatureEnumGet(
        src->config->camera_handle,
        "PixelFormat",
        &(src->config->format)
    )) {
        res = FALSE;
    }

    VmbUint32_t i;
    VmbError_t err = VmbFeatureEnumRangeQuery(
        src->config->camera_handle,
        "PixelFormat",
        src->config->supported_formats,
        GST_VIMBA_SRC_MAXFORMATS,
        &(src->config->format_count)
    );
//    if (VmbErrorSuccess == err) {
//        for (i = 0; i < src->config->format_count; i++) {
//            g_message("%s", src->config->supported_formats[i]);
//        }
//    }
//
    g_message(
        "camera configuration: width: %lu, height: %lu, format: %s",
        (unsigned long) src->config->width,
        (unsigned long) src->config->height,
        src->config->format
    );
    g_mutex_unlock(&src->config_lock);
    return res;
}

gboolean
gst_vimba_src_open_camera(GstVimbaSrc * src) {
    VmbError_t err;
    VmbInt64_t size;

    g_mutex_lock(&src->config_lock);
    if (src->config->camera_id == NULL) {
        GST_ERROR("You need to specify a camera id!");
        return FALSE;
    }
    g_message("opening camera %s", src->config->camera_id);

    err = VmbCameraOpen(
        src->config->camera_id,
        VmbAccessModeFull,
        &(src->config->camera_handle)
    );

    err = VmbFeatureIntGet(
        src->config->camera_handle,
        "PayloadSize",
         &size
    );

    src->config->frame.buffer = malloc(size * sizeof(char));
    src->config->frame.bufferSize = size;

    VmbFrameAnnounce(
        src->config->camera_handle,
        &src->config->frame,
        sizeof(VmbFrame_t)
    );
    g_mutex_unlock(&src->config_lock);

    if (VmbErrorSuccess == err) {
        g_message("success!");
        gst_vimba_src_init_config(src);
    } else if (VmbErrorNotFound == err) {
        g_error("Camera %s not found", src->config->camera_id);
    } else if (VmbErrorInvalidAccess == err) {
        g_error("Cannot acces camera");
    }

    return TRUE;
}

gboolean
gst_vimba_src_close_camera(GstVimbaSrc * src) {
    VmbError_t err;
    g_mutex_lock(&src->config_lock);
    err = VmbCameraClose(src->config->camera_handle);
    g_mutex_unlock(&src->config_lock);
    if (err != VmbErrorSuccess) {
        return FALSE;
    }
    return TRUE;
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

