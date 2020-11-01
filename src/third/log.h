#ifndef _LOG_H_
#define _LOG_H_

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#include "windows.h"
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>

#ifdef __WINDOWS__
#if defined COMMON_LIB || defined STRESS_EXPORTS
#define log_API __declspec(dllexport)
#else
#define log_API __declspec(dllimport)
#endif // COMMON_LIB
#else
#define log_API
#endif

#ifdef __WINDOWS__
#define THREAD_ID GetCurrentThreadId()
#else
#define THREAD_ID syscall(SYS_gettid)
#endif

#define MAX_LOG_LEN	1024

enum loglevel {LOG_TRACE = 0, LOG_DEBUG, LOG_NORMAL, LOG_ERROR, LOG_FAULT, LOG_NONE}; 

#ifdef __cplusplus
extern "C"
{
#endif

log_API int				RegisterLog(const char* filename, int level, int rotatesize, int rotateintval, int filenum);
log_API int				CloseLog(int fd);
log_API char*			GetLogStr(int loglevel);
log_API int				GetLogLevel(const char* loglevelstr) ;
log_API void			SetLogLevel(int fd, int level);
#ifdef __WINDOWS__
log_API HANDLE			GetLogFileHandle(int fd);
#else
log_API int             GetLogFileHandle(int fd);
#endif
log_API int				CheckLogLevel(int fd, int level);
log_API void			LOG(int fd, int level, const char* fmt, ...);
log_API void			LogLoop();

#ifdef __cplusplus
}
#endif
#endif
