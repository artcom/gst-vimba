#include "vimba.h"
#include <stdlib.h>

Vimba* vimba_init() {
    Vimba* vimba = malloc(sizeof(Vimba));
    vimba->camera_list = 0;
    VmbError_t err = VmbStartup();
    if (VmbErrorInternalFault == err) {
        g_error("Error initializing VIMBA");
        exit(EXIT_FAILURE);
    }
    return vimba;
}

void vimba_destroy(Vimba * vimba) {
    VmbShutdown();
    if (vimba) {
        free(vimba->camera_list);
        free(vimba);
    }
    vimba = NULL;
}

void vimba_discover(Vimba* vimba) {
    /* Look for any attached cameras */
    VmbBool_t gigE;
    VmbUint32_t i;
    VmbError_t err = VmbFeatureBoolGet(gVimbaHandle, "GeVTLIsPresent", &gigE);
    if (VmbErrorSuccess == err) {
        if (VmbBoolTrue == gigE) {
            err = VmbFeatureCommandRun(gVimbaHandle, "GeVDiscoveryAllOnce");
        }
    } else {
        g_error("No transport layer");
    }
    if (VmbErrorSuccess == err) {
        if (vimba->camera_list) {
            free(vimba->camera_list);
            vimba->camera_list = NULL;
        }
        err = VmbCamerasList(NULL, 0, &vimba->count, sizeof(VmbCameraInfo_t) );
        g_message("Found %d cameras", vimba->count);
        if (VmbErrorSuccess == err) {
            vimba->camera_list = (VmbCameraInfo_t*) malloc(
                                     vimba->count * sizeof(VmbCameraInfo_t)
                                 );
            err = VmbCamerasList(
                      vimba->camera_list,
                      vimba->count,
                      &vimba->count,
                      sizeof(VmbCameraInfo_t)
                  );
            g_message("Found the following cameras:\n");
            for (i = 0; i < vimba->count; ++i) {
                g_message("\t%s\n", vimba->camera_list[i].cameraIdString);
            }
        }
    } else {
        g_error("Unable to discover cameras");
    }
}
