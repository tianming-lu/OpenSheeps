#ifndef _SHEEPS_MEMORY_H_
#define _SHEEPS_MEMORY_H_

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

inline void* sheeps_malloc(size_t size)
{
#ifdef __WINDOWS__
	return GlobalAlloc(GPTR, size);
#else
	void* ptr = malloc(size);
	if (ptr)
		memset(ptr, 0, size);
	return ptr;
#endif
}

inline void* sheeps_realloc(void* ptr, size_t size)
{
#ifdef __WINDOWS__
	return GlobalReAlloc(ptr, size, GMEM_MOVEABLE);
#else
	return realloc(ptr, size);
#endif
}

inline void sheeps_free(void* ptr)
{
#ifdef __WINDOWS__
	GlobalFree(ptr);
#else
	free(ptr);
#endif
}

#endif // !_SHEEPS_MEMORY_H_