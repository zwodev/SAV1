// nanosleep(), clock_gettime(), clock_getres() for Windows
// MIT License, SciVision, Inc. 2025

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "win32_time.h"

static const long G = 1000000000L;
// 1 second in nanoseconds

#ifndef nullptr
#define nullptr NULL
#endif


int nanosleep(const struct timespec* ts, struct timespec* rem)
{
  // rem is not implemented
  rem = nullptr;

  HANDLE timer = CreateWaitableTimer(nullptr, TRUE, nullptr);
  if(!timer)
      return -1;

// SetWaitableTimer() defines interval in 100ns units.
// negative is to indicate relative time.
  time_t sec = ts->tv_sec;
  long nsec = ts->tv_nsec;
  if (sec < 0 || (sec == 0 && nsec <= 0)) {
    CloseHandle(timer);
    return 0;
  }

  LARGE_INTEGER delay;
  delay.QuadPart = -((LONGLONG)sec * 10000000LL + (LONGLONG)nsec / 100LL);
  BOOL ok = SetWaitableTimer(timer, &delay, 0, nullptr, nullptr, FALSE) &&
            WaitForSingleObject(timer, INFINITE) == WAIT_OBJECT_0;

  CloseHandle(timer);

  if(!ok)
    return -1;

  return 0;
}


static int win32_gettime_monotonic(struct timespec *t){
  LARGE_INTEGER count;
  static LARGE_INTEGER freq = {-1, -1};

  if (freq.QuadPart == -1) {
    if (!QueryPerformanceFrequency(&freq))
      return -1;
  }

  if(!QueryPerformanceCounter(&count))
    return -1;

  t->tv_sec = count.QuadPart / freq.QuadPart;
  t->tv_nsec = ((count.QuadPart % freq.QuadPart) * G) / freq.QuadPart;

  return 0;
}

static int win32_gettime_realtime(struct timespec *t){
  FILETIME ft;
  ULARGE_INTEGER ht;

  GetSystemTimePreciseAsFileTime(&ft);

  ht.LowPart = ft.dwLowDateTime;
  ht.HighPart = ft.dwHighDateTime;

  // FILETIME is in 100ns units since Jan 1, 1601
  // Convert to seconds since Unix epoch (Jan 1, 1970)
  t->tv_sec = (time_t)((ht.QuadPart - 116444736000000000ULL) / 10000000ULL);
  t->tv_nsec = (long)((ht.QuadPart % 10000000ULL) * 100ULL);

  return 0;
}


int clock_gettime(clockid_t clk_id, struct timespec *t){

switch (clk_id) {
  case CLOCK_MONOTONIC:
    return win32_gettime_monotonic(t);
  case CLOCK_REALTIME:
    return win32_gettime_realtime(t);
  default:
    return -1;
}
}


int clock_getres(clockid_t clk_id, struct timespec *res)
{
  LARGE_INTEGER freq;

  res->tv_sec = 0;

  switch (clk_id) {
    case CLOCK_MONOTONIC:
      if(!QueryPerformanceFrequency(&freq))
        return -1;
      break;
    case CLOCK_REALTIME:
      freq.QuadPart = 10000000LL;
      // 100ns resolution for FILETIME
      // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime
      break;
    default:
      return -1;
  }


  res->tv_nsec = (G / freq.QuadPart);

  return 0;
}
