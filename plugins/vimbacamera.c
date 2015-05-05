#include <stdlib.h>
#include "vimbacamera.h"

void VMB_CALL frame_callback( const VmbHandle_t camera_handle, VmbFrame_t * frame) {
    if (VmbFrameStatusComplete == frame->receiveStatus) {
        g_message("Frame successfully received");
    } else {
        g_message("Error receiving frame");
    }
    VmbCaptureFrameQueue(camera_handle, frame, frame_callback);
}

VimbaCamera* vimbacamera_init() {
    VimbaCamera* camera = malloc(sizeof(VimbaCamera));
    return camera;
}

void vimbacamera_destroy (VimbaCamera * camera) {
    if (camera) {
        free(camera);
    }
    camera = NULL;
}

gboolean vimbacamera_open (VimbaCamera * camera) {
    VmbError_t err;
    int i;

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

    err = VmbFeatureIntGet(
        camera->camera_handle,
        "PayloadSize",
         &camera->payload_size
    );

    for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
        camera->frames[i].buffer = malloc(camera->payload_size * sizeof(char));
        camera->frames[i].bufferSize = camera->payload_size;
        VmbFrameAnnounce(
            camera->camera_handle,
            &camera->frames[i],
            sizeof(VmbFrame_t)
        );
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

gboolean vimbacamera_load (VimbaCamera * camera) {
    g_message("Loading camera data for camera %s", camera->camera_id);
    gboolean res = TRUE;
    if (!VmbFeatureIntGet(
        camera->camera_handle,
        "HeightMax",
        &(camera->max_height)
    )) {
        res = FALSE;
    };
    if (!VmbFeatureIntGet(
        camera->camera_handle,
        "WidthMax",
        &(camera->max_width)
    )) {
        res = FALSE;
    };
    if (!VmbFeatureIntGet(
        camera->camera_handle,
        "Width",
        &(camera->width)
    )) {
        res = FALSE;
    };
    if (!VmbFeatureIntGet(
        camera->camera_handle,
        "Height",
        &(camera->height)
    )) {
        res = FALSE;
    };
    if (!VmbFeatureEnumGet(
        camera->camera_handle,
        "PixelFormat",
        &(camera->format)
    )) {
        res = FALSE;
    }
    VmbFeatureEnumRangeQuery(
        camera->camera_handle,
        "PixelFormat",
        camera->supported_formats,
        GST_VIMBA_SRC_MAXFORMATS,
        &(camera->format_count)
    );
    return res;
}

gboolean vimbacamera_start (VimbaCamera * camera) {
    VmbCaptureStart(camera->camera_handle);
    int i;
    for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
        VmbCaptureFrameQueue(
            camera->camera_handle,
            &camera->frames[i],
            frame_callback
        );
    }
    if (!VmbFeatureCommandRun(
            camera->camera_handle,
            "AquisitionStart"
        )
    ) {
        return FALSE;
    }
    return TRUE;
}

gboolean vimbacamera_stop (VimbaCamera * camera) {
    if (!VmbFeatureCommandRun(
            camera->camera_handle,
            "AquisitionStop"
        )
    ) {
        return FALSE;
    }
    VmbCaptureQueueFlush(camera->camera_handle);
    VmbCaptureEnd(camera->camera_handle);
    VmbFrameRevokeAll(camera->camera_handle);
    return TRUE;
}

