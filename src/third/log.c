#include "log.h"
#include <time.h>
#include "direct.h"

#define MAX_LOG	256
#define DEFAULT_MIN_LOG_NUM 2

#define NOWTIME time(NULL)		
char* LogString[] = {"TRACE", "DEBUG", "NORMAL", "ERROR", "FAULT", "NONE"};

#define LOGLOCK(a)  while (InterlockedExchange(a, 1)){Sleep(0);}
#define LOGUNLOCK(a)	InterlockedExchange(a, 0)

typedef struct {
	int		Lock;
	HANDLE	File;				//log File, internel variable		
	int		LogLevel;		//log level, caller set
	DWORD	RotateSize;		//rotate size(max log File size), caller set, 0 for disable
	time_t	RotateTime;		//internel variable
	int		RotateintVal;	//rotate interval time, caller set, 0 for disable	
	int		MaxLogNum;		//maximum log files number, caller set		
	int		NextLogNum;		//next log File no, internel variable
	char	FileName[256];		//log FileName, caller set
}LOG_Context, * HLOG_Context;

LOG_Context LogArray[MAX_LOG] = {0x0};
#define GETLOGLEVEL(fd) LogArray[fd].LogLevel
static u_char CurLogID = 0;


static HANDLE OpenLogFile(char* filename)
{
	HANDLE filefd = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE| FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);
	if (filefd == INVALID_HANDLE_VALUE)
	{
		char *s,*e;
		s = filename;
		while(1)
		{
			e = strchr(s, '\\');
			if(e == NULL)
				break;
			*e = 0x0;
			_mkdir(filename);
			*e = '/';
			s = e + 1;
		}
		filefd = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE| FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);
	}
	if (filefd != INVALID_HANDLE_VALUE)
	{
		SetFilePointer(filefd, 0, NULL, FILE_END);
	}
	return filefd;
}

static HANDLE ShiftFile(LOG_Context* ctx)
{
	char tmp[128] = {0};
	if(ctx->MaxLogNum > 2)
	{
		sprintf_s(tmp, 128, "%s.%d", ctx->FileName, ctx->NextLogNum);
		ctx->NextLogNum = (ctx->NextLogNum + 1) % ctx->MaxLogNum;
		if (ctx->NextLogNum == 0)
			ctx->NextLogNum = 1;
	}
	else
	{
		time_t now = time(NULL);
		struct tm tmm;
		localtime_s(&tmm, &now);
		sprintf_s(tmp, 128, "%s.%04d%02d%02d%02d%02d%02d", ctx->FileName, tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);
	}
	_unlink(tmp);
	CloseHandle(ctx->File);
	rename(ctx->FileName, tmp);
	return OpenLogFile(ctx->FileName);
}

static int InitNextLogNo(LOG_Context* ctx)
{
	return 1;

	char tmp[128] = {0};
	int i;
	struct stat st;
	time_t minmtime = NOWTIME;
	int minno = 1; 
	for(i = 1; i < ctx->MaxLogNum; ++i) {
		sprintf_s(tmp, 128, "%s.%d", ctx->FileName, i);
		if(stat(tmp, &st) != 0)
			break;
		else {
			if(st.st_mtime <= minmtime) {
				minmtime = st.st_mtime;
				minno = i;
			}
		}
	}
	if(i < ctx->MaxLogNum)
		return i;
	else
		return minno;
}

static int RegisterLogInternal(LOG_Context* ctx)
{
	for (int i = 0; i < MAX_LOG; i++)
	{
		LOG_Context* nctx = &LogArray[CurLogID];
		if (nctx->File != NULL)
		{
			CurLogID++;
			CurLogID = CurLogID % MAX_LOG;
			continue;
		}
		LOGLOCK(&nctx->Lock);
		memcpy(nctx, ctx, sizeof(LOG_Context));
		nctx->File = OpenLogFile(nctx->FileName);
		if (nctx->MaxLogNum < 2)
			nctx->MaxLogNum = DEFAULT_MIN_LOG_NUM;
		nctx->NextLogNum = InitNextLogNo(nctx);

		if (nctx->File >= 0) {
			if (nctx->RotateintVal > 0) {
				nctx->RotateTime = NOWTIME + nctx->RotateintVal;
			}
			else {
				nctx->RotateTime = 0;
			}
			int logfd = CurLogID;
			CurLogID++;
			CurLogID = CurLogID % MAX_LOG;
			return logfd;
		}
		nctx->File = NULL;
		LOGUNLOCK(&nctx->Lock);
		return -1;
	}
	return -1;
}

int RegisterLog(const char* filename, int level, int rotatesize, int rotateintval, int filenum)
{
	LOG_Context ctx;
	memset(&ctx, 0x0, sizeof(LOG_Context));
	strcpy_s(ctx.FileName, 256, filename);
	ctx.LogLevel = level;
	ctx.RotateSize = rotatesize << 20;
	ctx.RotateintVal = rotateintval;
	ctx.MaxLogNum = filenum;
	
	int ret = RegisterLogInternal(&ctx);
	return ret;
}

int CloseLog(int fd)
{
	if (fd < 0)
		return -1;
	LOG_Context* nctx = &LogArray[fd];
	LOGLOCK(&nctx->Lock);
	CloseHandle(nctx->File);
	memset(nctx, 0, sizeof(LOG_Context));
	LOGUNLOCK(&nctx->Lock);
	return 0;
}

int GetLogLevel(const char* loglevelstr) 
{
	if(!_stricmp(loglevelstr, "trace"))
		return LOG_TRACE;
	else if(!_stricmp(loglevelstr, "debug"))
		return LOG_DEBUG;
	else if(!_stricmp(loglevelstr, "normal"))
		return LOG_NORMAL;
	else if(!_stricmp(loglevelstr, "error"))
		return LOG_ERROR;
	else if(!_stricmp(loglevelstr, "fault"))
		return LOG_FAULT;
	else
		return LOG_NONE;	
}

void SetLogLevel(int fd, int level)
{
	LOG_Context* nctx = &LogArray[fd];
	nctx->LogLevel = level;
}

int CheckLogLevel(int fd, int level)
{
	if (fd < 0 || GETLOGLEVEL(fd) > level)
		return -1;
	return 0;
}

void LOG(int fd, int level, const char* fmt, ...) 
{
	if(fd < 0 || GETLOGLEVEL(fd) > level)
		return; 
	
	LOG_Context* nctx = &LogArray[fd];
	/* [time string] [thread pid] [log content] */
	int l;
	char buf[MAX_LOG_LEN];
    struct tm tmm; 
	time_t now = NOWTIME;
	localtime_s(&tmm, &now); 
	char *slog = LogString[level];
	l = snprintf(buf, MAX_LOG_LEN - 1, "[%s:%ld]:[%04d-%02d-%02d %02d:%02d:%02d]", slog, GetCurrentThreadId(), tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);

	va_list ap;
	va_start(ap, fmt);
	l += vsnprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, fmt, ap);
	va_end(ap);
	
	DWORD written;
	WriteFile(nctx->File, buf, l, &written, NULL);
}

char* GetLogStr(int loglevel)
{
	return LogString[loglevel];
}

HANDLE GetLogFileHandle(int fd)
{
	LOG_Context* nctx = &LogArray[fd];
	return nctx->File;
}

static void ScanLogs() 
{
	LOG_Context* ctx;
	HANDLE NewFile = NULL, ofd = NULL;
	int i, shift = 0;
	time_t now = NOWTIME;

	for(i = 0; i < MAX_LOG; ++i) {
		ctx = &LogArray[i];
		if (ctx->File == NULL)
			continue;

		LOGLOCK(&ctx->Lock);
		if(ctx->RotateTime > 0 && ctx->RotateTime < now) 
		{
			NewFile = ShiftFile(ctx);
			if(NewFile != INVALID_HANDLE_VALUE)
			{
				shift = 1;
				ctx->RotateTime = now + ctx->RotateintVal;
			}
		}
		
		DWORD filesize = GetFileSize(ctx->File, NULL);
		if(!shift && (ctx->RotateSize > 0 && filesize > 0 && ctx->RotateSize < filesize))
		{
			NewFile = ShiftFile(ctx);
			if(NewFile != INVALID_HANDLE_VALUE)
				shift = 1;
		}

		if(shift) 
		{
			ctx->File = NewFile;
			shift = 0;
		}
		LOGUNLOCK(&ctx->Lock);
	}
}

void LogLoop()
{
	ScanLogs();
}
