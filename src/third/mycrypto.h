/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#ifndef _MYCRYPTO_H_
#define _MYCRYPTO_H_
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#define __STDCALL __stdcall
#define __CDECL__	__cdecl
#if defined COMMON_LIB || defined STRESS_EXPORTS
#define crypto_API __declspec(dllexport)
#else
#define crypto_API __declspec(dllimport)
#endif // COMMON_LIB
#else
#define __STDCALL
#define __CDECL__
#define crypto_API
#endif

//base64
typedef unsigned char u_char;

typedef struct {
    size_t      len;
    u_char     *data;
} Base64_Context;

#ifdef __cplusplus
extern "C"
{
#endif
crypto_API void     __STDCALL   encode_base64(Base64_Context* dst, Base64_Context* src);
crypto_API int      __STDCALL   decode_base64(Base64_Context* dst, Base64_Context* src);
crypto_API size_t   __STDCALL   base64_encode(char *src, int src_len, char *dst);
crypto_API size_t   __STDCALL   base64_decode(char *src, size_t src_len, char *dst);
//base64

//urlencode
crypto_API void __STDCALL   urlencode(unsigned char * src, int src_len, unsigned char * dest, int dest_len);
crypto_API unsigned char* __STDCALL urldecode(unsigned char* encd, unsigned char* decd);
//urlencode

//sha1
crypto_API void __STDCALL GetStringSHA1(char *input, unsigned long length, char *output);
//sha1

//md5
crypto_API int __STDCALL getfilemd5view(const char* filename, char* md5, size_t size);
crypto_API int __STDCALL getstringmd5view(const char* data, size_t data_len, char* md5, size_t size);
//md5

#ifdef __cplusplus
}
#endif
#endif
