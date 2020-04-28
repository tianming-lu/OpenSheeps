#ifndef _COMMON_H_
#define _COMMON_H_

#if defined COMMON_LIB || defined STRESS_EXPORTS
#define Common_API __declspec(dllexport)
#else
#define Common_API __declspec(dllimport)
#endif // COMMON_LIB

#ifdef __cplusplus
extern "C"
{
#endif

Common_API int GetHostByName(char* name, char* buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif