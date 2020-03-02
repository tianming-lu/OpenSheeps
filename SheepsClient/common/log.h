#ifndef _LOG_H_
#define _LOG_H_
#include "windows.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>

#if defined COMMON_LIB || defined STRESS_EXPORTS
#define log_API __declspec(dllexport)
#else
#define log_API __declspec(dllimport)
#endif // COMMON_LIB

#define MAX_LOG_LEN	10240

enum loglevel {LOG_TRACE = 0, LOG_DEBUG, LOG_NORMAL, LOG_ERROR, LOG_FAULT, LOG_NONE}; 

typedef struct {
	HANDLE	File;				//log File, internel variable		
	int		LogLevel;		//log level, caller set
	DWORD	RotateSize;		//rotate size(max log File size), caller set, 0 for disable
	time_t	RotateTime;		//internel variable
	int		RotateintVal;	//rotate interval time, caller set, 0 for disable	
	int		MaxLogNum;		//maximum log files number, caller set		
	int		NextLogNum;		//next log File no, internel variable
	char	FileName[256];		//log FileName, caller set
}LOG_Context, *HLOG_Context;

#ifdef __cplusplus
extern "C"
{
#endif

log_API int	RegisterLog(const char* filename, int level, int rotatesize, int rotateintval, int filenum);
log_API HLOG_Context	RegTaskLog(const char* filename, int level, int rotatesize, int rotateintval, int filenum);
log_API char*	GetLogStr(int loglevel);
log_API int	GetLogLevel(const char* loglevelstr) ;
log_API void	LOG(int fd, int level, const char* fmt, ...);
log_API void	LogLoop();

#ifdef __cplusplus
}
#endif
#endif
