#ifndef user_h
#define user_h

#include "boundless.h"
#include "chat.h"
#include <time.h>
#include <stdatomic.h>

#define UNREGISTERED_NAME "unreg"

// Data about an user
// When a user is first loaded from save
// All details will come from the save
// except socketInfo, it must filled with 0 bytes
// except with socketInfo.socket must equal -1
struct usr_UserData {
	atomic_int id;
	char modes[NUM_MODES];
	char *nickname;
	struct com_SocketInfo socketInfo;	
	struct link_List sendQ;
	atomic_int groupsJoined;
	struct clus_Cluster **groups;

	time_t lastMsg; // Keep track of time, too fast = kick, too slow = kick
	atomic_int pinged; // Send only one ping to prevent spam from server

	pthread_mutex_t mutex;
};

// Properly formats an empty user array
struct usr_UserData *usr_createUserArray(int size);

// Fills in buffer with selected user's nickname
int usr_getNickname(char *buff, struct usr_UserData *user);

//Get a user by name
struct usr_UserData *usr_getUserByName(char *name);

//Get a user by id
struct usr_UserData *usr_getUserById(int id);

//Get a user by socket fd
struct usr_UserData *usr_getUserBySocket(int sock);

// Returns a new user, after adding them to the main list
struct usr_UserData *usr_createUser(struct com_SocketInfo *sockInfo, char *name);

// Remove a user from the server
int usr_deleteUser(struct usr_UserData *user);

// Adds the specified group to the user's personal list
int usr_addGroup(struct usr_UserData *user, struct clus_Cluster *c);

// Same as addGroup, but removes it instead
void usr_removeGroup(struct usr_UserData *user, struct clus_Cluster *c);

// Searches for and kicks users that surpassed their message timeouts
int usr_timeOutUsers(int timeOut);

// Adds or removes a mode from a user
void usr_changeUserMode(struct usr_UserData *user, char op, char mode);

// Checks if a user has a mode active
int usr_userHasMode(struct usr_UserData *user, char mode);

/*	A contact is someone the user has contact with (channel, group, etc) but
	only wants to send their message to them once (as opposed to mutliple
	sendClusterMessage), even if they are in multiple
	similar channels/groups, useful for NICK changes, QUIT, AWAY, etc */
int usr_sendContactMessage(struct chat_Message *msg, struct usr_UserData *user);

#endif
