/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#ifndef _SHEEPS_MAIN_H_
#define _SHEEPS_MAIN_H_

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#define __STDCALL __stdcall
#define __CDECL__	__cdecl
#ifdef STRESS_EXPORTS
#define Sheeps_API __declspec(dllexport)
#else
#define Sheeps_API __declspec(dllimport)
#endif
#else
#define STDCALL
#define CDECL
#define Sheeps_API
#endif // __WINDOWS__

extern char managerIp[16];
extern int managerPort;

#ifdef __cplusplus
extern "C" {
#endif

	Sheeps_API int __STDCALL SheepsServerRun(u_short listenPort);
	Sheeps_API int __STDCALL SheepsClientRun(int projectid, bool server);
	Sheeps_API int __STDCALL StressClientStop();

#ifdef __cplusplus
}
#endif

#endif // !_SHEEPS_MAIN_H_