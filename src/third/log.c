#include "log.h"
#include <time.h>
#include <stdint.h>

#define MAX_LOG	256
#define DEFAULT_MIN_LOG_NUM 2

#define NOWTIME time(NULL)		
char* LogString[] = {"TRACE", "DEBUG", "NORMAL", "ERROR", "FAULT", "NONE"};

#ifdef __WINDOWS__
#include "direct.h"
#define LOGLOCK(a)  while (InterlockedExchange(a, 1)){Sleep(0);}
#define LOGUNLOCK(a)	InterlockedExchange(a, 0)
#else
#define LOGLOCK(a)	while (__sync_fetch_and_or(a, 1)){sleep(0);}
#define LOGUNLOCK(a)	__sync_fetch_and_and(a, 0)
#define INVALID_HANDLE_VALUE -1
#endif

typedef struct {
	int		Lock;
#ifdef __WINDOWS__
	HANDLE	filefd;				//log File, internel variable		
#else
	int		filefd;
#endif
	int		LogLevel;		//log level, caller set
	uint32_t		RotateSize;		//rotate size(max log File size), caller set, 0 for disable
	time_t	RotateTime;		//internel variable
	int		RotateintVal;	//rotate interval time, caller set, 0 for disable	
	int		MaxLogNum;		//maximum log files number, caller set		
	int		NextLogNum;		//next log File no, internel variable
	char	FileName[256];		//log FileName, caller set
}LOG_Context, * HLOG_Context;

LOG_Context LogArray[MAX_LOG];
int loginit = 0;
#define GETLOGLEVEL(fd) LogArray[fd].LogLevel
static uint8_t CurLogID = 0;

static void InitLogArray()
{
#ifdef __WINDOWS__
	if (!InterlockedExchange(&loginit, 1))
#else
	if (!__sync_fetch_and_or(&loginit, 1))
#endif // __WINDOWS__
	{
		memset(&LogArray, 0, sizeof(LOG_Context) * MAX_LOG);
	}
}

static int InitNextLogNo(LOG_Context* ctx)
{
	return 1;

	char tmp[128] = { 0 };
	int i;
	struct stat st;
	time_t minmtime = NOWTIME;
	int minno = 1;
	for (i = 1; i < ctx->MaxLogNum; ++i) {
		snprintf(tmp, sizeof(tmp), "%s.%d", ctx->FileName, i);
		if (stat(tmp, &st) != 0)
			break;
		else {
			if (st.st_mtime <= minmtime) {
				minmtime = st.st_mtime;
				minno = i;
			}
		}
	}
	if (i < ctx->MaxLogNum)
		return i;
	else
		return minno;
}

#ifdef __WINDOWS__
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
#else
static int OpenLogFile(char* fn)
{
	//return open(fn, O_CREAT | O_RDWR | O_APPEND | O_LARGEFILE, 0644);
	int filefd = open(fn, O_CREAT | O_RDWR | O_APPEND, 0644);
	if (filefd < 0)
	{
		char* s, * e;
		s = fn;
		while (1)
		{
			e = strchr(s, '/');
			if (e == NULL)
				break;
			*e = 0x0;
			mkdir(fn, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			*e = '/';
			s = e + 1;
		}
		filefd = open(fn, O_CREAT | O_RDWR | O_APPEND, 0644);
	}
	return filefd;
}
#endif // __WINDOWS__

static int RegisterLogInternal(LOG_Context* ctx)
{
	InitLogArray();
	int i;
	for (i = 0; i < MAX_LOG; i++)
	{
		LOG_Context* nctx = &LogArray[CurLogID];
		if (nctx->filefd != 0)
		{
			CurLogID++;
			CurLogID = CurLogID % MAX_LOG;
			continue;
		}
		LOGLOCK(&nctx->Lock);
		memcpy(nctx, ctx, sizeof(LOG_Context));
		nctx->filefd = OpenLogFile(nctx->FileName);
		if (nctx->MaxLogNum < 2)
			nctx->MaxLogNum = DEFAULT_MIN_LOG_NUM;
		nctx->NextLogNum = InitNextLogNo(nctx);

		if (nctx->filefd >= 0) {
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
		nctx->filefd = 0;
		LOGUNLOCK(&nctx->Lock);
		return -1;
	}
	return -1;
}

int RegisterLog(const char* filename, int level, int rotatesize, int rotateintval, int filenum)
{
	LOG_Context ctx;
	memset(&ctx, 0x0, sizeof(LOG_Context));
	snprintf(ctx.FileName, sizeof(ctx.FileName), "%s", filename);
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
#ifdef __WINDOWS__
	CloseHandle(nctx->filefd);
#else
	close(nctx->filefd);
#endif
	memset(nctx, 0, sizeof(LOG_Context));
	LOGUNLOCK(&nctx->Lock);
	return 0;
}

#ifdef __WINDOWS__
static int strncasecmp(const char* input_buffer, char* s2, register int n)
{
	return _strnicmp(input_buffer, s2, n);
}
#endif

int GetLogLevel(const char* loglevelstr) 
{
	if(!strncasecmp(loglevelstr, "trace", 5))
		return LOG_TRACE;
	else if(!strncasecmp(loglevelstr, "debug", 5))
		return LOG_DEBUG;
	else if(!strncasecmp(loglevelstr, "normal", 6))
		return LOG_NORMAL;
	else if(!strncasecmp(loglevelstr, "error", 5))
		return LOG_ERROR;
	else if(!strncasecmp(loglevelstr, "fault", 5))
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
#ifdef __WINDOWS__
	localtime_s(&tmm, &now);
#else
	localtime_r(&now, &tmm);
#endif // __WINDOWS__
	char *slog = LogString[level];
	l = snprintf(buf, MAX_LOG_LEN - 1, "[%s:%ld]:[%04d-%02d-%02d %02d:%02d:%02d]", slog, (long)THREAD_ID, tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);

	va_list ap;
	va_start(ap, fmt);
	l += vsnprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, fmt, ap);
	va_end(ap);
	
#ifdef __WINDOWS__
	DWORD written;
	WriteFile(nctx->filefd, buf, l, &written, NULL);
#else
	write(nctx->filefd, buf, l);
#endif
}

char* GetLogStr(int loglevel)
{
	return LogString[loglevel];
}

#ifdef __WINDOWS__
HANDLE	GetLogFileHandle(int fd)
#else
int GetLogFileHandle(int fd)
#endif
{
	LOG_Context* nctx = &LogArray[fd];
	return nctx->filefd;
}

#ifdef __WINDOWS__
static HANDLE ShiftFile(LOG_Context* ctx)
#else
static int ShiftFile(LOG_Context* ctx)
#endif // __WINDOWS__
{
	char tmp[128] = { 0 };
	if (ctx->MaxLogNum > 2)
	{
		snprintf(tmp, sizeof(tmp), "%s.%d", ctx->FileName, ctx->NextLogNum);
		ctx->NextLogNum = (ctx->NextLogNum + 1) % ctx->MaxLogNum;
		if (ctx->NextLogNum == 0)
			ctx->NextLogNum = 1;
	}
	else
	{
		time_t now = time(NULL);
		struct tm tmm;
#ifdef __WINDOWS__
		localtime_s(&tmm, &now);
#else
		localtime_r(&now, &tmm);
#endif // __WINDOWS__
		snprintf(tmp, sizeof(tmp), "%s.%04d%02d%02d%02d%02d%02d", ctx->FileName, tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);
	}
#ifdef __WINDOWS__
	_unlink(tmp);
	CloseHandle(ctx->filefd);
#else
	unlink(tmp);
	close(ctx->filefd);
#endif // __WINDOWS__
	rename(ctx->FileName, tmp);
	return OpenLogFile(ctx->FileName);
}

static void ScanLogs()
{
	InitLogArray();
	LOG_Context* ctx;
	int i, shift = 0;
	time_t now = NOWTIME;
#ifdef __WINDOWS__
	HANDLE NewFile = NULL;
#else
	int NewFile = 0;
	struct stat st;
#endif // __WINDOWS__

	for (i = 0; i < MAX_LOG; ++i) {
		ctx = &LogArray[i];
#ifdef __WINDOWS__
		if (ctx->filefd == NULL) continue;
#else
		if (ctx->filefd == 0) continue;
#endif // __WINDOWS__

		LOGLOCK(&ctx->Lock);
		if (ctx->RotateTime > 0 && ctx->RotateTime < now)
		{
			NewFile = ShiftFile(ctx);
			if (NewFile != INVALID_HANDLE_VALUE)
			{
				shift = 1;
				ctx->RotateTime = now + ctx->RotateintVal;
			}
		}

#ifdef __WINDOWS__
		DWORD filesize = GetFileSize(ctx->filefd, NULL);
		if (!shift && (ctx->RotateSize > 0 && filesize > 0 && ctx->RotateSize < filesize))
#else
		if (!shift && (ctx->RotateSize > 0 && !fstat(ctx->filefd, &st) && ctx->RotateSize < st.st_size))
#endif // __WINDOWS__
		{
			NewFile = ShiftFile(ctx);
			if (NewFile != INVALID_HANDLE_VALUE)
				shift = 1;
		}

		if (shift)
		{
			ctx->filefd = NewFile;
			shift = 0;
		}
		LOGUNLOCK(&ctx->Lock);
	}
}

void LogLoop()
{
	ScanLogs();
}
