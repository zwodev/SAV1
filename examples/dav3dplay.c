#include "thread_manager.h"
#include "thread_queue.h"
#include "sav1_video_frame.h"
#include "sav1_audio_frame.h"
#include "sav1_settings.h"

#include <stdio.h>
#include <time.h>
#include <math.h>

#include <dav1d/dav1d.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

void *
video_postprocessing_func(Sav1VideoFrame *frame, void *cookie)
{
    // pre-compute the brightness of each pixel
    uint8_t *avg = (uint8_t *)malloc(frame->width * frame->height * sizeof(uint8_t));
    for (size_t y = 0; y < frame->height; y++) {
        for (size_t x = 0; x < frame->width; x++) {
            size_t pos = y * frame->stride + x * 4;
            size_t avg_pos = y * frame->width + x;
            avg[avg_pos] =
                (frame->data[pos] + frame->data[pos + 1] + frame->data[pos + 2]) / 3;
        }
    }

    // define the edge-detection convolutions
    int x_con[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    int y_con[3][3] = {{1, 2, 1}, {0, 0, 0}, {-1, -2, -1}};

    for (size_t y = 0; y < frame->height; y++) {
        for (size_t x = 0; x < frame->width; x++) {
            size_t pos = y * frame->stride + x * 4;
            if (x == 0 || x == frame->width - 1 || y == 0 || y == frame->height - 1) {
                frame->data[pos] = 0;
                frame->data[pos + 1] = 0;
                frame->data[pos + 2] = 0;
                continue;
            }

            int x_grad = 0;
            int y_grad = 0;

            // look at 8 surrounding pixels for every pixel
            for (int y_offset = -1; y_offset <= 1; y_offset++) {
                for (int x_offset = -1; x_offset <= 1; x_offset++) {
                    // compute horizontal and vertical gradients
                    size_t con_pos = (y + y_offset) * frame->width + x + x_offset;
                    x_grad += x_con[1 + y_offset][1 + x_offset] * avg[con_pos];
                    y_grad += y_con[1 + y_offset][1 + x_offset] * avg[con_pos];
                }
            }

            uint16_t grad = abs(x_grad) + abs(y_grad);
            if (grad > 255) {
                grad = 255;
            }

            // set the brightness of the pixel to the gradient
            frame->data[pos] = grad;
            frame->data[pos + 1] = grad;
            frame->data[pos + 2] = grad;
        }
    }

    free(avg);
    return (void *)frame;
}

void *
audio_postprocessing_func(Sav1AudioFrame *frame, void *cookie)
{
    // sextuple the volume
    for (int i = 0; i < frame->size; i += 2) {
        uint64_t sample = frame->data[i] | frame->data[i + 1] << 8;
        sample *= 6;
        if (sample > 65535) {
            sample = 65535;
        }
        frame->data[i] = sample;
        frame->data[i + 1] = sample >> 8;
    }

    return (void *)frame;
}

void
switch_window_surface(SDL_Window *window, SDL_Surface **existing)
{
    SDL_FreeSurface(*existing);
    SDL_Surface *new_surf = SDL_GetWindowSurface(window);
    *existing = new_surf;
}

void
rect_fit(SDL_Rect *arg, SDL_Rect target)
{
    /* Stolen from pygame source: src_c/Rect.c */

    int w, h, x, y;
    float xratio, yratio, maxratio;

    xratio = (float)arg->w / (float)target.w;
    yratio = (float)arg->h / (float)target.h;
    maxratio = (xratio > yratio) ? xratio : yratio;

    // w = (int)(arg->w / maxratio);
    // h = (int)(arg->h / maxratio);
    w = (int)ceil(arg->w / maxratio);
    h = (int)ceil(arg->h / maxratio);
    x = target.x + (target.w - w) / 2;
    y = target.y + (target.h - h) / 2;

    arg->w = w;
    arg->h = h;
    arg->x = x;
    arg->y = y;
}

int
get_frame_display_ready(struct timespec *start_time, Sav1VideoFrame *frame)
{
    // quick and dirty check to see if we should display the next frame
    struct timespec curr_time;
    clock_gettime(CLOCK_MONOTONIC, &curr_time);
    uint64_t total_elapsed_ms = ((curr_time.tv_sec - start_time->tv_sec) * 1000) +
                                ((curr_time.tv_nsec - start_time->tv_nsec) / 1000000);

    return total_elapsed_ms >= frame->timecode;
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Error: No input file specified\n");
        exit(1);
    }

    int screen_width = 1200;
    int screen_height = 760;
    int frame_width, frame_height;

    SDL_Init(SDL_INIT_EVERYTHING);

    SDL_AudioSpec desired = {0};
    desired.freq = 24000;
    desired.format = AUDIO_S32LSB;
    desired.channels = 1; /* Only single channel, despite source being stereo? */
    desired.samples = 4096;
    desired.callback = NULL;

    SDL_AudioSpec obtained = {0};
    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);

    SDL_Window *window = SDL_CreateWindow("Dav3d video player", SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED, screen_width,
                                          screen_height, SDL_WINDOW_RESIZABLE);
    SDL_Surface *screen = SDL_GetWindowSurface(window);
    SDL_Rect screen_rect = {0, 0, screen_width, screen_height};

    SDL_Surface *frame = NULL;
    SDL_Rect frame_rect;

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    struct timespec *pause_time = NULL;

    /* start audio device */
    SDL_PauseAudioDevice(audio_device, 0);

    int running = 1;
    int needs_initial_resize = 1;
    SDL_Event event;

    Sav1Settings settings;
    sav1_default_settings(&settings, argv[1]);
    settings.desired_pixel_format = SAV1_PIXEL_FORMAT_BGRA;
    // sav1_settings_use_custom_video_processing(&settings, video_postprocessing_func,
    // NULL); sav1_settings_use_custom_audio_processing(&settings,
    // audio_postprocessing_func, NULL);

    ThreadManager *manager;
    Sav1VideoFrame *sav1_frame = NULL;
    thread_manager_init(&manager, &settings);
    thread_manager_start_pipeline(manager);
    uint8_t *pixel_buffer = NULL;
    uint8_t *next_pixel_buffer = NULL;

    while (running) {
        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 103, 155, 203));

        if (pause_time == NULL) {
            if (sav1_frame == NULL) {
                sav1_frame =
                    (Sav1VideoFrame *)sav1_thread_queue_pop(manager->video_output_queue);
                if (sav1_frame == NULL) {
                    running = 0;
                }
                else {
                    frame_width = sav1_frame->width;
                    frame_height = sav1_frame->height;
                    if (needs_initial_resize) {
                        SDL_SetWindowSize(window, frame_width, frame_height);
                        needs_initial_resize = 0;
                    }
                    next_pixel_buffer = sav1_frame->data;
                }
            }
            else if (get_frame_display_ready(&start_time, sav1_frame)) {
                SDL_FreeSurface(frame);
                if (pixel_buffer != NULL) {
                    free(pixel_buffer);
                }
                pixel_buffer = next_pixel_buffer;
                frame = SDL_CreateRGBSurfaceWithFormatFrom(
                    (void *)pixel_buffer, frame_width, frame_height, 32,
                    sav1_frame->stride, SDL_PIXELFORMAT_BGRA32);
                free(sav1_frame);
                sav1_frame = NULL;
            }
        }

        /* SDL Queue Audio does unlimited queueing, so for now we just want to get
         * everything out and queued ASAP */
        Sav1AudioFrame *audio_frame;
        while ((audio_frame = (Sav1AudioFrame *)sav1_thread_queue_pop_timeout(
                    manager->audio_output_queue)) != NULL) {
            SDL_QueueAudio(audio_device, audio_frame->data, audio_frame->size);
            free(audio_frame->data);
            free(audio_frame);
        }

        if (frame) {
            frame_rect.x = 0;
            frame_rect.y = 0;
            frame_rect.w = frame_width;
            frame_rect.h = frame_height;
            rect_fit(&frame_rect, screen_rect);
            SDL_BlitScaled(frame, NULL, screen, &frame_rect);
        }
        SDL_UpdateWindowSurface(window);

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = 0;
                    }
                    else if (event.key.keysym.sym == SDLK_SPACE) {
                        if (pause_time == NULL) {
                            // pause the video
                            SDL_PauseAudioDevice(audio_device, 1);
                            pause_time =
                                (struct timespec *)malloc(sizeof(struct timespec));
                            clock_gettime(CLOCK_MONOTONIC, pause_time);
                        }
                        else {
                            // unpause the video
                            SDL_PauseAudioDevice(audio_device, 0);
                            struct timespec curr_time;
                            clock_gettime(CLOCK_MONOTONIC, &curr_time);
                            start_time.tv_sec += curr_time.tv_sec - pause_time->tv_sec;
                            start_time.tv_sec +=
                                (curr_time.tv_nsec - pause_time->tv_nsec) / 1000000000;
                            start_time.tv_nsec +=
                                (curr_time.tv_nsec - pause_time->tv_nsec) % 1000000000;

                            free(pause_time);
                            pause_time = NULL;
                        }
                    }
                    else if (event.key.keysym.sym == SDLK_1 && sav1_frame) {
                        SDL_SetWindowSize(window, frame_width / 2, frame_height / 2);
                    }
                    else if (event.key.keysym.sym == SDLK_2 && sav1_frame) {
                        SDL_SetWindowSize(window, 2 * frame_width / 3,
                                          2 * frame_height / 3);
                    }
                    else if (event.key.keysym.sym == SDLK_3 && sav1_frame) {
                        SDL_SetWindowSize(window, frame_width, frame_height);
                    }
                    else if (event.key.keysym.sym == SDLK_4 && sav1_frame) {
                        SDL_SetWindowSize(window, 3 * frame_width / 2,
                                          3 * frame_height / 2);
                    }
                    else if (event.key.keysym.sym == SDLK_5 && sav1_frame) {
                        SDL_SetWindowSize(window, 2 * frame_width, 2 * frame_height);
                    }
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        screen_width = event.window.data1;
                        screen_height = event.window.data2;
                        screen_rect.w = screen_width;
                        screen_rect.h = screen_height;

                        SDL_FreeSurface(screen);
                        screen = SDL_GetWindowSurface(window);
                    }

                default:
                    break;
            }
        }
    }

    thread_manager_kill_pipeline(manager);
    thread_manager_destroy(manager);
    if (pixel_buffer != NULL) {
        free(pixel_buffer);
    }
    if (pause_time != NULL) {
        free(pause_time);
    }

    SDL_Quit();
}
