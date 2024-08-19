#define INITGUID
#include "pch.h"

void *mem_alloc(size_t size, const char *file, int32_t line)
{
  (void)file;
  (void)line;
  return malloc(size);
}

void mem_free(void *ptr, const char *file, int32_t line)
{
  (void)file;
  (void)line;
  free(ptr);
}
