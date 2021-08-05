#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "hstring.h"

void lowerString(char *str){
	for(size_t i = 0; i < strlen(str); i++){
		str[i] = tolower(str[i]);
	}
}

size_t strhcpy(char *dst, char *src, size_t size){
	size_t i;
	for(i = 0; i < size-1; i++){
		dst[i] = src[i];

		if(src[i] == '\0')
			break;
	}

	dst[size-1] = '\0';
	return i+1;
}

size_t strhcat(char *dst, char *src, size_t size){
	size_t n = strlen(dst);
	dst += n;
	return strhcpy(dst, src, size-n);
}
