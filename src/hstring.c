#include <string.h>
#include <ctype.h>
#include "hstring.h"

void lowerString(char *str){
	for(int i = 0; i < (int) strlen(str); i++){
		str[i] = tolower(str[i]);
	}
}

