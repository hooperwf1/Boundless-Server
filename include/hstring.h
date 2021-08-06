#ifndef hstring_h
#define hstring_h

void lowerString(char *str);

// Safe version of strncpy, always NULL terminated and faster (no unneeded NULL writing)
size_t strhcpy(char *dst, char *src, size_t size);

// Same as strhcpy, but starts copying at the end of dst
size_t strhcat(char *dst, char *src, size_t size);

// General character location
int findCharacter(char *str, int size, char key);

#endif
