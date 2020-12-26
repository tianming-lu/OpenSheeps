#ifndef _MD5_H
#define _MD5_H

#include <stdint.h>

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#define __STDCALL __stdcall
#define __CDECL__	__cdecl
#else
#define __STDCALL
#define __CDECL__
#endif

typedef struct{
	uint32_t stat[4];
	uint32_t count[2];
	uint8_t  buf[64];
}MD5_Context, *LPMD5_Context;

#ifdef __cplusplus
extern "C"
{
#endif

void __STDCALL MD5Init(LPMD5_Context ctx);
void __STDCALL MD5Update(LPMD5_Context ctx, unsigned char const *buf, unsigned len);
void __STDCALL MD5Final(unsigned char digest[16], LPMD5_Context ctx);

void __STDCALL MD5Digest( const unsigned char *message, int len, unsigned char *digest);
void __STDCALL MD5DigestHex( const unsigned char *message, int len, unsigned char *digest);
void __STDCALL MD5HMAC(const unsigned char *password,  unsigned pass_len, const unsigned char *challenge, unsigned chal_len, unsigned char response[16]);
void __STDCALL MD5HMAC2(const unsigned char *password,  unsigned pass_len, const unsigned char *challenge, unsigned chal_len, const unsigned char *challenge2, unsigned chal_len2, unsigned char response[16]);
#ifdef __cplusplus
}
#endif

#endif
