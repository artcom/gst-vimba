#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vimbacamera.h"

void VMB_CALL frame_callback(
    const VmbHandle_t camera_handle, VmbFrame_t * frame
) {
    /*g_message("Frame received %lu", (unsigned long int)frame->frameID);*/
    /*
    * make new frame cp_frame
    * memcpy frame to cp_frame
    * queue cp_frame in frame_queue
    * VmbCaptureFrameQueue with this callback
    */
    if (VmbFrameStatusComplete == frame->receiveStatus) {
        //g_message("Frame received %lu", (unsigned long int)frame->frameID);
        VmbFrame_t * q_frame = malloc(sizeof(VmbFrame_t));
        q_frame->buffer = (unsigned char*)malloc( (VmbUint32_t)frame->bufferSize);
        q_frame->bufferSize = frame->bufferSize;
	q_frame->frameID = frame->frameID;
	q_frame->receiveFlags = frame->receiveFlags;
        q_frame->timestamp = frame->timestamp;	
        memcpy(q_frame->buffer, frame->buffer, frame->bufferSize);
        g_async_queue_push(frame_queue, q_frame);
    } else if (VmbFrameStatusIncomplete == frame->receiveStatus) {
        g_message("Frame %lu incomplete", (unsigned long int) frame->frameID);
    } else if (VmbFrameStatusTooSmall == frame->receiveStatus) {
        g_message("Frame %lu too small", (unsigned long int) frame->frameID);
    } else if (VmbFrameStatusInvalid == frame->receiveStatus) {
        g_message("Frame %lu invalid", (unsigned long int) frame->frameID);
    } else {
        g_message(
            "Error receiving frame %lu", (unsigned long int) frame->frameID
        );
    }
       VmbCaptureFrameQueue(
          camera_handle,
          frame,
          &frame_callback);
}

VmbFrame_t * vimbacamera_consume_frame(VimbaCamera * camera) {
    if (camera->acquiring == FALSE) {
        return NULL;
    }
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
        g_message("Invalid frame");
    } else if (VmbErrorStructSize == err) {
        g_error("Invalid struct size for current frame");
    } else if (VmbErrorTimeout == err) {
        g_error("Call timed out");
    } else if (VmbErrorApiNotStarted == err) {
        g_error("Api not started");
    }
}


VimbaCamera* vimbacamera_init() {
    VimbaCamera* camera = malloc(sizeof(VimbaCamera));
    camera->acquiring = FALSE;
    camera->streaming_started = FALSE;
    camera->open = FALSE;
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
    VmbBool_t IsCommandDone = VmbBoolFalse;
    
    if (VmbErrorSuccess == err) {
        g_message("success!");
        camera->open = TRUE;
        // Set the GeV packet size to the highest possible value
        // (In this example we do not test whether this cam actually is a GigE cam)
        if ( VmbErrorSuccess == VmbFeatureCommandRun( camera->camera_handle, "GVSPAdjustPacketSize" )) {
            do {
                if ( VmbErrorSuccess != VmbFeatureCommandIsDone(    camera->camera_handle,
            							"GVSPAdjustPacketSize",
            							&IsCommandDone ))
                {
            	break;
                }
            } while ( VmbBoolFalse == IsCommandDone );
        }

        //VmbFeatureIntSet(camera->camera_handle, "GevSCPSPacketSize", 1500);
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
    if (camera->open == TRUE) {
        g_message("vimbacamera_close");
        err = VmbCameraClose(camera->camera_handle);
        if (err != VmbErrorSuccess) {
            return FALSE;
        }
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

    if (camera->open == FALSE) {
        return TRUE;
    }
    VmbError_t err;
    int i;

    g_message("vimbacamera_start");

    /* Reset base time (should be set when reading the first frame) */
    camera->base_time = 0;

    /* Create global frame queue that's going to be used by gstreamer */
    frame_queue = g_async_queue_new();

    err = VmbFeatureIntGet(
        camera->camera_handle,
        "PayloadSize",
         &camera->payload_size
    );

    g_message("payload %lu, %d", (unsigned int)camera->payload_size, err);
    memset( camera->frames, 0, sizeof( camera->frames ));

    /* create and announce frame buffers */
    for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
        //memset(&camera->frames[i], 0, sizeof(VmbFrame_t));
        camera->frames[i].buffer = (unsigned char*)malloc(
                                       (VmbUint32_t)camera->payload_size
                                   );
        camera->frames[i].bufferSize = (VmbUint32_t)camera->payload_size;
        /* Somehow announcing a frame prevented the api from working */
        err = VmbFrameAnnounce(
            camera->camera_handle,
            &camera->frames[i],
            sizeof(VmbFrame_t)
        );
	if (err != VmbErrorSuccess) {
	    g_message("error from frame announce %d", err);
	}
    }

    /* Start capture engine */
    err = VmbCaptureStart(camera->camera_handle);
    if (VmbErrorSuccess == err) {

        camera->streaming_started = TRUE;
        /* Queue frames */
        for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
            vimbacamera_queue_frame(camera, &camera->frames[i]);
        }
        
        /* Continuous frame grabbing (in contrast to single frame capture) */
        //err = VmbFeatureEnumSet(
        //    camera->camera_handle,
        //    "AcquisitionMode",
        //    "Continuous"
        //);
        
        /* Start aquisition */
        err = VmbFeatureCommandRun(camera->camera_handle, "AcquisitionStart");
        if (VmbErrorSuccess == err) {
            g_message("Acquisition started");
            camera->acquiring = TRUE;
        } else if (VmbErrorInvalidAccess == err) {
            g_error("Operation not valid with curren access mode");
        } else if (VmbErrorWrongType == err) {
            g_error("Not a command");
        }
    }
    if( VmbErrorSuccess != err ) {
        vimbacamera_stop(camera);
        return FALSE;
    }
    return TRUE;
}

gboolean vimbacamera_stop (VimbaCamera * camera) {
    if (camera->open == FALSE) {
        return TRUE;
    }
    VmbError_t err;
    int i;
    g_message("vimbacamera_stop");
    if (camera->camera_handle != NULL) {
        if (camera->acquiring) {
            err = VmbFeatureCommandRun( camera->camera_handle, "AcquisitionStop");
            if (VmbErrorSuccess != err) {
                g_message("failed to AcquisitionStop");
            }
            g_message("Acquisition stopped");
            camera->acquiring = FALSE;
        }
        if (camera->streaming_started) {
            err = VmbCaptureEnd(camera->camera_handle);
            if (VmbErrorSuccess != err) {
                g_message("failed to VmbCaptureEnd");
            }
            camera->streaming_started = FALSE;
        }
        VmbCaptureQueueFlush(camera->camera_handle);

        for (i = 0; i < VIMBA_FRAME_COUNT; i++) {
            if( NULL != camera->frames[i].buffer ) {
                VmbFrameRevoke( camera->camera_handle, &camera->frames[i] );
                free(camera->frames[i].buffer);
                memset( &camera->frames[i], 0, sizeof( VmbFrame_t ));
            }

        }
    
        g_async_queue_unref(frame_queue);
    }
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

#define VMB_HANDLE_FEATURE_ERROR(err) \
    if (VmbErrorBadHandle == err) { \
        g_message("You have to open the camera before setting any features!"); \
    } else if (VmbErrorWrongType == err) { \
        g_error("%s has the wrong type!", name); \
    } \

void vimbacamera_set_feature_int(
    VimbaCamera * camera, const char * name, int value
) {
    VmbError_t err = VmbFeatureIntSet(camera->camera_handle, name, value);
    VMB_HANDLE_FEATURE_ERROR(err);
}

long long vimbacamera_get_feature_int(VimbaCamera * camera, const char * name) {
    long long value = 0;
    VmbError_t err = VmbFeatureIntGet(camera->camera_handle, name, &value);
    VMB_HANDLE_FEATURE_ERROR(err);
    return value;
}

void vimbacamera_list_features(VimbaCamera * camera) {
    VmbFeatureInfo_t *features;
    VmbUint32_t i, string_size = 0, count = 0;
    VmbInt64_t int_value = 0;
    char bool_value = 'f';
    double float_value = 0;
    const char * enum_value = NULL;
    char buffer[1024];
    char value_string[1024];
    VmbError_t err;
    FILE * outfile = fopen("/tmp/features.txt", "w");

    err = VmbFeaturesList(
        camera->camera_handle, NULL, 0, &count, sizeof *features
    );

    if (VmbErrorSuccess == err && 0 < count) {
        features = malloc(count * sizeof(VmbFeatureInfo_t));
        err = VmbFeaturesList(
            camera->camera_handle, features, count, &count, sizeof(*features)
        );
        for (i = 0; i < count; ++i) {
            switch(features[i].featureDataType) {
                case VmbFeatureDataInt:
                    VmbFeatureIntGet(
                        camera->camera_handle,
                        features[i].name,
                        &int_value
                    );
                    sprintf(value_string, "%lld", int_value);
                    break;
                case VmbFeatureDataBool:
                    VmbFeatureBoolGet(
                        camera->camera_handle,
                        features[i].name,
                        &bool_value
                    );
                    sprintf(value_string, "%c", bool_value);
                    break;
                case VmbFeatureDataFloat:
                    VmbFeatureFloatGet(
                        camera->camera_handle,
                        features[i].name,
                        &float_value
                    );
                    sprintf(value_string, "%f", float_value);
                    break;
                case VmbFeatureDataEnum:
                    VmbFeatureEnumGet(
                        camera->camera_handle,
                        features[i].name,
                        &enum_value
                    );
                    sprintf(value_string, "%s", enum_value);
                    break;
                case VmbFeatureDataString:
                    VmbFeatureStringGet(
                        camera->camera_handle,
                        features[i].name,
                        buffer,
                        1024,
                        &string_size
                    );
                    sprintf(value_string, "%s", buffer);
                    break;
                default:
                    break;
            }
            fprintf(
                outfile,
                "Feature %s: %s\n",
                features[i].name,
                value_string
            );
        }
    }
    fclose(outfile);
}

