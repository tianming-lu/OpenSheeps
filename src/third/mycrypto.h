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

#if defined COMMON_LIB || defined STRESS_EXPORTS
#define crypto_API __declspec(dllexport)
#else
#define crypto_API __declspec(dllimport)
#endif // COMMON_LIB
#else
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
crypto_API void     __stdcall   encode_base64(Base64_Context* dst, Base64_Context* src);
crypto_API int      __stdcall   decode_base64(Base64_Context* dst, Base64_Context* src);
crypto_API size_t   __stdcall   base64_encode(char *src, int src_len, char *dst);
crypto_API size_t   __stdcall   base64_decode(char *src, size_t src_len, char *dst);
//base64

//urlencode
crypto_API void __stdcall   urlencode(unsigned char * src, int src_len, unsigned char * dest, int dest_len);
crypto_API unsigned char* __stdcall urldecode(unsigned char* encd, unsigned char* decd);
//urlencode

//sha1
crypto_API void __stdcall GetStringSHA1(char *input, unsigned long length, char *output);
//sha1

//md5
crypto_API int __stdcall getfilemd5view(const char* filename, char* md5, size_t size);
//md5

#ifdef __cplusplus
}
#endif
#endif
