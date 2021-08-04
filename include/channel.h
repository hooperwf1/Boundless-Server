#ifndef channel_h
#define channel_h

#include "user.h"
#include "chat.h"
#include "security.h"
#include "group.h"
#include "hstring.h"

/*	CHANNEL NAME FORMAT:
	&<groupname>/#<channelname>

	if no group name is supplied then
	it is assumed to be:
	&General-Chat/#<channelname>
*/

struct clus_Cluster *chan_createChannelArray(int size);

int chan_initChannel(struct clus_Cluster *c);

void chan_removeUserFromAllChannels(struct usr_UserData *user, struct clus_Cluster *g);

// Create a channel with the specified name, and add it to the specified group
struct clus_Cluster *chan_createChannel(char *name, struct clus_Cluster *group, struct usr_UserData *user);

// Take a channel mode and execute it
// Index is used to help the command parser know which parameter to use
char *chan_executeChanMode(char op, char mode, struct clus_Cluster *channel, char *data, int *index);

// Give or remove chan op or voice
char *chan_giveChanPerms(struct clus_Cluster *channel, struct usr_UserData *user, char op, int perm);

#endif
