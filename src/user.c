#include "user.h"

size_t usr_globalUserID = 0;

struct usr_UserData *usr_createUserArray(int size){
	struct usr_UserData *u = calloc(size, sizeof(struct usr_UserData));
	if(u == NULL){
        log_logError("Error initalizing users list.", ERROR);
        return NULL;
	}
	char buff[200];
	snprintf(buff, ARRAY_SIZE(buff),"Maximum user count: %d.", fig_Configuration.clients);
	log_logMessage(buff, INFO);

	// Set id of all users to -1 and init their mutexes
	for (int i = 0; i < size; i++){
		u[i].id = -1;

		// Initalize mutex to prevent locking issues
		int ret = pthread_mutex_init(&u[i].mutex, NULL);
		if (ret < 0){
			log_logError("Error initalizing pthread_mutex.", ERROR);
			free(u);
			return NULL;
		}
	}

	return u;
}

int usr_getNickname(char *buff, struct usr_UserData *user){
    if(user == NULL || user->id < 0){
        return -1;
    }

    pthread_mutex_lock(&user->mutex);
    strhcpy(buff, user->nickname, fig_Configuration.nickLen);
    pthread_mutex_unlock(&user->mutex);

    return 1;
}

int usr_isValidName(char *buff){
	int len = strlen(buff);
	if(buff[0] == '&' || buff[0] == '#')
		return -1;

	for(int i = 0; i < len; i++){
		switch (buff[i]) {
			case '*':
			case ' ':
			case '\'':
			case '\n':
			case '\0':
				return -1;
		}
	}

	return 1;
}

struct usr_UserData *usr_getUserByName(char *name, struct chat_ServerLists *sLists){
    struct usr_UserData *user;
	if(usr_isValidName(name) == -1)
		return NULL;

	// Can't be UNREGISTED_NAME
	if(strncmp(UNREGISTERED_NAME, name, strlen(UNREGISTERED_NAME)) == 0)
		return NULL;

	// Case insensitive
	lowerString(name);
    for(int i = 0; i < sLists->max; i++){
		user = &sLists->users[i];
		pthread_mutex_lock(&user->mutex);

		if(user->id >= 0 && !strncmp(user->nickname, name, fig_Configuration.nickLen)){
			pthread_mutex_unlock(&user->mutex);
			return user;
		}

		pthread_mutex_unlock(&user->mutex);
    }

    return NULL;
}

//Create a new user and return it
struct usr_UserData *usr_createUser(char *name, struct chat_ServerLists *sLists, struct com_Connection *con){
    struct usr_UserData *user;
	int success = -1;

	// No invalid characters
	if(usr_isValidName(name) == -1)
		return NULL;

	// Find an empty spot
    for(int i = 0; i < sLists->max; i++){
		user = &sLists->users[i];
		pthread_mutex_lock(&user->mutex);

		if(user->id < 0){
			success = 1;
			break;
		}

		pthread_mutex_unlock(&user->mutex);
    }

	if(success == -1) { // Failed to find a spot
		log_logMessage("No spots avaliable for new user", ERROR);
		return NULL;
	}

    //Set user's data
    memset(user, 0, sizeof(struct usr_UserData));

	// Allocate necesary data for the user's nickname
	user->nickname = calloc(fig_Configuration.nickLen, sizeof(char));
	if(user->nickname == NULL){
		log_logError("Error allocating user memory", ERROR);
		return NULL;
	}

	// Initalize joined groups
	user->groups = calloc(fig_Configuration.maxUserGroups, sizeof(struct clus_Cluster *));
	if(user->groups == NULL){
		log_logError("Error initalizing user's group list.", ERROR);
		return NULL;
	}

	usr_changeUserMode(user, '+', 'r');
	user->con = con;

    user->id = 0; // Not invalid, but still not registered

	// Do this last to ensure user isn't selected before it is ready to be used
    strhcpy(user->nickname, name, fig_Configuration.nickLen);
	lowerString(user->nickname);

    return user;
}

// TODO - close connection?
int usr_deleteUser(struct usr_UserData *user){
	if(user == NULL)
		return -1;

    // Nothing new will be sent to queue
    pthread_mutex_lock(&user->mutex);
    user->id = -1; // -1 means invalid user

	// Save the con to close it later
	struct com_Connection *tempCon = user->con;
	user->con = NULL;

    // Groups and channels
	grp_removeUserFromAllGroups(user);

	// Free allocated data
	free(user->nickname);
	free(user->groups);

	user->nickname = NULL;
	user->groups = NULL;

    pthread_mutex_unlock(&user->mutex);

	// Close the connection
	if(tempCon != NULL)
		com_deleteConnection(tempCon);

    return 1;
}

int usr_addGroup(struct usr_UserData *user, struct clus_Cluster *c){
	if(user->id < 0 || user->groupsJoined >= fig_Configuration.maxUserGroups)
		return -1;

	int ret = -1;
	pthread_mutex_lock(&user->mutex);
	for(int i = 0; i < fig_Configuration.maxUserGroups; i++){
		struct clus_Cluster *g = user->groups[i];

		if(g == NULL){
			user->groups[i] = c;
			user->groupsJoined++;
			ret = 1;
			break;
		}
	}
	pthread_mutex_unlock(&user->mutex);
	
	return ret;
}

void usr_removeGroup(struct usr_UserData *user, struct clus_Cluster *c){
	if(user->id < 0 || user->groupsJoined >= fig_Configuration.maxUserGroups)
		return;

	pthread_mutex_lock(&user->mutex);
	for(int i = 0; i < fig_Configuration.maxUserGroups; i++){
		struct clus_Cluster *g = user->groups[i];

		if(g == c){
			user->groups[i] = NULL;
			user->groupsJoined--;
			break;
		}
	}
	pthread_mutex_unlock(&user->mutex);
}

void usr_changeUserMode(struct usr_UserData *user, char op, char mode){
	if(user == NULL || user->id < 0){
		return;
	}

	// No duplicates allowed
	if(op == '+' && usr_userHasMode(user, mode) == 1){
		return;
	}

	// May not set themselves as OP or registered or away
	if(op == 'o' || op == 'r' || op == 'a')
		return;

	pthread_mutex_lock(&user->mutex);
	for(int i = 0; i < ARRAY_SIZE(user->modes); i++){
		if(op == '-' && user->modes[i] == mode){
			user->modes[i] = '\0';
			break;
		} else if(op == '+' && user->modes[i] == '\0'){
			user->modes[i] = mode;
			break;
		}
	}
	pthread_mutex_unlock(&user->mutex);
}

int usr_userHasMode(struct usr_UserData *user, char mode){
	if(user == NULL || user->id < 0){
		return -1;
	}

	int ret = -1;
	pthread_mutex_lock(&user->mutex);
	for(int i = 0; i < ARRAY_SIZE(user->modes); i++){
		if(user->modes[i] == mode){
			ret = 1;
			break;
		}
	}
	pthread_mutex_unlock(&user->mutex);

	return ret;
}

// Generate a quit for a user
void usr_generateQuit(struct usr_UserData *user, char *reason){
	char msg[MAX_MESSAGE_LENGTH];
	snprintf(msg, ARRAY_SIZE(msg), "QUIT %s\n", reason);
	chat_processInput(msg, user->con);
}

int usr_sendContactMessage(struct chat_Message *msg, struct usr_UserData *user){
	struct link_List contacts = {0};
	struct usr_UserData *original = msg->user;

	// Collect contacts
	pthread_mutex_lock(&user->mutex);
	for(int i = 0; i < fig_Configuration.maxUserGroups; i++){
		struct clus_Cluster *g = user->groups[i];
		struct usr_UserData *u;

		if(g == NULL)
			continue;

		pthread_mutex_lock(&g->mutex);
		for(int x = 0; x < g->max; x++){
			u = g->users[x].user;
			if(u == NULL)
				continue;

			if(link_contains(&contacts, u) == -1)
				link_add(&contacts, u);
		}
		pthread_mutex_unlock(&g->mutex);
	}
	pthread_mutex_unlock(&user->mutex);

	// Send to contacts
	while(link_isEmpty(&contacts) == -1){
		struct usr_UserData *u = link_remove(&contacts, 0);
		msg->user = u;
		if(u == original)
			continue;
		chat_sendMessage(msg);
	}

	msg->user = original;

	return 1;
}
