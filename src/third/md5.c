#include "md5.h"
#include <string.h>

#define FUNC_A(x, y, z) (z ^ (x & (y ^ z)))
#define FUNC_B(x, y, z) (y ^ (z & (x ^ y)))
#define FUNC_C(x, y, z) (x ^ y ^ z)
#define FUNC_D(x, y, z) (y ^ (x | ~z))

#define MD5STEP(f, a, x, y, z, data, s) ( a += f(x, y, z) + data,  a = a<<s | a>>(32-s),  a += x )

void __STDCALL MD5Init(LPMD5_Context ctx)
{
    ctx->stat[0] = 0x67452301;
    ctx->stat[1] = 0xefcdab89;
    ctx->stat[2] = 0x98badcfe;
    ctx->stat[3] = 0x10325476;

    ctx->count[0] = 0;
    ctx->count[1] = 0;
}

static void MD5Transform(uint32_t stat[4], uint32_t const buf[16])
{
	register uint32_t a, b, c, d;

	a = stat[0];
	b = stat[1];
	c = stat[2];
	d = stat[3];

	MD5STEP(FUNC_A, a, b, c, d, buf[0] + 0xd76aa478, 7);
	MD5STEP(FUNC_A, d, a, b, c, buf[1] + 0xe8c7b756, 12);
	MD5STEP(FUNC_A, c, d, a, b, buf[2] + 0x242070db, 17);
	MD5STEP(FUNC_A, b, c, d, a, buf[3] + 0xc1bdceee, 22);
	MD5STEP(FUNC_A, a, b, c, d, buf[4] + 0xf57c0faf, 7);
	MD5STEP(FUNC_A, d, a, b, c, buf[5] + 0x4787c62a, 12);
	MD5STEP(FUNC_A, c, d, a, b, buf[6] + 0xa8304613, 17);
	MD5STEP(FUNC_A, b, c, d, a, buf[7] + 0xfd469501, 22);
	MD5STEP(FUNC_A, a, b, c, d, buf[8] + 0x698098d8, 7);
	MD5STEP(FUNC_A, d, a, b, c, buf[9] + 0x8b44f7af, 12);
	MD5STEP(FUNC_A, c, d, a, b, buf[10] + 0xffff5bb1, 17);
	MD5STEP(FUNC_A, b, c, d, a, buf[11] + 0x895cd7be, 22);
	MD5STEP(FUNC_A, a, b, c, d, buf[12] + 0x6b901122, 7);
	MD5STEP(FUNC_A, d, a, b, c, buf[13] + 0xfd987193, 12);
	MD5STEP(FUNC_A, c, d, a, b, buf[14] + 0xa679438e, 17);
	MD5STEP(FUNC_A, b, c, d, a, buf[15] + 0x49b40821, 22);

	MD5STEP(FUNC_B, a, b, c, d, buf[1] + 0xf61e2562, 5);
	MD5STEP(FUNC_B, d, a, b, c, buf[6] + 0xc040b340, 9);
	MD5STEP(FUNC_B, c, d, a, b, buf[11] + 0x265e5a51, 14);
	MD5STEP(FUNC_B, b, c, d, a, buf[0] + 0xe9b6c7aa, 20);
	MD5STEP(FUNC_B, a, b, c, d, buf[5] + 0xd62f105d, 5);
	MD5STEP(FUNC_B, d, a, b, c, buf[10] + 0x02441453, 9);
	MD5STEP(FUNC_B, c, d, a, b, buf[15] + 0xd8a1e681, 14);
	MD5STEP(FUNC_B, b, c, d, a, buf[4] + 0xe7d3fbc8, 20);
	MD5STEP(FUNC_B, a, b, c, d, buf[9] + 0x21e1cde6, 5);
	MD5STEP(FUNC_B, d, a, b, c, buf[14] + 0xc33707d6, 9);
	MD5STEP(FUNC_B, c, d, a, b, buf[3] + 0xf4d50d87, 14);
	MD5STEP(FUNC_B, b, c, d, a, buf[8] + 0x455a14ed, 20);
	MD5STEP(FUNC_B, a, b, c, d, buf[13] + 0xa9e3e905, 5);
	MD5STEP(FUNC_B, d, a, b, c, buf[2] + 0xfcefa3f8, 9);
	MD5STEP(FUNC_B, c, d, a, b, buf[7] + 0x676f02d9, 14);
	MD5STEP(FUNC_B, b, c, d, a, buf[12] + 0x8d2a4c8a, 20);

	MD5STEP(FUNC_C, a, b, c, d, buf[5] + 0xfffa3942, 4);
	MD5STEP(FUNC_C, d, a, b, c, buf[8] + 0x8771f681, 11);
	MD5STEP(FUNC_C, c, d, a, b, buf[11] + 0x6d9d6122, 16);
	MD5STEP(FUNC_C, b, c, d, a, buf[14] + 0xfde5380c, 23);
	MD5STEP(FUNC_C, a, b, c, d, buf[1] + 0xa4beea44, 4);
	MD5STEP(FUNC_C, d, a, b, c, buf[4] + 0x4bdecfa9, 11);
	MD5STEP(FUNC_C, c, d, a, b, buf[7] + 0xf6bb4b60, 16);
	MD5STEP(FUNC_C, b, c, d, a, buf[10] + 0xbebfbc70, 23);
	MD5STEP(FUNC_C, a, b, c, d, buf[13] + 0x289b7ec6, 4);
	MD5STEP(FUNC_C, d, a, b, c, buf[0] + 0xeaa127fa, 11);
	MD5STEP(FUNC_C, c, d, a, b, buf[3] + 0xd4ef3085, 16);
	MD5STEP(FUNC_C, b, c, d, a, buf[6] + 0x04881d05, 23);
	MD5STEP(FUNC_C, a, b, c, d, buf[9] + 0xd9d4d039, 4);
	MD5STEP(FUNC_C, d, a, b, c, buf[12] + 0xe6db99e5, 11);
	MD5STEP(FUNC_C, c, d, a, b, buf[15] + 0x1fa27cf8, 16);
	MD5STEP(FUNC_C, b, c, d, a, buf[2] + 0xc4ac5665, 23);

	MD5STEP(FUNC_D, a, b, c, d, buf[0] + 0xf4292244, 6);
	MD5STEP(FUNC_D, d, a, b, c, buf[7] + 0x432aff97, 10);
	MD5STEP(FUNC_D, c, d, a, b, buf[14] + 0xab9423a7, 15);
	MD5STEP(FUNC_D, b, c, d, a, buf[5] + 0xfc93a039, 21);
	MD5STEP(FUNC_D, a, b, c, d, buf[12] + 0x655b59c3, 6);
	MD5STEP(FUNC_D, d, a, b, c, buf[3] + 0x8f0ccc92, 10);
	MD5STEP(FUNC_D, c, d, a, b, buf[10] + 0xffeff47d, 15);
	MD5STEP(FUNC_D, b, c, d, a, buf[1] + 0x85845dd1, 21);
	MD5STEP(FUNC_D, a, b, c, d, buf[8] + 0x6fa87e4f, 6);
	MD5STEP(FUNC_D, d, a, b, c, buf[15] + 0xfe2ce6e0, 10);
	MD5STEP(FUNC_D, c, d, a, b, buf[6] + 0xa3014314, 15);
	MD5STEP(FUNC_D, b, c, d, a, buf[13] + 0x4e0811a1, 21);
	MD5STEP(FUNC_D, a, b, c, d, buf[4] + 0xf7537e82, 6);
	MD5STEP(FUNC_D, d, a, b, c, buf[11] + 0xbd3af235, 10);
	MD5STEP(FUNC_D, c, d, a, b, buf[2] + 0x2ad7d2bb, 15);
	MD5STEP(FUNC_D, b, c, d, a, buf[9] + 0xeb86d391, 21);

	stat[0] += a;
	stat[1] += b;
	stat[2] += c;
	stat[3] += d;
}

void __STDCALL MD5Update(LPMD5_Context ctx, unsigned char const *buf, unsigned len)
{
    register uint32_t t;

    t = ctx->count[0];
    if ((ctx->count[0] = t + ((uint32_t) len << 3)) < t)
	ctx->count[1]++;
    ctx->count[1] += len >> 29;

    t = (t >> 3) & 0x3f;

    if (t) {
	unsigned char *p = (unsigned char *) ctx->buf + t;

	t = 64 - t;
	if (len < t) {
	    memcpy(p, buf, len);
	    return;
	}
	memcpy(p, buf, t);
	MD5Transform(ctx->stat, (uint32_t *) ctx->buf);
	buf += t;
	len -= t;
    }

    while (len >= 64) {
	memcpy(ctx->buf, buf, 64);
	MD5Transform(ctx->stat, (uint32_t *) ctx->buf);
	buf += 64;
	len -= 64;
    }
    memcpy(ctx->buf, buf, len);
}

void __STDCALL MD5Final(unsigned char digest[16], LPMD5_Context ctx)
{
    unsigned int count;
    unsigned char *p;

    count = (ctx->count[0] >> 3) & 0x3F;
    p = ctx->buf + count;
    *p++ = 0x80;

    count = 64 - 1 - count;

    if (count < 8) {
	memset(p, 0, count);
	MD5Transform(ctx->stat, (uint32_t *) ctx->buf);
	memset(ctx->buf, 0, 56);
    } else {
	memset(p, 0, (size_t)count - 8);
    }

    ((uint32_t *) ctx->buf)[14] = ctx->count[0];
    ((uint32_t *) ctx->buf)[15] = ctx->count[1];

    MD5Transform(ctx->stat, (uint32_t *) ctx->buf);
    memcpy(digest, ctx->stat, 16);
    memset(ctx, 0, sizeof(MD5_Context));
}

void __STDCALL MD5Digest( const unsigned char *message, int len, unsigned char *digest) {
	MD5_Context ctx;
	MD5Init (&ctx);
	MD5Update (&ctx, message, len);
	MD5Final (digest, &ctx);
}

void __STDCALL MD5DigestHex( const unsigned char *message, int len, unsigned char *digest) {
	MD5_Context ctx;
	unsigned char h[16];
	char xdigit[] = "0123456789abcdef";
	int i;

	MD5Init (&ctx);
	MD5Update (&ctx, message, len);
	MD5Final (h, &ctx);
	for(i=0; i<16; i++) {
		*digest++ = xdigit[h[i]>>4&0xf];
		*digest++ = xdigit[h[i]&0xf];
	}
	*digest = '\0';
}

void __STDCALL MD5HMAC (const unsigned char *password,  unsigned pass_len,
		const unsigned char *challenge, unsigned chal_len,
		unsigned char response[16])
{
	int i;
	unsigned char ipad[64];
	unsigned char opad[64];
	unsigned char hash_passwd[16];

	MD5_Context ctx;

	if (pass_len > sizeof (ipad))
	{
		MD5Init (&ctx);
		MD5Update (&ctx, password, pass_len);
		MD5Final (hash_passwd, &ctx);
		password = hash_passwd; pass_len = sizeof (hash_passwd);
	}

	memset (ipad, 0, sizeof (ipad));
	memset (opad, 0, sizeof (opad));
	memcpy (ipad, password, pass_len);
	memcpy (opad, password, pass_len);

	for (i=0; i<64; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}

	MD5Init (&ctx);
	MD5Update (&ctx, ipad, sizeof (ipad));
	MD5Update (&ctx, challenge, chal_len);
	MD5Final (response, &ctx);

	MD5Init (&ctx);
	MD5Update (&ctx, opad, sizeof (opad));
	MD5Update (&ctx, response, 16);
	MD5Final (response, &ctx);
}

void __STDCALL MD5HMAC2 (const unsigned char *password,  unsigned pass_len,
		const unsigned char *challenge, unsigned chal_len,
		const unsigned char *challenge2, unsigned chal_len2,
		unsigned char response[16])
{
	int i;
	unsigned char ipad[64];
	unsigned char opad[64];
	unsigned char hash_passwd[16];

	MD5_Context ctx;

	if (pass_len > sizeof (ipad))
	{
		MD5Init (&ctx);
		MD5Update (&ctx, password, pass_len);
		MD5Final (hash_passwd, &ctx);
		password = hash_passwd; pass_len = sizeof (hash_passwd);
	}

	memset (ipad, 0, sizeof (ipad));
	memset (opad, 0, sizeof (opad));
	memcpy (ipad, password, pass_len);
	memcpy (opad, password, pass_len);

	for (i=0; i<64; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}

	MD5Init (&ctx);
	MD5Update (&ctx, ipad, sizeof (ipad));
	MD5Update (&ctx, challenge, chal_len);
	MD5Update (&ctx, challenge2, chal_len2);
	MD5Final (response, &ctx);

	MD5Init (&ctx);
	MD5Update (&ctx, opad, sizeof (opad));
	MD5Update (&ctx, response, 16);
	MD5Final (response, &ctx);
}
