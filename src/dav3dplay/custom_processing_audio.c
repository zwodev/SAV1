#include <cstdlib>

#include "thread_queue.h"
#include "custom_processing_audio.h"

void
custom_processing_audio_init(CustomProcessingAudioContext **context,
                             void *(*process_function)(Sav1AudioFrame *, void *),
                             void *cookie, Sav1ThreadQueue *input_queue,
                             Sav1ThreadQueue *output_queue)
{
    CustomProcessingAudioContext *process_context =
        (CustomProcessingAudioContext *)malloc(sizeof(CustomProcessingAudioContext));
    *context = process_context;

    process_context->process_function = process_function;
    process_context->cookie = cookie;
    process_context->input_queue = input_queue;
    process_context->output_queue = output_queue;
}

void
custom_processing_audio_destroy(CustomProcessingAudioContext *context)
{
    free(context);
}

int
custom_processing_audio_start(void *context)
{
    CustomProcessingAudioContext *process_context =
        (CustomProcessingAudioContext *)context;
    thread_atomic_int_store(&(process_context->do_process), 1);

    while (thread_atomic_int_load(&(process_context->do_process))) {
        Sav1AudioFrame *input_frame =
            (Sav1AudioFrame *)sav1_thread_queue_pop(process_context->input_queue);

        if (input_frame == NULL) {
            sav1_thread_queue_push(process_context->output_queue, NULL);
            break;
        }

        void *output_frame =
            process_context->process_function(input_frame, process_context->cookie);

        sav1_thread_queue_push(process_context->output_queue, output_frame);
    }
    return 0;
}

void
custom_processing_audio_stop(CustomProcessingAudioContext *context)
{
    thread_atomic_int_store(&(context->do_process), 0);

    // add a fake entry to the input queue if necessary
    if (sav1_thread_queue_get_size(context->input_queue) == 0) {
        sav1_thread_queue_push_timeout(context->input_queue, NULL);
    }

    // drain the output queue
    while (1) {
        Sav1AudioFrame *frame =
            (Sav1AudioFrame *)sav1_thread_queue_pop_timeout(context->output_queue);
        if (frame == NULL) {
            break;
        }
        free(frame->data);
        free(frame);
    }
}