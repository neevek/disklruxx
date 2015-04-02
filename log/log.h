#ifndef LOG_H_
#define LOG_H_
#include <string>
#include <stdarg.h>    // for va_list, va_start and va_end
#include <sys/time.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

#define MAX_FMT_SIZE 0xFF
#define TIME_BUFFER_SIZE 24

#if !defined(__ANDROID__) && \
    (defined(LOG_VERBOSE) || \
     defined(LOG_DEBUG) || \
     defined(LOG_INFO) || \
     defined(LOG_WARN) || \
     defined(LOG_ERROR))
static char *strtime(char *buffer) {
    timeval now;
    gettimeofday(&now, NULL);

    size_t len = strftime(buffer, TIME_BUFFER_SIZE, "%Y-%m-%d %H:%M:%S.", 
        localtime(&now.tv_sec));
    int milli = now.tv_usec / 1000;
    sprintf(buffer + len, "%03d", milli);

    return buffer;
}
#endif

#if defined(LOG_VERBOSE)
#ifdef __ANDROID__
#define LOG_V(tag, fmt, ...) \
{ \
  __android_log_print(ANDROID_LOG_VERBOSE, tag, "[KKNative] %s#%d - " fmt "\n", \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#else
#define LOG_V(tag, fmt, ...) \
{ \
  char _Buf_[TIME_BUFFER_SIZE];  \
  fprintf(stderr, "%s [V] [%s] %s#%d - " fmt "\n", strtime(_Buf_), tag, \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#endif
#else
#define LOG_V(tag, fmt, ...)
#endif

#if defined(LOG_VERBOSE) || defined(LOG_DEBUG)
#ifdef __ANDROID__
#define LOG_D(tag, fmt, ...) \
{ \
  __android_log_print(ANDROID_LOG_DEBUG, tag, "[KKNative] %s#%d - " fmt "\n", \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#else
#define LOG_D(tag, fmt, ...) \
{ \
  char _Buf_[TIME_BUFFER_SIZE];  \
  fprintf(stderr, "%s [D] [%s] %s#%d - " fmt "\n", strtime(_Buf_), tag, \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#endif
#else
#define LOG_D(tag, fmt, ...)
#endif

#if defined(LOG_VERBOSE) || defined(LOG_DEBUG) || defined(LOG_INFO)
#ifdef __ANDROID__
#define LOG_I(tag, fmt, ...) \
{ \
  __android_log_print(ANDROID_LOG_INFO, tag, "[KKNative] %s#%d - " fmt "\n", \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#else
#define LOG_I(tag, fmt, ...) \
{ \
  char _Buf_[TIME_BUFFER_SIZE];  \
  fprintf(stderr, "%s [I] [%s] %s#%d - " fmt "\n", strtime(_Buf_), tag, \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#endif
#else
#define LOG_I(tag, fmt, ...)
#endif

#if defined(LOG_VERBOSE) || defined(LOG_DEBUG) || defined(LOG_INFO) || \
    defined(LOG_WARN)
#ifdef __ANDROID__
#define LOG_W(tag, fmt, ...) \
{ \
  __android_log_print(ANDROID_LOG_WARN, tag, "[KKNative] %s#%d - " fmt "\n", \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#else
#define LOG_W(tag, fmt, ...) \
{ \
  char _Buf_[TIME_BUFFER_SIZE];  \
  fprintf(stderr, "%s [W] [%s] %s#%d - " fmt "\n", strtime(_Buf_), tag, \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#endif
#else
#define LOG_W(tag, fmt, ...)
#endif

#if defined(LOG_VERBOSE) || defined(LOG_DEBUG) || defined(LOG_INFO) || \
    defined(LOG_WARN) || defined(LOG_ERROR)
#ifdef __ANDROID__
#define LOG_E(tag, fmt, ...) \
{ \
  __android_log_print(ANDROID_LOG_ERROR, tag, "[KKNative] %s#%d - " fmt "\n", \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#else
#define LOG_E(tag, fmt, ...) \
{ \
  char _Buf_[TIME_BUFFER_SIZE];  \
  fprintf(stderr, "%s [E] [%s] %s#%d - " fmt "\n", strtime(_Buf_), tag, \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
}
#endif
#else
#define LOG_E(tag, fmt, ...)
#endif

#endif /* end of include guard: LOG_H_ */
