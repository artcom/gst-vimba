#ifndef _VIMBA_H_
#define _VIMBA_H_

#include "VimbaC.h"
#include "gst/gst.h"

#define GST_VIMBA_SRC_MAXFORMATS 64
#define VIMBA_FRAME_COUNT 3

typedef struct _VimbaCamera VimbaCamera;
struct _VimbaCamera {
    const char*  camera_id;
    VmbHandle_t  camera_handle;
    VmbInt64_t   max_width;
    VmbInt64_t   max_height;
    VmbInt64_t   width;
    VmbInt64_t   height;
    VmbFrame_t   frames[VIMBA_FRAME_COUNT];
    VmbInt64_t   payload_size;
    const char*  format;
    const char*  supported_formats[GST_VIMBA_SRC_MAXFORMATS];
    VmbUint32_t  format_count;
    gboolean     started;
};

VimbaCamera* vimbacamera_init();
void     vimbacamera_destroy (VimbaCamera * camera);
gboolean vimbacamera_open (VimbaCamera * camera);
gboolean vimbacamera_close (VimbaCamera * camera);
gboolean vimbacamera_load (VimbaCamera * camera);
gboolean vimbacamera_start (VimbaCamera * camera);
gboolean vimbacamera_stop (VimbaCamera * camera);
void vimbacamera_capture (VimbaCamera * camera);

#endif
