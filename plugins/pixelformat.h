#include <string.h>

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

const char* GST_RAW_FORMATS[RAW_FORMAT_COUNT] = {
    "GRAY8",
    "RGBx",
    "BGRx",
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

enum VimbaBayerFormats
{
    BAYERGB8,
    BAYERRG8,
    BAYERGR8,
    BAYERBG8,
    BAYER_FORMAT_COUNT
};
typedef enum VimbaBayerFormats VimbaBayerFormat_t;

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

int 
vimbasrc_is_bayer(const char * input_format, const char ** format_list) {
    int i;
    for (i = 0; i < BAYER_FORMAT_COUNT; i++) {
        if (strcmp(input_format, format_list[i])) {
            return 1;
        }
    }
    return 0;
}

const char*
vimbasrc_match_formats(const char * format, const char ** input_list,
        const char** output_list, int length) 
{
    int i;
    for (i = 0; i < length; i++) {
        if (strcmp(format, input_list[i])) {
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
