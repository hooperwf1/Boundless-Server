#ifndef chat_h
#define chat_h

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "modes.h"
#include "communication.h"
#include "logging.h"
#include "linkedlist.h"
#include "commands.h"
#include "user.h"
#include "channel.h"
#include "group.h"
#include "cluster.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))
#define MAX_GROUPS 1000

/* This header is the intermediate
   between user/cluster and the
   connections */

struct clus_Cluster;

struct chat_ServerLists {
	int max;
	int connected;
	struct usr_UserData *users;
	struct clus_Cluster *groups;
};

// Contains all parts of a typical message
// The userNode is used to identify the user when receiving
// and used to identify the recipient when sending
struct chat_Message {
    struct usr_UserData *user;
	struct chat_ServerLists *sLists; // Used to identify server items
    char prefix[50];
    char command[50];
    int paramCount;
    char params[10][400];
};

struct chat_ServerLists *init_chat();

void chat_close();

int chat_serverIsFull(struct chat_ServerLists *serverLists);

// Process the queue's contents and then send data back
// to the communication queue for sending back to clients
void chat_processInput(char *str, struct com_Connection *con);

// Parse the input from a user and act on it
int chat_parseStr(char *str, struct chat_Message *cmd);

// Will send a Message struct to specified node
int chat_sendMessage(struct chat_Message *msg);

// Sends a message to every connected and registered user
int chat_sendServerMessage(struct chat_Message *cmd);

// Fills in a Message struct
int chat_createMessage(struct chat_Message *msg, struct usr_UserData *user, char *prefix, char *cmd, char **params, int paramCount);

// Converts a message struct into a string form suitable for sending
int chat_messageToString(struct chat_Message *msg, char *str, int sizeStr);

// Locate the next space character
int chat_findNextSpace(int starting, int size, char *str);

// Returns the location of either \n or \r
int chat_findEndLine(char *str, int size, int starting);

// Divide a string into groupname and channelname
int chat_divideChanName(char *str, int size, char data[2][1000]);

#endif
