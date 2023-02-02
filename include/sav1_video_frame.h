#ifndef SAV1_VIDEO_FRAME_H
#define SAV1_VIDEO_FRAME_H

#include <cstdint>

// duplicate declaration
typedef struct Sav1Context Sav1Context;

typedef struct Sav1VideoFrame {
    uint8_t *data;
    size_t size;
    ptrdiff_t stride;
    size_t width;
    size_t height;
    uint64_t timecode;
    uint8_t color_depth;
    int codec;
    int pixel_format;
} Sav1VideoFrame;

int
sav1_video_frame_destroy(Sav1Context *context, Sav1VideoFrame *frame);

int
sav1_video_frame_clone(Sav1Context *context, Sav1VideoFrame *src_frame,
                       Sav1VideoFrame **dst_frame);

#endif
