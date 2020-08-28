#ifndef _SHEEPS_MEMORY_H_
#define _SHEEPS_MEMORY_H_

void* sheeps_malloc(size_t size);
void* sheeps_realloc(void* ptr, size_t size);
void  sheeps_free(void* ptr);

#endif // !_SHEEPS_MEMORY_H_