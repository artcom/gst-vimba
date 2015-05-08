#ifndef _VIMBASRC_PIXELFORMAT_H_
#define _VIMBASRC_PIXELFORMAT_H_

#include <string.h>
#include <stdlib.h>

enum VimbaRawFormats
{
    MONO8,
    RGB8PACKED,
    BGR8PACKED,
    RGBA8PACKED,
    BGRA8PACKED,
    YUV411PACKED,
    YUV422PACKED,
    YUV444PACKED,
    RAW_FORMAT_COUNT
};
typedef enum VimbaRawFormats VimbaFormat_t;

enum VimbaBayerFormats
{
    BAYERGB8,
    BAYERRG8,
    BAYERGR8,
    BAYERBG8,
    BAYER_FORMAT_COUNT
};
typedef enum VimbaBayerFormats VimbaBayerFormat_t;

void vimbasrc_supported_formats(
    const char ** camera_formats,
    int length,
    const char ** supported_formats,
    int format_count,
    const char ** output,
    int * output_length
);
void vimbasrc_supported_raw_formats(
    const char ** camera_formats,
    int length,
    const char ** output,
    int * output_length
);
void vimbasrc_supported_bayer_formats(
    const char ** camera_formats,
    int length,
    const char ** output,
    int * output_length
);
const char* vimbasrc_match_formats(
    const char * format,
    const char ** input_list,
    const char** output_list,
    int length
);
const char* vimbasrc_gstreamer_to_vimba_bayer(const char * format);
const char* vimbasrc_vimba_to_gstreamer_bayer(const char * format);
const char* vimbasrc_gstreamer_to_vimba_raw(const char * format);
const char* vimbasrc_vimba_to_gstreamer_raw(const char * format);

#endif
