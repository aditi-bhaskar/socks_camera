// Small libc routines the ported 140e FAT32 code needs but that aren't in
// librpi_os.a (which only supplies memcpy/memset). <string.h> already declares
// the str* prototypes; we just provide definitions, plus 140e's memiszero.

#include <stddef.h>
#include <stdint.h>

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++)) { }
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++)) { }
    return dst;
}

// 140e helper: returns 1 if all n bytes are zero, else 0.
int memiszero(const void *p, unsigned n) {
    const uint8_t *b = p;
    for (unsigned i = 0; i < n; i++)
        if (b[i]) return 0;
    return 1;
}
