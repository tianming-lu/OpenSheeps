#include "mycrypto.h"
#include <stdio.h>
#include <stdlib.h>
#include "sha1.h"
#include "md5.h"

//base64
static void encode_base64_internal(Base64_Context *dst, Base64_Context *src, const u_char *basis, uint8_t padding)
{
    u_char         *d, *s;
    size_t          len;

    len = src->len;
    s = src->data;
    d = dst->data;

    while (len > 2) {
        *d++ = basis[(s[0] >> 2) & 0x3f];
        *d++ = basis[((s[0] & 3) << 4) | (s[1] >> 4)];
        *d++ = basis[((s[1] & 0x0f) << 2) | (s[2] >> 6)];
        *d++ = basis[s[2] & 0x3f];

        s += 3;
        len -= 3;
    }

    if (len) {
        *d++ = basis[(s[0] >> 2) & 0x3f];

        if (len == 1) {
            *d++ = basis[(s[0] & 3) << 4];
            if (padding) {
                *d++ = '=';
            }

        } else {
            *d++ = basis[((s[0] & 3) << 4) | (s[1] >> 4)];
            *d++ = basis[(s[1] & 0x0f) << 2];
        }

        if (padding) {
            *d++ = '=';
        }
    }
	
	dst->len = d - dst->data;
    *(dst->data + dst->len) = 0x0;
}

static int decode_base64_internal(Base64_Context *dst, Base64_Context *src, const u_char *basis)
{
    size_t          len;
	int				acount = 0;
    u_char         *d, *s;

    for (len = 0; len < src->len; len++) {
        if (src->data[len] == '=') {
			acount++;
            //break;
        }

        /*if (basis[src->data[len]] == 77) {
            return -1;
        }*/
    }

    if (len % 4 == 1) {
        return -1;
    }

    s = src->data;
    d = dst->data;

    while (len > 3) {
        *d++ = (u_char) (basis[s[0]] << 2 | basis[s[1]] >> 4);
        *d++ = (u_char) (basis[s[1]] << 4 | basis[s[2]] >> 2);
        *d++ = (u_char) (basis[s[2]] << 6 | basis[s[3]]);

        s += 4;
        len -= 4;
    }

    if (len > 1) {
        *d++ = (u_char) (basis[s[0]] << 2 | basis[s[1]] >> 4);
    }
	
	if (len > 1) {
        *d++ = (u_char) (basis[s[0]] << 2 | basis[s[1]] >> 4);
    }

    if (len > 2) {
        *d++ = (u_char) (basis[s[1]] << 4 | basis[s[2]] >> 2);
    }

    dst->len = d - dst->data - acount;
    return 0;
}

void encode_base64(Base64_Context *dst, Base64_Context *src)
{
    static u_char   basis64[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    encode_base64_internal(dst, src, basis64, 1);
}

int decode_base64(Base64_Context *dst, Base64_Context *src)
{
    static u_char   basis64[] = {
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 62, 77, 77, 77, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 77, 77, 77, 77, 77, 77,
        77,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 77, 77, 77, 77, 77,
        77, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 77, 77, 77, 77, 77,

        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
        77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77, 77
    };

    return decode_base64_internal(dst, src, basis64);
}

size_t base64_encode(char *src, int src_len, char *dst)
{
	if (src == NULL)
		return -1;

	Base64_Context str;
    str.data = (u_char*)src;
    str.len = src_len;

    Base64_Context temp;
    temp.len = str.len*8/6+4;
    temp.data = malloc(temp.len);
	if (temp.data == NULL)
		return 0;
    encode_base64(&temp, &str);
    memcpy(dst, temp.data, temp.len);
	*(dst+temp.len) = 0x0;

    free(temp.data);
    return temp.len;
}

size_t base64_decode(char *src, size_t src_len, char *dst)
{
	if(src == NULL)
		return -1;

	Base64_Context str;
    str.data = (u_char*)src;
    str.len = src_len;

	Base64_Context temp;
    temp.len = (str.len)*6/8;
    temp.data = malloc(temp.len);
	if (temp.data == NULL)
	{
		return 0;
	}
    decode_base64(&temp, &str);
    memcpy(dst, temp.data, temp.len);
	*(dst+temp.len) = 0x0;

    free(temp.data);
    return temp.len;
}
//base64


//urlencode
static unsigned char char_to_hex(unsigned char x) {
    return (unsigned char) (x > 9 ? x + 55 : x + 48);
}

static int is_alpha_number_char(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9'))
        return 1;
    return 0;
}

void urlencode(unsigned char * src, int src_len, unsigned char * dest,
        int dest_len) {
    unsigned char ch;
    int len = 0;

    while (len < (dest_len - 4) && *src) {
        ch = (unsigned char) *src;
        if (*src == ' ') {
            *dest++ = '+';
        } else if (is_alpha_number_char(ch) || strchr("-_.!~*'()", ch)) {
            *dest++ = *src;
        } else {
            *dest++ = '%';
            *dest++ = char_to_hex((unsigned char) (ch >> 4));
            *dest++ = char_to_hex((unsigned char) (ch % 16));
        }
        ++src;
        ++len;
    }
    *dest = 0;
    return;
}

unsigned char* urldecode(unsigned char* encd, unsigned char* decd) {
    int j, i;
    unsigned char *cd = encd;
    char p[2];
    size_t num = strlen((char*)cd);
    j = 0;

    for (i = 0; i < num; i++) {
        memset(p, 0, 2);

        if (cd[i] == '+') {
            decd[j++] = ' ';
            continue;
        }

        if (cd[i] != '%') {
            decd[j++] = cd[i];
            continue;
        }

        p[0] = cd[++i];
        p[1] = cd[++i];

        p[0] = p[0] - 48 - ((p[0] >= 'A') ? 7 : 0) - ((p[0] >= 'a') ? 32 : 0);
        p[1] = p[1] - 48 - ((p[1] >= 'A') ? 7 : 0) - ((p[1] >= 'a') ? 32 : 0);
        decd[j++] = (unsigned char) (p[0] * 16 + p[1]);

    }
    decd[j] = '\0';

    return decd;
}
//urlencode

//sha1
void GetStringSHA1(char *input, unsigned long length, char *output)
{
    if (NULL == input || NULL == output)
        return;
    
    SHA1Context sha1;           
    
    SHA1Reset(&sha1);
    SHA1Input(&sha1, (const unsigned char *)input, length);
    SHA1Result(&sha1, (uint8_t *)output);
}
//sha1

//md5
int getfilemd5(const char* filename, unsigned char* md5) 
{
	FILE* file = NULL;
	fopen_s(&file, filename, "rb");
	if (!file)
		return -1;
	MD5_Context ctx;
	MD5Init(&ctx);

#define BUFSIZE  10240
	//const int BUFSIZE = 1 << 20;
	char buf[BUFSIZE];
	size_t len;
	while (1) {
		len = fread(buf, 1, BUFSIZE, file);
		if (len > 0) {
			MD5Update(&ctx, (const unsigned char*)buf, (int)len);
			if (len < BUFSIZE)
				break;
		}
		else if (feof(file))
			break;
		else {
			fclose(file);
			return -1;
		}

	}
	fclose(file);
	MD5Final(md5, &ctx);
	return 0;
}

int getfilemd5view(const char* filename, unsigned char* md5, size_t size)
{
	unsigned char omd5[16] = { 0x0 };
	unsigned char smd5[36] = { 0x0 };

	if (getfilemd5(filename, omd5))
		return -1;

	char* s = (char*)smd5;
	int l = 0;
	int i = 0;
	for (i = 0; i < 16; i++)
	{
		l += snprintf(s + l, sizeof(smd5) - l, "%02x", omd5[i]);
	}
	strcpy_s((char*)md5,  size, (char*)smd5);
	return 0;
}
//md5