#include <stdlib.h>
#include <string.h>
#include "vimbacamera.h"

void VMB_CALL frame_callback( const VmbHandle_t camera_handle, VmbFrame_t * frame) {
    if (VmbFrameStatusComplete == frame->receiveStatus) {
        /*g_message("Frame received %lu", (unsigned long int)frame->frameID);*/
        g_async_queue_push(frame_queue, frame);
    } else {
        g_message("Error receiving frame %lu", (unsigned long int) frame->frameID);
    }
}

VmbFrame_t * vimbacamera_consume_frame() {
    VmbFrame_t * frame = g_async_queue_pop(frame_queue);
    /*g_message("Frame consumed %lu", (unsigned long int) frame->frameID);*/
    return frame;
}

void vimbacamera_queue_frame (VimbaCamera * camera, VmbFrame_t * frame) {
    /*g_message("queuing frame %lu", (unsigned long int) frame->frameID);*/
    VmbError_t err = VmbCaptureFrameQueue(
        camera->camera_handle,
        frame,
        &frame_callback
    );
    if (VmbErrorBadHandle == err) {
        g_error("Invalid frame");
    } else if (VmbErrorStructSize == err) {
        g_error("Invalid struct size for current frame");
    }
}


VimbaCamera* vimbacamera_init() {
    VimbaCamera* camera = malloc(sizeof(VimbaCamera));
    camera->started = FALSE;
    return camera;
}

void vimbacamera_destroy (VimbaCamera * camera) {
    g_async_queue_unref(frame_queue);
    if (camera) {
        free(camera);
    }
    camera = NULL;
}

gboolean vimbacamera_open (VimbaCamera * camera) {
    VmbError_t err;

    if (camera->camera_id == NULL) {
        GST_ERROR("You need to specify a camera id!");
        return FALSE;
    }
    g_message("opening camera %s", camera->camera_id);

    err = VmbCameraOpen(
        camera->camera_id,
        VmbAccessModeFull,
        &(camera->camera_handle)
    );

    if (VmbErrorSuccess == err) {
        g_message("success!");
    } else if (VmbErrorNotFound == err) {
        g_error("Camera %s not found", camera->camera_id);
        return FALSE;
    } else if (VmbErrorInvalidAccess == err) {
        g_error("Cannot acces camera");
        return FALSE;
    }

    return TRUE;
}

gboolean vimbacamera_close (VimbaCamera * camera) {
    VmbError_t err;
    err = VmbCameraClose(camera->camera_handle);
    if (err != VmbErrorSuccess) {
        return FALSE;
    }
    return TRUE;
}

void print_feature_error(VmbError_t err) {
    if (VmbErrorBadHandle == err) {
        g_error("Bad handle");
    } else if (VmbErrorInvalidAccess == err) {
        g_error("Invalid access");
    } else if (VmbErrorWrongType == err) {
        g_error("Wrong type");
    }
}

gboolean vimbacamera_load (VimbaCamera * camera) {
    g_message("Loading camera data for camera %s", camera->camera_id);
    gboolean res = TRUE;
    VmbFeatureIntGet(
        camera->camera_handle,
        "HeightMax",
        &(camera->max_height)
    );
    /*
     *if (VmbErrorSuccess != err) {
     *    g_error("Unable to get max height");
     *    print_feature_error(err);
     *    res = FALSE;
     *};
     */
    VmbFeatureIntGet(
        camera->camera_handle,
        "WidthMax",
        &(camera->max_width)
    );
    VmbFeatureIntGet(
        camera->camera_handle,
        "Width",
        &(camera->width)
    );
    VmbFeatureIntGet(
        camera->camera_handle,
        "Height",
        &(camera->height)
    );
    VmbFeatureEnumGet(
        camera->camera_handle,
        "PixelFormat",
        &(camera->format)
    );
    VmbFeatureEnumRangeQuery(
        camera->camera_handle,
        "PixelFormat",
        camera->supported_formats,
        GST_VIMBA_SRC_MAXFORMATS,
        &camera->format_count
    );
    /*
     *unsigned int i;
     *for (i = 0; i < camera->format_count; i++) {
     *    g_message("%s", camera->supported_formats[i]);
     *}
     */
    VmbFeatureFloatRangeQuery(
        camera->camera_handle,
        "AcquisitionFrameRateAbs",
        &camera->min_framerate,
        &camera->max_framerate
    );
    VmbFeatureFloatGet(
        camera->camera_handle,
        "AcquisitionFrameRateAbs",
        &camera->framerate
    );
    g_message(
        "Current camera configuration:\n \
        \tMaxWidth:\t\t%lu\n \
        \tMaxHeight:\t\t%lu\n \
        \tWidth:\t\t\t%lu\n \
        \tHeight:\t\t\t%lu\n \
        \tFormat:\t\t\t%s",
        (unsigned long int) camera->max_width,
        (unsigned long int) camera->max_height,
        (unsigned long int) camera->width,
        (unsigned long int) camera->height,
        camera->format
    );
    return res;
}


gboolean vimbacamera_start (VimbaCamera * camera) {

    if (camera->started == TRUE) {
        vimbacamera_stop(camera);
    }

    VmbError_t err;
    int i;

    /* Reset base time (should be set when reading the first frame) */
    camera->base_time = 0;

    /* Create global frame queue that's going to be used by gstreamer */
    frame_queue = g_async_queue_new();

    /* Continuous frame grabbing (in contrast to single frame capture) */
    err = VmbFeatureEnumSet(
        camera->camera_handle,
        "AcquisitionMode",
        "Continuous"
    );

    /* Create and announce frame buffers */
    err = VmbFeatureIntGet(
        camera->camera_handle,
        "PayloadSize",
         &camera->payload_size
    );

    /* create and announce frame buffers */
    for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
        memset(&camera->frames[i], 0, sizeof(VmbFrame_t));
        camera->frames[i].buffer = (unsigned char*)malloc((VmbUint32_t)camera->payload_size);
        camera->frames[i].bufferSize = (VmbUint32_t)camera->payload_size;
        /* Somehow announcing a frame prevented the api from working */
//        VmbFrameAnnounce(
//            camera->camera_handle,
//            &camera->frames[i],
//            sizeof(VmbFrame_t)
//        );
    }

    /* Start capture engine */
    VmbCaptureStart(camera->camera_handle);

    /* Queue frames */
    for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
        vimbacamera_queue_frame(camera, &camera->frames[i]);
    }

    /* Start aquisition */
    err = VmbFeatureCommandRun(camera->camera_handle, "AcquisitionStart");
    if (VmbErrorInvalidAccess == err) {
        g_error("Operation not valid with curren access mode");
        return FALSE;
    } else if (VmbErrorWrongType == err) {
        g_error("Not a command");
        return FALSE;
    }
    g_message("Acquisition started");
    return TRUE;
}

gboolean vimbacamera_stop (VimbaCamera * camera) {
    if (!VmbFeatureCommandRun(
            camera->camera_handle,
            "AcquisitionStop"
        )
    ) {
        return FALSE;
    }
    VmbCaptureQueueFlush(camera->camera_handle);
    VmbCaptureEnd(camera->camera_handle);
    VmbFrameRevokeAll(camera->camera_handle);

    g_async_queue_unref(frame_queue);

    int i;
    for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
        free(camera->frames[i].buffer);
    }

    g_message("Acquisition stopped");
    camera->started = FALSE;
    return TRUE;
}


void vimbacamera_capture (VimbaCamera * camera) {
    /* Create and announce frame buffers */
    VmbFeatureIntGet(
        camera->camera_handle,
        "PayloadSize",
         &camera->payload_size
    );

    camera->frames[0].buffer = malloc(camera->payload_size * sizeof(char));
    camera->frames[0].bufferSize = camera->payload_size;
    VmbFrameAnnounce(
        camera->camera_handle,
        &camera->frames[0],
        sizeof(VmbFrame_t)
    );
    VmbCaptureFrameWait(camera->camera_handle, &camera->frames[0], 9000);
}
