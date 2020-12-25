#ifndef _MD5_H
#define _MD5_H

#include <stdint.h>

typedef struct{
	uint32_t stat[4];
	uint32_t count[2];
	uint8_t  buf[64];
}MD5_Context, *LPMD5_Context;

#ifdef __cplusplus
extern "C"
{
#endif

void __stdcall MD5Init(LPMD5_Context ctx);
void __stdcall MD5Update(LPMD5_Context ctx, unsigned char const *buf, unsigned len);
void __stdcall MD5Final(unsigned char digest[16], LPMD5_Context ctx);

void __stdcall MD5Digest( const unsigned char *message, int len, unsigned char *digest);
void __stdcall MD5DigestHex( const unsigned char *message, int len, unsigned char *digest);
void __stdcall MD5HMAC(const unsigned char *password,  unsigned pass_len, const unsigned char *challenge, unsigned chal_len, unsigned char response[16]);
void __stdcall MD5HMAC2(const unsigned char *password,  unsigned pass_len, const unsigned char *challenge, unsigned chal_len, const unsigned char *challenge2, unsigned chal_len2, unsigned char response[16]);
#ifdef __cplusplus
}
#endif

#endif
