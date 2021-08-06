#ifndef cmds_h
#define cmds_h

#include "chat.h"
#include "modes.h"
#include "linkedlist.h"
#include "numerics.h"
#include "user.h"
#include "channel.h"
#include "auth.h"

#define NUMERIC_SIZE 15
#define UNUSED(x) x __attribute__((unused))

struct chat_Message;
struct chan_Channel;

struct cmd_CommandList {
    struct link_List commands;	
    pthread_mutex_t commandMutex;
};

// permLevel describes the permissions needed for a user to run a command
// 0 = Unregistered
// 1 = Registered
// 2 = Server Admin
struct cmd_Command {
    char word[50];
    int minParams;
    int (*func)(struct chat_Message *, struct chat_Message *);
	int permLevel;
};

int init_commands();

// Will use parameters to construct a cmd_Command struct
// and adds it to the cmd_commandList struct
int cmd_addCommand(char *word, int minParams, int permLevel, int (*func)(struct chat_Message *, struct chat_Message *));

int cmd_runCommand(struct chat_Message *cmd);

// Will check if a user is able to execute a command in a channel or group
struct clus_Cluster *cmd_checkClusterPerms(struct chat_Message *msg, char *name, struct usr_UserData *user, int reqPrivs);

// Change a user's nickname
int cmd_nick(struct chat_Message *cmd, struct chat_Message *reply);

// Send to a message to user or channel
int cmd_privmsg(struct chat_Message *cmd, struct chat_Message *reply);

// Join a channel or group
int cmd_join(struct chat_Message *cmd, struct chat_Message *reply);

// Return a list of names inside a channel
int cmd_names(struct chat_Message *cmd, struct chat_Message *reply);

// Remove a user from a channel
int cmd_part(struct chat_Message *cmd, struct chat_Message *reply);

// Forcefully remove a user from a channel
int cmd_kick(struct chat_Message *cmd, struct chat_Message *reply);

// Change modes for a user or channel
int cmd_mode(struct chat_Message *cmd, struct chat_Message *reply);
int cmd_modeUser(struct chat_Message *cmd, struct chat_Message *reply, char op, int hasOp);
int cmd_modeCluster(struct chat_Message *cmd, struct chat_Message *reply, char op, int hasOp);

// Send back a PONG
int cmd_ping(struct chat_Message *cmd, struct chat_Message *reply);

// Response to PONG = do nothing
int cmd_pong(struct chat_Message *cmd, struct chat_Message *reply);

// Used by either the server or the user to signal a user disconnect
int cmd_quit(struct chat_Message *cmd, struct chat_Message *reply);

// Used to forcefully remove a user
int cmd_kill(struct chat_Message *cmd, struct chat_Message *reply);

// Used to promote a user to an OPER
int cmd_oper(struct chat_Message *cmd, struct chat_Message *reply);

#endif
