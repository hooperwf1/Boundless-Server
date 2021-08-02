#include "group.h"

// TODO - fix error with calloc midway
struct clus_Cluster *grp_createGroupArray(int size){
	struct clus_Cluster *array = calloc(size, sizeof(struct clus_Cluster));

	if(array == NULL){
        log_logError("Error initalizing groups list.", ERROR);
		free(array);
        return NULL;
	}

	// Init each group individually
	for (int i = 0; i < size; i++){
		if(grp_initGroup(&array[i]) == -1){
			free(array);
			return NULL;
		}
	}

	return array;
}

int grp_initGroup(struct clus_Cluster *g){
    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&g->mutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex.", ERROR);
        return -1;
    }

	g->id = -1;
	g->type = TYPE_GROUP;

	g->name = calloc(fig_Configuration.groupNameLength, sizeof(char));
	if(g->name == NULL){
		log_logError("Error initializing group", WARNING);
		return -1;
	}

	return 1;
}

struct clus_Cluster *grp_getGroup(char *name){
	char data[2][1000];
	int ret = chat_divideChanName(name, strlen(name), data);
	if(ret == -1)
		return NULL;

	if(data[0][0] == '\0')
		return &serverLists.groups[0];

	if(data[0][0] != '&')
		return NULL;

	for(int i = 0; i < MAX_GROUPS; i++){
		struct clus_Cluster *g = &serverLists.groups[i];

		pthread_mutex_lock(&g->mutex);
		if(!strncmp(g->name, name, fig_Configuration.groupNameLength)){
			pthread_mutex_unlock(&g->mutex);
			return g;
		}
		pthread_mutex_unlock(&g->mutex);
	}

	return NULL;
}

// Adds a group to the main list, and creates a default channel
struct clus_Cluster *grp_createGroup(char *name, struct usr_UserData *user, int maxUsers){
	if(name[0] != '&'){
		return NULL;
	}

	struct clus_Cluster *group = NULL;

	// Add to main list
	for(int i = 0; i < MAX_GROUPS; i++){
		struct clus_Cluster *g = &serverLists.groups[i];

		pthread_mutex_lock(&g->mutex);
		if(g->id == -1){
			group = g;
			pthread_mutex_unlock(&g->mutex);
			break;
		}
		pthread_mutex_unlock(&g->mutex);
	}

	if(group == NULL){
		log_logMessage("Max groups reached", WARNING);
		return NULL;
	}

	// Allocate channels
	group->ident.channels = chan_createChannelArray(fig_Configuration.maxChannels);

	// Set name
	strncpy(group->name, name, fig_Configuration.groupNameLength-1);

	// Set Maximun users
	group->max = maxUsers;
	group->users = calloc(group->max, sizeof(struct clus_ClusterUser));
	if(group->users == NULL){
        log_logError("Error creating group", ERROR);
		free(group->name);
        return NULL;
	}

	// Make selected user OPER of the group
	clus_addUser(group, user, 2);

	// Generate a default Channel
	chan_createChannel("#default", group, user);

	char buff[1024];
	snprintf(buff, ARRAY_SIZE(buff), "Created new group: %s", name);
	log_logMessage(buff, INFO);

	return group;
}

struct clus_Cluster *grp_getChannel(struct clus_Cluster *group, char *name){
	struct clus_Cluster *channel;

	pthread_mutex_lock(&group->mutex);
	for(int i = 0; i < fig_Configuration.maxChannels; i++){
		channel = &group->ident.channels[i];

		pthread_mutex_lock(&channel->mutex);
		if(!strncmp(channel->name, name, fig_Configuration.chanNameLength)){
			pthread_mutex_unlock(&channel->mutex);
			pthread_mutex_unlock(&group->mutex);
			return channel;
		}
		pthread_mutex_unlock(&channel->mutex);
	}
	pthread_mutex_unlock(&group->mutex);

	return NULL;
}
