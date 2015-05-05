#include "vimbacamera.h"

typedef struct _Vimba Vimba;

struct _Vimba {
    VmbCameraInfo_t * camera_list;
    VmbUint32_t count;
};

Vimba* vimba_init();
void vimba_destroy(Vimba* vimba);
void vimba_discover(Vimba * vimba);
