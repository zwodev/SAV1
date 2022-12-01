#include "audio.h"

void
audio_init(AudioContext *context, int Fs, int channels, int error)
{
    AudioContext *audio_context = (AudioContext *)malloc(sizeof(AudioContext));
    context = audio_context;

    int max_size = 10000;  // Random number
    // opus_int16* decoded = (opus_int16*)calloc(max_size, sizeof(opus_int16));
    
    context->dec = opus_decoder_create(Fs, channels, &error);
    context->decoded = (opus_int16*)calloc(max_size, sizeof(opus_int16));
    context->Fs = Fs;
    context->channels = channels;
    context->error = error;
}

void
audio_destroy(AudioContext *context)
{
    opus_decoder_destroy(context->dec);
    free(context->decoded);
    free(context);
}

int 
audio_decode(AudioContext *context, uint8_t *data, size_t size)
{   
    int frame_size;
    int max_size = 10000;  // Random number

    // max size = frame_size*channels*sezeof(opus16_int)

    frame_size = opus_decode(context->dec, data, size, context->decoded, max_size, 0);

    return frame_size*context->channels*sizeof(opus_int16);
}
