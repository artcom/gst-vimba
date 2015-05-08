#include "pixelformat.h"
#include <stdio.h>

const char* GST_RAW_FORMATS[RAW_FORMAT_COUNT] = {
    "GRAY8",
    "RGB",
    "BGR",
    "RGBA",
    "BGRA",
    "Y41P",
    "YUY2",
    "IYU2"
};

const char* VIMBA_RAW_FORMATS[RAW_FORMAT_COUNT] = {
    "Mono8",
    "RGB8Packed",
    "BGR8Packed",
    "RGBA8Packed",
    "BGRA8Packed",
    "YUV411Packed",
    "YUV422Packed",
    "YUV444Packed"
};
const char * GST_BAYER_FORMATS[BAYER_FORMAT_COUNT] = {
    "gbrg",
    "rggb",
    "grbg",
    "bggr"
};

const char * VIMBA_BAYER_FORMATS[BAYER_FORMAT_COUNT] = {
    "BayerGB8",
    "BayerRG8",
    "BayerGR8",
    "BayerBG8"
};

void
vimbasrc_supported_formats(
    const char ** camera_formats,
    int length,
    const char ** supported_formats,
    int format_count,
    const char ** output,
    int * output_length
) {
    int i, j, count = 0;
    for (i = 0; i < length; i++) {
        for (j = 0; j < format_count; j++) {
            if (strcmp(camera_formats[i], supported_formats[j]) == 0) {
                output[count++] = camera_formats[i];
                continue;
            }
        }
    }
    *output_length = count;
}

void vimbasrc_supported_raw_formats(
    const char ** camera_formats,
    int length,
    const char ** output,
    int * output_length)
{
    vimbasrc_supported_formats(
        camera_formats,
        length,
        VIMBA_RAW_FORMATS,
        RAW_FORMAT_COUNT,
        output,
        output_length
    );
}
void vimbasrc_supported_bayer_formats(
    const char ** camera_formats,
    int length,
    const char ** output,
    int * output_length)
{
    vimbasrc_supported_formats(
        camera_formats,
        length,
        VIMBA_BAYER_FORMATS,
        BAYER_FORMAT_COUNT,
        output,
        output_length
    );
}

const char* vimbasrc_match_formats(
    const char * format,
    const char ** input_list,
    const char** output_list,
    int length
) {
    int i;
    for (i = 0; i < length; i++) {
        if (strcmp(format, input_list[i]) == 0) {
            return output_list[i];
        }
    }
    return NULL;
}

const char*
vimbasrc_gstreamer_to_vimba_bayer(const char * format) {
    return vimbasrc_match_formats(
        format,
        GST_BAYER_FORMATS,
        VIMBA_BAYER_FORMATS,
        BAYER_FORMAT_COUNT
    );
}

const char*
vimbasrc_vimba_to_gstreamer_bayer(const char * format) {
    return vimbasrc_match_formats(
        format,
        VIMBA_BAYER_FORMATS,
        GST_BAYER_FORMATS,
        BAYER_FORMAT_COUNT
    );
}

const char*
vimbasrc_gstreamer_to_vimba_raw(const char * format) {
    return vimbasrc_match_formats(
        format,
        GST_RAW_FORMATS,
        VIMBA_RAW_FORMATS,
        RAW_FORMAT_COUNT
    );
}

const char*
vimbasrc_vimba_to_gstreamer_raw(const char * format) {
    return vimbasrc_match_formats(
        format,
        VIMBA_RAW_FORMATS,
        GST_RAW_FORMATS,
        RAW_FORMAT_COUNT
    );
}
