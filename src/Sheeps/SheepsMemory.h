/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#ifndef _SHEEPS_MEMORY_H_
#define _SHEEPS_MEMORY_H_

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

inline void* sheeps_malloc(size_t size)
{
	void* ptr = malloc(size);
	if (ptr)
		memset(ptr, 0, size);
	return ptr;
}

inline void* sheeps_realloc(void* ptr, size_t size)
{
	return realloc(ptr, size);
}

inline void sheeps_free(void* ptr)
{
	free(ptr);
}

#endif // !_SHEEPS_MEMORY_H_