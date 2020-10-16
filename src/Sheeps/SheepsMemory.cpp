#include "pch.h"
#include "SheepsMemory.h"

void* sheeps_malloc(size_t size)
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

void* sheeps_realloc(void* ptr, size_t size)
{
#ifdef __WINDOWS__
	return GlobalReAlloc(ptr, size, GMEM_MOVEABLE);
#else
	return realloc(ptr, size);
#endif
}

void sheeps_free(void* ptr)
{
#ifdef __WINDOWS__
	GlobalFree(ptr);
#else
	free(ptr);
#endif
}