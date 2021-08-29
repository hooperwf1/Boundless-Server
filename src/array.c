#include "array.h"

int findArrayItem(const void *arr, int objSize, int arrSize, const void *obj){
	const char *ptr = arr;
	for(int i = 0; i < arrSize * objSize; i += objSize){
		if(memcmp(&ptr[i], obj, objSize) == 0)
			return i/objSize;
	}

	return -1;
}
