#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "alloc.h"

// A function to allocate, grow or shrink memory with the C library
static void *library_reallocate(void *memory, size_t size)
{
    return realloc(memory, size);
}

// A function to free memory with the C library
static void library_release(void *memory)
{
    free(memory);
}

// The functions in use: the C library's, unless a unit test has put its own in place
static AllocHooks hooks = { library_reallocate, library_release };

// A function to put a unit test's allocation functions in place, or the C library's back (NULL)
void alloc_set_hooks(const AllocHooks *replacement)
{
    if (replacement != NULL)
        hooks = *replacement;
    else {
        hooks.reallocate = library_reallocate;
        hooks.release = library_release;
    }
}

// A function to allocate memory. A size of 0 asks for 1 byte, so NULL always means out of memory.
void *alloc_malloc(size_t size)
{
    return hooks.reallocate(NULL, size > 0 ? size : 1);
}

// A function to allocate memory for `count` items, filled with zeros
void *alloc_calloc(size_t count, size_t size)
{
    if (size > 0 && count > SIZE_MAX / size)
        return NULL;
    void *memory = alloc_malloc(count * size);
    if (memory != NULL)
        memset(memory, 0, count * size);
    return memory;
}

// A function to grow or shrink memory; on failure the old memory is left as it was
void *alloc_realloc(void *memory, size_t size)
{
    return hooks.reallocate(memory, size > 0 ? size : 1);
}

// A function to copy a string into new memory
char *alloc_strdup(const char *text)
{
    size_t length = strlen(text);
    char *copy = alloc_malloc(length + 1);
    if (copy != NULL)
        memcpy(copy, text, length + 1);
    return copy;
}

// A function to free memory from these functions
void alloc_free(void *memory)
{
    if (memory != NULL)
        hooks.release(memory);
}
