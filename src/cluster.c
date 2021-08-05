#include "cluster.h"

// Returns cluster's name safely
int clus_getClusterName(struct clus_Cluster *cluster, char *buff, int size){
	pthread_mutex_lock(&cluster->mutex);
	strhcpy(buff, cluster->name, size);
	pthread_mutex_unlock(&cluster->mutex);

	return 1;
}

struct clus_Cluster *clus_getCluster(char *name){
	struct clus_Cluster *group;

	// Case-insensitive
	lowerString(name);

	char data[2][1000] = {0};
	int ret = chat_divideChanName(name, strlen(name), data);
	if(ret == -1)
		return NULL;

	if(data[0][0] == '\0'){
		group = &serverLists.groups[0];
	} else {
		group = grp_getGroup(data[0]);
	}

	if(group == NULL)
		return NULL;

	// Is a group
	if(data[1][0] == '\0')
		return group;

	return grp_getChannel(group, data[1]);
}

int clus_checkClusterName(char *name){
	int len = strlen(name);

	for(int i = 0; i < len; i++){
		switch(name[i]){
			case ' ':
			case ',':
			case '/':
			case 7:
				return -1;
		}
	}

	return 1;
}

// Add user to the group or channel
struct clus_ClusterUser *clus_addUser(struct clus_Cluster *cluster, struct usr_UserData *user, int permLevel){
	struct clus_ClusterUser *grpUser = NULL;

	if(user->groupsJoined >= fig_Configuration.maxUserGroups)
		return NULL;

	// Already in group
	grpUser = clus_isInCluster(cluster, user);
	if(grpUser != NULL)
		return grpUser;

	pthread_mutex_lock(&cluster->mutex);
	for(int i = 0; i < cluster->max; i++){
		if(cluster->users[i].user == NULL){
			cluster->users[i].user = user;
			cluster->users[i].permLevel = permLevel;
			grpUser = &cluster->users[i];
			break;
		}
	}
	pthread_mutex_unlock(&cluster->mutex);

	if(cluster->type == TYPE_GROUP && grpUser != NULL)
		usr_addGroup(user, cluster);

	return grpUser;
}

int clus_removeUser(struct clus_Cluster *c, struct usr_UserData *user){
    int ret = -1;

	if(user == NULL || c == NULL || c->id == -1)
		return -1;

	if(c->type == TYPE_GROUP)
		chan_removeUserFromAllChannels(user, c);

	pthread_mutex_lock(&c->mutex);
	for(int i = 0; i < c->max; i++){
		if(c->users[i].user == user){
			// Match
			memset(&c->users[i], 0, sizeof(struct clus_ClusterUser));
			ret = 1;
			break;
		}
	}
	pthread_mutex_unlock(&c->mutex);

	if(c->type == TYPE_GROUP && ret == 1)
		usr_removeGroup(user, c);
    
    return ret;
}


struct clus_ClusterUser *clus_isInCluster(struct clus_Cluster *cluster, struct usr_UserData *user){
	struct clus_ClusterUser *userData = NULL;

	// Go thru each user and check
	pthread_mutex_lock(&cluster->mutex);
	for(int i = 0; i < cluster->max; i++){
		if(cluster->users[i].user == user){
			userData = &cluster->users[i];
			break;
		}
	}
	pthread_mutex_unlock(&cluster->mutex);
	
	return userData;
}

int clus_getUserClusterPrivs(struct usr_UserData *user, struct clus_Cluster *cluster) {
	if(cluster == NULL || user->id < 0){
		return -1;
	}

	int ret = -1;

    pthread_mutex_lock(&cluster->mutex);
	for(int i = 0; i < cluster->max; i++){
		if(cluster->users[i].user == user){
			ret = cluster->users[i].permLevel;
			break;
		}
	}
    pthread_mutex_unlock(&cluster->mutex);

	return ret;
}

int clus_sendClusterMessage(struct chat_Message *cmd, struct clus_Cluster *c){
	struct usr_UserData *origin = cmd->user;

    pthread_mutex_lock(&c->mutex);
	for(int i = 0; i < c->max; i++){
        cmd->user = c->users[i].user;
		
		// Dont send to sender or invalid users
		if(cmd->user == origin || cmd->user == NULL){
			continue;
		}

        chat_sendMessage(cmd);
    }
    pthread_mutex_unlock(&c->mutex);

	cmd->user = origin;

    return 1;
}


// Will fill a string with a list of users
int clus_getUsersInCluster(struct clus_Cluster *c, char *buff, int size){
    char nickname[fig_Configuration.nickLen];
    int pos = 1;

	strhcpy(buff, ":", size);
    pthread_mutex_lock(&c->mutex);
	for(int i = 0; i < c->max; i++){
		if(c->users[i].user != NULL){
			switch(c->users[i].permLevel) {
				case 2: // Channel operator
					strhcat(buff, "@", size);
					pos++;
					break;

				case 1: // Channel voice
					strhcat(buff, "+", size);
					pos++;
					break;
			}

			usr_getNickname(nickname, c->users[i].user);
			strhcat(buff, nickname, size);
			strhcat(buff, " ", size);
		}
    }
    pthread_mutex_unlock(&c->mutex);

    return 1;
}
