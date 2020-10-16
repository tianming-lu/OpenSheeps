#ifndef _SHEEPS_MEMORY_H_
#define _SHEEPS_MEMORY_H_

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

void* sheeps_malloc(size_t size);
void* sheeps_realloc(void* ptr, size_t size);
void  sheeps_free(void* ptr);

#endif // !_SHEEPS_MEMORY_H_