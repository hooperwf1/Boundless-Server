#ifndef cluster_h
#define cluster_h

#include "boundless.h"
#include <stdatomic.h>

struct clus_ClusterUser {
	struct usr_UserData *user;
	atomic_int permLevel; 
	// Channels 0 - Default, 1 - chanvoice, 2 - chanop, 3 - groupop
	// Groups 0 - Default, 1 - halfop, 2 - groupop
};

struct clus_Cluster { // Represents either a group or channel
	atomic_int id;
	atomic_int type; // TYPE_CHAN or TYPE_GROUP
	char *name;
	char modes[NUM_MODES];
	char key[KEY_LEN];
	atomic_int max;
	struct clus_ClusterUser *users;

	// Identifies the group for channel, and the channels for a group
	union {
		struct clus_Cluster *group;
		struct clus_Cluster *channels;
	};

	pthread_mutex_t mutex;
};

// Returns cluster's name safely
int clus_getClusterName(struct clus_Cluster *cluster, char *buff, int size);

// Returns the cluster based on its name
struct clus_Cluster *clus_getCluster(char *name, struct chat_ServerLists *sLists);

// Makes sure a cluster's name is valid (no illegal characters allowed)
int clus_checkClusterName(char *name);

// Add a user to a cluster (channel or group)
struct clus_ClusterUser *clus_addUser(struct clus_Cluster *cluster, struct usr_UserData *user, int permLevel);

int clus_removeUser(struct clus_Cluster *c, struct usr_UserData *user);

struct clus_ClusterUser *clus_isInCluster(struct clus_Cluster *cluster, struct usr_UserData *user);

struct usr_UserData *clus_getUserInCluster(struct clus_Cluster *cluster, char *name);

// Returns the privs the user has for a channel
int clus_getUserClusterPrivs(struct usr_UserData *user, struct clus_Cluster *cluster);

// Give or remove cluster perms
char *clus_giveClusterPerms(struct clus_Cluster *cluster, struct usr_UserData *user, char op, int perm);

// Take a cluster mode and execute it
// Index is used to help the command parser know which parameter to use
char *clus_executeClusterMode(char op, char mode, struct clus_Cluster *cluster, char *data, int *index);

int clus_sendClusterMessage(struct chat_Message *cmd, struct clus_Cluster *c);

// Will fill a string with a list of users
int clus_getUsersInCluster(struct clus_Cluster *c, char *buff, int size);

#endif
