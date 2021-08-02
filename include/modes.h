#ifndef modes_h
#define modes_h

#include "numerics.h"
#include "chat.h"

#define TYPE_USER 0
#define TYPE_CHAN 1
#define TYPE_GROUP 2

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))

struct clus_Cluster;

int mode_arrayHasMode(struct clus_Cluster *c, char mode);

void mode_editArray(struct clus_Cluster *c, char op, char mode);

int mode_isValidMode(char mode, int type);

// Check, set and remove key
char *mode_setKey(struct clus_Cluster *c, char *key);
char *mode_removeKey(struct clus_Cluster *c, char *key);
int mode_checkKey(struct clus_Cluster *c, char *key);

#endif
