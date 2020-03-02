#ifndef _MYCRYPTO_H_
#define _MYCRYPTO_H_
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sha1.h"
#include "md5.h"

#if defined COMMON_LIB || defined STRESS_EXPORTS
#define crypto_API __declspec(dllexport)
#else
#define crypto_API __declspec(dllimport)
#endif // COMMON_LIB

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
crypto_API int decode_base64(Base64_Context* dst, Base64_Context* src);
crypto_API size_t base64_encode(char *src, int src_len, char *dst);
crypto_API size_t base64_decode(char *src, size_t src_len, char *dst);
//base64

//urlencode
crypto_API void urlencode(unsigned char * src, int src_len, unsigned char * dest, int dest_len);
crypto_API unsigned char* urldecode(unsigned char* encd, unsigned char* decd);
//urlencode

//sha1
crypto_API void GetStringSHA1(char *input, unsigned long length, char *output);
//sha1

//md5
crypto_API int getfilemd5view(const char* filename, unsigned char* md5, size_t size);
//md5

#ifdef __cplusplus
}
#endif
#endif
