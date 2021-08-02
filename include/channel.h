#ifndef channel_h
#define channel_h

#include "user.h"
#include "chat.h"
#include "security.h"
#include "group.h"

/*	CHANNEL NAME FORMAT:
	&<groupname>/#<channelname>

	if no group name is supplied then
	it is assumed to be:
	&General-Chat/#<channelname>
*/

struct chat_Group;
struct clus_Cluster;

struct clus_Cluster *chan_createChannelArray(int size);

int chan_initChannel(struct clus_Cluster *c);

int chan_removeUserFromChannel(struct clus_Cluster *chan, struct usr_UserData *user);

int chan_removeUserFromAllChannels(struct usr_UserData *user);

// Returns the node to a channel if it exists
struct clus_Cluster *chan_getChannelByName(char *name);

// Create a channel with the specified name, and add it to the specified group
struct clus_Cluster *chan_createChannel(char *name, struct clus_Cluster *group, struct usr_UserData *user);

// Take a channel mode and execute it
// Index is used to help the command parser know which parameter to use
char *chan_executeChanMode(char op, char mode, struct clus_Cluster *channel, char *data, int *index);

// Give or remove chan op or voice
char *chan_giveChanPerms(struct clus_Cluster *channel, struct usr_UserData *user, char op, int perm);

#endif
