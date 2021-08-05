#include "user.h"

size_t usr_globalUserID = 0;

struct usr_UserData *usr_createUserArray(int size){
	struct usr_UserData *u = calloc(size, sizeof(struct usr_UserData));
	if(u == NULL){
        log_logError("Error initalizing users list.", ERROR);
        return NULL;
	}
	char buff[200];
	snprintf(buff, ARRAY_SIZE(buff), "Maximum user count: %d.", serverLists.max);
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

struct usr_UserData *usr_getUserByName(char *name){
    struct usr_UserData *user;

	// Case insensitive
	lowerString(name);
    for(int i = 0; i < serverLists.max; i++){
            user = &serverLists.users[i];
            pthread_mutex_lock(&user->mutex);

            if(user->id >= 0 && !strncmp(user->nickname, name, fig_Configuration.nickLen)){
                    pthread_mutex_unlock(&user->mutex);
                    return user;
            }

            pthread_mutex_unlock(&user->mutex);
    }

    return NULL;
}

struct usr_UserData *usr_getUserBySocket(int sock){
    struct usr_UserData *user;

    for(int i = 0; i < serverLists.max; i++){
            user = &serverLists.users[i];
            pthread_mutex_lock(&user->mutex);

            if(user->socketInfo.socket == sock){
                    pthread_mutex_unlock(&user->mutex);
                    return user;
            }

            pthread_mutex_unlock(&user->mutex);
    }

    return NULL;
}

struct usr_UserData *usr_getUserById(int id){
    struct usr_UserData *user;

    for(int i = 0; i < serverLists.max; i++){
            user = &serverLists.users[i];
            pthread_mutex_lock(&user->mutex);

            if(user->id == id){
                    pthread_mutex_unlock(&user->mutex);
                    return user;
            }

            pthread_mutex_unlock(&user->mutex);
    }

    return NULL;
}

//Create a new user and return it
// TODO fix double nicks if two clients request same name slightly different times
struct usr_UserData *usr_createUser(struct com_SocketInfo *sockInfo, char *name){
    struct usr_UserData *user;
	int success = -1;

	// Find an empty spot
    for(int i = 0; i < serverLists.max; i++){
            user = &serverLists.users[i];
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

	memcpy(&user->socketInfo, sockInfo, sizeof(struct com_SocketInfo));
	usr_changeUserMode(user, '+', 'r');
	user->lastMsg = time(NULL); // Starting time
	user->pinged = 0; // Dont ping on registration, but still kick if idle

    //eventually get this id from saved user data
    user->id = usr_globalUserID++;

	// Do this last to ensure user isn't selected before it is ready to be used
    strhcpy(user->nickname, name, fig_Configuration.nickLen);
	lowerString(user->nickname);

    return user;
}

int usr_deleteUser(struct usr_UserData *user){
	if(user == NULL)
		return -1;

    // Nothing new will be sent to queue
    pthread_mutex_lock(&user->mutex);
    user->id = -1; // -1 means invalid user
	if(user->nickname != NULL)
		free(user->nickname);
	if(user->groups != NULL)
		free(user->groups);

    // Remove socket
	close(user->socketInfo.socket);
    user->socketInfo.socket = -2; // Ensure that no data sent to wrong user
	close(user->socketInfo.socket2);
    user->socketInfo.socket2 = -2; // Ensure that no data sent to wrong user

    // Remove all pending messages
	link_clear(&user->sendQ);

    // Groups and channels
	grp_removeUserFromAllGroups(user);

    pthread_mutex_unlock(&user->mutex);

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
	pthread_mutex_lock(&user->mutex);
}

// Searches for and kicks users that surpassed their message timeouts
int usr_timeOutUsers(int timeOut){
    struct usr_UserData *user;

    for(int i = 0; i < serverLists.max; i++){
            user = &serverLists.users[i];

            pthread_mutex_lock(&user->mutex);
			int id = user->id;
			int diff = (int) difftime(time(NULL), user->lastMsg);
			int pinged = user->pinged;
			pthread_mutex_unlock(&user->mutex);

			if(id != -1 && id != 0){ // Neither invalid nor SERVER
				if(diff > timeOut){
					log_logMessage("User timeout.", INFO);
					usr_deleteUser(user);	
				} else if(pinged == -1 && diff > timeOut/2){ // Ping user
					com_sendStr(user, "PING :Timeout imminent.");

					pthread_mutex_lock(&user->mutex);
					user->pinged = 1;
					pthread_mutex_unlock(&user->mutex);
				}
			}
    }

    return 1;
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
