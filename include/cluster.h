#ifndef cluster_h
#define cluster_h

#include "boundless.h"

struct clus_ClusterUser {
	struct usr_UserData *user;
	int permLevel; 
	// Channels 0 - Default, 1 - chanvoice, 2 - chanop, 3 - groupop
	// Groups 0 - Default, 1 - halfop, 2 - groupop
};

struct clus_Cluster { // Represents either a group or channel
	int id;
	int type; // TYPE_CHAN or TYPE_GROUP
	char *name;
	char modes[NUM_MODES];
	char key[KEY_LEN];
	int max;
	struct clus_ClusterUser *users;

	// Identifies the group for channel, and the channels for a group
	union {
		struct clus_Cluster *group;
		struct clus_Cluster *channels;
	} ident;

	pthread_mutex_t mutex;
};

// Returns cluster's name safely
int clus_getClusterName(struct clus_Cluster *cluster, char *buff, int size);

// Returns the cluster based on its name
struct clus_Cluster *clus_getCluster(char *name);

// Add a user to a cluster (channel or group)
struct clus_ClusterUser *clus_addUser(struct clus_Cluster *cluster, struct usr_UserData *user, int permLevel);

int clus_removeUser(struct clus_Cluster *c, struct usr_UserData *user);

struct clus_ClusterUser *clus_isInCluster(struct clus_Cluster *cluster, struct usr_UserData *user);

// Returns the privs the user has for a channel
int clus_getUserClusterPrivs(struct usr_UserData *user, struct clus_Cluster *cluster);

int clus_sendClusterMessage(struct chat_Message *cmd, struct clus_Cluster *c);

// Will fill a string with a list of users
int clus_getUsersInCluster(struct clus_Cluster *c, char *buff, int size);

#endif
