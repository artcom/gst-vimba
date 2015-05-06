#include <stdlib.h>
#include <string.h>
#include "vimbacamera.h"

void VMB_CALL frame_callback( const VmbHandle_t camera_handle, VmbFrame_t * frame) {
//    if (VmbFrameStatusComplete == frame->receiveStatus) {
//        g_message("Frame successfully received");
//    } else {
//        g_message("Error receiving frame");
//    }
    VmbCaptureFrameQueue(camera_handle, frame, &frame_callback);
}

VimbaCamera* vimbacamera_init() {
    VimbaCamera* camera = malloc(sizeof(VimbaCamera));
    camera->started = FALSE;
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
    VmbError_t err;
    err = VmbFeatureIntGet(
        camera->camera_handle,
        "HeightMax",
        &(camera->max_height)
    );
    if (VmbErrorSuccess != err) {
        g_error("Unable to get max height");
        print_feature_error(err);
        res = FALSE;
    };
    err = VmbFeatureIntGet(
        camera->camera_handle,
        "WidthMax",
        &(camera->max_width)
    );
    if (VmbErrorSuccess != err) {
        g_error("Unable to get max width");
        print_feature_error(err);
        res = FALSE;
    };
    err = VmbFeatureIntGet(
        camera->camera_handle,
        "Width",
        &(camera->width)
    );
    if (VmbErrorSuccess != err) {
        g_error("Unable to get width");
        print_feature_error(err);
        res = FALSE;
    };
    err = VmbFeatureIntGet(
        camera->camera_handle,
        "Height",
        &(camera->height)
    );
    if (VmbErrorSuccess != err) {
        g_error("Unable to get height");
        print_feature_error(err);
        res = FALSE;
    };
    err = VmbFeatureEnumGet(
        camera->camera_handle,
        "PixelFormat",
        &(camera->format)
    );
    if (VmbErrorSuccess != err) {
        g_error("Unable to get pixelformat");
        print_feature_error(err);
        res = FALSE;
    }
    err = VmbFeatureEnumRangeQuery(
        camera->camera_handle,
        "PixelFormat",
        camera->supported_formats,
        GST_VIMBA_SRC_MAXFORMATS,
        &(camera->format_count)
    );
    if (VmbErrorSuccess != err) {
        g_error("Unable to get pixelformat list");
        print_feature_error(err);
        res = FALSE;
    }
    return res;
}


gboolean vimbacamera_start (VimbaCamera * camera) {

    if (camera->started == TRUE) {
        vimbacamera_stop(camera);
    }

    VmbError_t err;
    int i;

    err = VmbFeatureEnumSet(camera->camera_handle, "AcquisitionMode", "Continuous");

    /* Create and announce frame buffers */
    err = VmbFeatureIntGet(
        camera->camera_handle,
        "PayloadSize",
         &camera->payload_size
    );

    /* create and announce frame buffers */
    for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
        camera->frames[i].buffer = (unsigned char*)malloc((VmbUint32_t)camera->payload_size);
        camera->frames[i].bufferSize = (VmbUint32_t)camera->payload_size;
        VmbFrameAnnounce(
            camera->camera_handle,
            &camera->frames[i],
            (VmbUint32_t)sizeof(VmbFrame_t)
        );
        if ( VmbErrorSuccess != err )
        {
            free( camera->frames[i].buffer );
            memset( &camera->frames[i], 0, sizeof( VmbFrame_t ));
            break;
        }
    }

    /* Start capture engine */
    VmbCaptureStart(camera->camera_handle);

    /* Queue frames */
    for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
        err = VmbCaptureFrameQueue(
            camera->camera_handle,
            &camera->frames[i],
            &frame_callback
        );
        if (VmbErrorBadHandle == err) {
            g_error("Invalid frame");
            return FALSE;
        } else if (VmbErrorStructSize == err) {
            g_error("Invalid struct size for current frame");
            return FALSE;
        }
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
