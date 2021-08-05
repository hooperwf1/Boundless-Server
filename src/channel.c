#include "channel.h"

struct clus_Cluster *chan_createChannelArray(int size){
	struct clus_Cluster *array = calloc(size, sizeof(struct clus_Cluster));
	if(array == NULL){
        log_logError("Error allocating channels", ERROR);
        return NULL;
	}

	for(int i = 0; i < size; i++){
		if(chan_initChannel(&array[i]) == -1){
			free(array);
			return NULL;
		}
	}

	return array;
}

int chan_initChannel(struct clus_Cluster *c){
	int ret = pthread_mutex_init(&c->mutex, NULL);
	if (ret < 0){
		log_logError("Error initalizing pthread_mutex.", ERROR);
		return -1;
	}

	c->id = -1;
	c->type = TYPE_CHAN;

	// Allocate channel names
	c->name = calloc(fig_Configuration.chanNameLength, sizeof(char));
	if(c->name == NULL){
		log_logError("Error allocating channel name", ERROR);
		return -1;
	}

	return 1;
}

void chan_removeUserFromAllChannels(struct usr_UserData *user, struct clus_Cluster *g){
	if(g == NULL || user == NULL || g->type != TYPE_GROUP)
		return;

	pthread_mutex_lock(&g->mutex);
	for(int i = 0; i < fig_Configuration.maxChannels; i++){
		struct clus_Cluster *chan = &g->channels[i];
		clus_removeUser(chan, user);
	}
	pthread_mutex_unlock(&g->mutex);
}

// Create a channel with the specified name
struct clus_Cluster *chan_createChannel(char *name, struct clus_Cluster *group, struct usr_UserData *user){
    if(name[0] != '#')
		return NULL;

	if(clus_checkClusterName(name) == -1)
		return NULL;

    // Add to the group
	if(group == NULL)
		group = &serverLists.groups[0]; // Default group

    struct clus_Cluster *channel; // Find empty spot
	for(int i = 0; i < fig_Configuration.maxChannels; i++){
		channel = &group->channels[i];

		if(channel->id == -1){
			break;
		}
	
		channel = NULL;
	}

	if(channel == NULL){
		return NULL; // Full
	}

	// All cluster names are case insensitive
    strhcpy(channel->name, name, fig_Configuration.chanNameLength);
	lowerString(channel->name);

	// Setup array of users with default size of 10
	channel->max = 10;
	channel->users = calloc(channel->max, sizeof(struct clus_ClusterUser));
	if(channel->users == NULL){
        log_logError("Error allocating channel users", ERROR);
		free(channel);
        return NULL;
	}

	channel->group = group;

	if(user != NULL){ // Add first user
		clus_addUser(channel, user, 2);
	}

	channel->id = 1; // Load from memory later TODO

	char buff[1024];
	snprintf(buff, ARRAY_SIZE(buff), "Created new channel: %s", name);
	log_logMessage(buff, INFO);

	return channel;
}
