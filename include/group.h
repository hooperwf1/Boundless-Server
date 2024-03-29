#ifndef group_h
#define group_h

#include "boundless.h"
#include "channel.h"
#include "chat.h"
#include "user.h"
#include "hstring.h"

struct chan_Channel;
struct usr_UserData;

// Allocates an array of groups
struct clus_Cluster *grp_createGroupArray(int size);

// Properly initialize a group
int grp_initGroup(struct clus_Cluster *g);

// Will return the group regardless of whether a channel is specified
struct clus_Cluster *grp_getGroup(char *name);

struct clus_Cluster *grp_getChannel(struct clus_Cluster *group, char *name);

// Adds a group to the main list, and creates a default channel
struct clus_Cluster *grp_createGroup(char *name, struct usr_UserData *user, int maxUsers);

void grp_removeUserFromAllGroups(struct usr_UserData *user);

#endif
