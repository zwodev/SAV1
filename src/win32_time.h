// nanosleep(), clock_gettime(), clock_getres() for Windows
// MIT License, SciVision, Inc. 2025

#include <time.h>

#ifndef clockid_t
#define clockid_t int
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif


int nanosleep(const struct timespec*, struct timespec*);

int clock_gettime(clockid_t, struct timespec*);

int clock_getres(clockid_t, struct timespec*);
