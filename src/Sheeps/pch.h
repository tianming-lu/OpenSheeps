/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#ifndef PCH_H
#define PCH_H

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
#include <windows.h>

extern HMODULE Sheeps_Module;
#else
#define MAX_PATH 260
#endif // __WINDOWS__
#include "cJSON.h"
#include "common.h"
#include "log.h"
#include "mycrypto.h"
#include "sqlite3.h"
#include "Config.h"

extern char EXE_Path[];
extern char ConfigFile[];
extern char ProjectPath[];
extern char RecordPath[];
extern char LogPath[];
#endif //PCH_H
