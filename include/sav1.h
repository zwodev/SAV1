#ifndef SAV1_H
#define SAV1_H

#include "sav1_settings.h"
#include "sav1_video_frame.h"
#include "sav1_audio_frame.h"

typedef struct Sav1Context {
    void *internal_state;
} Sav1Context;

int
sav1_create_context(Sav1Context *context, Sav1Settings *settings);

int
sav1_destroy_context(Sav1Context *context);

char *
sav1_get_error(Sav1Context *context);

int
sav1_get_video_frame(Sav1Context *context, Sav1VideoFrame **frame);

int
sav1_get_audio_frame(Sav1Context *context, Sav1AudioFrame **frame);

int
sav1_get_video_frame_ready(Sav1Context *context, int *is_ready);

int
sav1_get_audio_frame_ready(Sav1Context *context, int *is_ready);

int
sav1_start_playback(Sav1Context *context);

int
sav1_pause_playback(Sav1Context *context, int pause);

int
sav1_get_paused(Sav1Context *context);

int
sav1_seek_playback(Sav1Context *context, uint64_t timecode_ms);

#endif