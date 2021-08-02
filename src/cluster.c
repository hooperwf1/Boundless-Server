#include "cluster.h"

// Returns cluster's name safely
int clus_getClusterName(struct clus_Cluster *cluster, char *buff, int size){
	pthread_mutex_lock(&cluster->mutex);
	strncpy(buff, cluster->name, size);
	pthread_mutex_unlock(&cluster->mutex);

	return 1;
}


// Add user to the group or channel
struct clus_ClusterUser *clus_addUser(struct clus_Cluster *cluster, struct usr_UserData *user, int permLevel){
	struct clus_ClusterUser *grpUser = NULL;

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

	return grpUser;
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

	strncpy(buff, ":", size);
    pthread_mutex_lock(&c->mutex);
	for(int i = 0; i < c->max; i++){
		if(c->users[i].user != NULL){
			switch(c->users[i].permLevel) {
				case 2: // Channel operator
					strncat(buff, "@", size - pos - 1);
					pos++;
					break;

				case 1: // Channel voice
					strncat(buff, "+", size - pos - 1);
					pos++;
					break;
			}

			usr_getNickname(nickname, c->users[i].user);
			strncat(buff, nickname, size - pos - 1);
			pos = strlen(buff);

			// Space inbetween users
			buff[pos] = ' ';
			buff[pos + 1] = '\0';
			pos++;
		}
    }
    pthread_mutex_unlock(&c->mutex);

    return 1;
}
