#include "commands.h"

struct cmd_CommandList cmd_commandList;
struct chat_Message cmd_unknownCommand;
char *thisServer = "example.boundless.chat"; 

// Common reply messages
const char *invalidChanName = ":Invalid channel name";

int init_commands() {
    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&cmd_commandList.commandMutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex.", ERROR);
        return -1;
    }

	if(fig_Configuration.serverName[0] != '\0'){
		thisServer = fig_Configuration.serverName;
	}

    // Init cmd_unknownCommand
    char *params[] = {":Unknown command: "};
    chat_createMessage(&cmd_unknownCommand, NULL, thisServer, ERR_UNKNOWNCOMMAND, params, 1);
    cmd_unknownCommand.user = NULL;

	// Fill in command linked list
	//			  WORD PARAM PERM COMMAND  
    cmd_addCommand("NICK", 1, 0, &cmd_nick);
    cmd_addCommand("PRIVMSG", 2, 1, &cmd_privmsg);
    cmd_addCommand("JOIN", 1, 1, &cmd_join);
    cmd_addCommand("NAMES", 1, 1, &cmd_names);
    cmd_addCommand("PART", 1, 1, &cmd_part);
    cmd_addCommand("KICK", 2, 1, &cmd_kick);
    cmd_addCommand("MODE", 2, 1, &cmd_mode);
    cmd_addCommand("PING", 0, 0, &cmd_ping);
    cmd_addCommand("PONG", 0, 0, &cmd_pong);
    cmd_addCommand("QUIT", 0, 0, &cmd_quit);
    cmd_addCommand("KILL", 2, 2, &cmd_kill);
    cmd_addCommand("OPER", 2, 1, &cmd_oper);
    cmd_addCommand("AUTH", 1, 0, &cmd_auth);

    log_logMessage("Successfully initalized commands.", INFO);
    return 1;
}

int cmd_addCommand(char *word, int minParams, int permLevel, int (*func)(struct chat_Message *, struct chat_Message *)) {
    struct cmd_Command *command = malloc(sizeof(struct cmd_Command));
    if(command == NULL){
        log_logError("Failed to allocate cmd_Command!", DEBUG);
        return -1;
    }

    command->minParams = minParams;
    strhcpy(command->word, word, ARRAY_SIZE(command->word));	
    command->func = func;
	command->permLevel = permLevel;

    pthread_mutex_lock(&cmd_commandList.commandMutex);
    link_add(&cmd_commandList.commands, command);
    pthread_mutex_unlock(&cmd_commandList.commandMutex);

    return 1;
}

int cmd_runCommand(struct chat_Message *cmd){
    struct link_Node *cmdNode;
    struct chat_Message reply;
    struct cmd_Command *command;
    int ret = -2;

    // Loop thru the commands looking for the same command
    pthread_mutex_lock(&cmd_commandList.commandMutex);
    for(cmdNode = cmd_commandList.commands.head; cmdNode != NULL; cmdNode = cmdNode->next){
        if(cmdNode->data == NULL){
            continue;
        }

        command = cmdNode->data;
        if(!strncmp(command->word, cmd->command, ARRAY_SIZE(command->word))){
            // Successful match
            pthread_mutex_unlock(&cmd_commandList.commandMutex);
            ret = -1; // Default to failure

			if(usr_userHasMode(cmd->user, 'r') == 1 && command->permLevel >= 1){
                char *params[] = {":You have not registered: use NICK first"};
                chat_createMessage(&reply, cmd->user, thisServer, ERR_NOTREGISTERED, params, 1);
                break;
			}

			if(usr_userHasMode(cmd->user, 'o') == -1 && command->permLevel >= 2){
                char *params[] = {":Insufficient Permissions"};
                chat_createMessage(&reply, cmd->user, thisServer, ERR_NOPRIVILEGES, params, 1);
                break;
			}

            // Check number of params
            if(cmd->paramCount < command->minParams){
                char *params[] = {":Command needs more params"};
                chat_createMessage(&reply, cmd->user, thisServer, ERR_NEEDMOREPARAMS, params, 1);
                break;
            }

            ret = command->func(cmd, &reply);
            break;
        }
    }
    pthread_mutex_unlock(&cmd_commandList.commandMutex);

    // Unknown command: -1 is reserved for known command error
    if(ret == -2){
        memcpy(&reply, &cmd_unknownCommand, sizeof(reply));
        reply.user = cmd->user;
        strhcat(reply.params[0], cmd->command, ARRAY_SIZE(reply.params[0]));
    }

    if(ret != 2){ // 2 is a request that message is not sent
        chat_sendMessage(&reply);
    }
    return ret;
}

struct clus_Cluster *cmd_checkClusterPerms(struct chat_Message *msg, char *name, struct usr_UserData *user, int reqPrivs) {
    struct clus_Cluster *cluster = clus_getCluster(name, user->con->cList->sLists);
	char *params[5] = {name};

    if(cluster == NULL){
        params[1] = ":Invalid name!";
        chat_createMessage(msg, user, thisServer, ERR_NOSUCHCHANNEL, params, 2);
        return NULL;
    }

	if(clus_isInCluster(cluster, user) == NULL){
        params[1] = ":You are not in this user cluster!";
        chat_createMessage(msg, user, thisServer, ERR_USERNOTINCHANNEL, params, 2);
        return NULL;
	}

	if(clus_getUserClusterPrivs(user, cluster) < reqPrivs) {
        params[1] = ":Insufficient permissions to run this command!";
        chat_createMessage(msg, user, thisServer, ERR_CHANOPRIVSNEEDED, params, 2);
        return NULL;
	}

	return cluster;
}

// Changes a user's nickname
int cmd_nick(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];

    // No nickname given
    if(cmd->params[0][0] == '\0'){
		params[0] = ":Usage: NICK <nickname>";
        chat_createMessage(reply, user, thisServer, ERR_NONICKNAMEGIVEN, params, 1);
        return 1;
    }

    struct usr_UserData *otherUser = usr_getUserByName(cmd->params[0], cmd->sLists);
    if(otherUser == NULL) { // No other user has this name
		char oldName[fig_Configuration.nickLen];
		usr_getNickname(oldName, user);
		int isUnreg = usr_userHasMode(user, 'r');

		// Set the name in the user's buffer, keep ID -1 if not reg
        pthread_mutex_lock(&user->mutex);
        strhcpy(user->nickname, cmd->params[0], fig_Configuration.nickLen);
        pthread_mutex_unlock(&user->mutex);

        params[0] = cmd->params[0];
		// User is already registered - TODO save user data
		if(isUnreg != 1){
			chat_createMessage(reply, user, oldName, "NICK", params, 1);
			usr_sendContactMessage(reply, user);
			return 1;
		}

		// Wait for auth
		return 2;
    }

	params[0] = cmd->params[0];
	params[1] = ":Nickname already in use";

	chat_createMessage(reply, user, thisServer, ERR_NICKNAMEINUSE, params, 2);
	return 1;
}

// Send a message to user or channel
// TODO - add multiple receivers -> <receiver>{,<receiver>}
int cmd_privmsg(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
    struct usr_UserData *otherUser;
    struct clus_Cluster *channel = NULL;
    char *params[ARRAY_SIZE(cmd->params)];
    int size = 1;

    // Sending to another user
    params[0] = cmd->params[0];
    size = 2;
    if(cmd->params[0][0] != '#' && cmd->params[0][0] != '&'){
        otherUser = usr_getUserByName(cmd->params[0], cmd->sLists);
        if(otherUser == NULL){
			params[1] = ":Nick not found!";
            chat_createMessage(reply, user, thisServer, ERR_NOSUCHNICK, params, size);
            return -1;
        }
    } else { // To a channel
		channel = cmd_checkClusterPerms(reply, cmd->params[0], user, 0);
        if(channel == NULL)
			return -1;

		if(channel->type != TYPE_CHAN){
			params[1] = ":Cannot send PRIVMSG to groups!";
            chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 1);
            return -1;
		}

		if(mode_arrayHasMode(channel, 'm') == 1 && clus_getUserClusterPrivs(user, channel) < 1){
			chat_createMessage(reply, user, thisServer, ERR_CANNOTSENDTOCHAN, params, 1);
			return -1;
		}
    }

    // Success
    char nickname[fig_Configuration.nickLen];
    usr_getNickname(nickname, user);

    params[0] = cmd->params[0];
    params[1] = cmd->params[1];

    chat_createMessage(reply, otherUser, nickname, "PRIVMSG", params, size);
    if(channel == NULL){
        return 1;
    }

	reply->user = user;
    clus_sendClusterMessage(reply, channel);
    return 2;
}

// Join a channel and/or a group
// TODO - add error checking
int cmd_join(struct chat_Message *cmd, struct chat_Message *reply){
	struct usr_UserData *user = cmd->user;
	char *params[ARRAY_SIZE(cmd->params)];
	params[0] = cmd->params[0];

	char nick[fig_Configuration.nickLen];
	usr_getNickname(nick, user);

	struct clus_Cluster *cluster = clus_getCluster(cmd->params[0], cmd->sLists);
	if(cluster == NULL) { //try to create it
		// Only a group
		if(findCharacter(cmd->params[0], strlen(cmd->params[0]), '/') == -1 && cmd->params[0][0] == '&'){
			cluster = grp_createGroup(cmd->params[0], user, cmd->sLists);

		} else { // Channel
			// Get name for each part
			char data[2][1000] = {0};
			int ret = chat_divideChanName(cmd->params[0], ARRAY_SIZE(cmd->params[0]), data);
			if(ret == -1 || data[1][0] != '#'){
				params[1] = ":No such channel!";
				chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 2);
				return -1;
			}

			struct clus_Cluster *g = cmd_checkClusterPerms(reply, data[0], user, 0);
			if(g == NULL)
				return -1;

			cluster = chan_createChannel(data[1], g, user);
		}

		// Add a key if specified
		if(cmd->params[1][0] != '\0')
			mode_setKey(cluster, cmd->params[1]);
	}

	// Cluster still unavaliable: FULL or invalid name
	if(cluster == NULL){
		chat_createMessage(reply, user, thisServer, ERR_CHANNELISFULL, params, 1);
		return -1;
	}

	// May only join channel if member of group above
	if(cluster->type == TYPE_CHAN) { // Check if user is in group
		if(clus_isInCluster(cluster->group, user) == NULL){
			params[1] = ":Not on parent group!";
			chat_createMessage(reply, user, thisServer, ERR_NOTONCHANNEL, params, 2);
			return -1;
		}
	}

	// Check key
	if(mode_checkKey(cluster, cmd->params[1]) == -1){
		chat_createMessage(reply, user, thisServer, ERR_BADCHANNELKEY, params, 1);
		return -1;
	}

	/* Join channel or group if not already */
	struct clus_ClusterUser *cUsr = clus_addUser(cluster, user, 0);
	if(cUsr == NULL){
		chat_createMessage(reply, user, thisServer, ERR_CHANNELISFULL, params, 1);
		return -1;
	}

	// Success
	chat_createMessage(reply, NULL, nick, "JOIN", params, 1);
	clus_sendClusterMessage(reply, cluster);

	// Generate a NAMES command
	char names[1024];
	snprintf(names, ARRAY_SIZE(names), "NAMES %s\n", cmd->params[0]);
	chat_processInput(names, user->con);

	return 2;
}

// Returns list of names
// TODO - Hidden/private channels and multiple channels
int cmd_names(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];
	char items[5][1001] = {0};

	char buff[BUFSIZ];
	strhcpy(buff, cmd->params[0], ARRAY_SIZE(buff));
    
	// Split up into array based on commas
	int loc = 0, num = 0;
	while(loc != -1 && num < ARRAY_SIZE(items)){
		int oldLoc = loc;
		loc = findCharacter(buff, strlen(cmd->params[0]), ',');
		if(loc > -1){
			loc++; // Start at char after ','
		}
		strhcpy(items[num], &buff[oldLoc], ARRAY_SIZE(items[0]));

		num++;
	}

	char nick[fig_Configuration.nickLen];
	usr_getNickname(nick, user);
	char names[ARRAY_SIZE(cmd->params[0])];
	for(int i = 0; i < ARRAY_SIZE(items); i++){
		if(items[i][0] == '\0')
			break;
		
		// Default structure
		params[0] = nick;
		params[1] = "=";
		params[2] = items[i];
		params[3] = names;

		// Check to see if channel is the correct one first
		struct clus_Cluster *cluster = clus_getCluster(items[i], cmd->sLists);
		if(cluster == NULL){ // Invalid
			params[0] = items[i];
			chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 1);
		} else { // Valid
			clus_getUsersInCluster(cluster, names, ARRAY_SIZE(names));
			chat_createMessage(reply, user, thisServer, RPL_NAMREPLY, params, 4);
		}
		
		chat_sendMessage(reply);
	}

	params[0] = cmd->params[0];
	params[1] = ":End of /NAMES list";
	chat_createMessage(reply, user, thisServer, RPL_ENDOFNAMES, params, 2);
	return 1;
}

// Leave a channel or group
// TODO - add error checking
int cmd_part(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];
	params[0] = cmd->params[0];

    struct clus_Cluster *cluster = clus_getCluster(cmd->params[0], cmd->sLists);
    if(cluster == NULL){
        chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 1);
        return -1;
    }

    if(clus_removeUser(cluster, user) < 0) { // Not in the channel
		params[1] = ":Not on the selected channel!";
		chat_createMessage(reply, user, thisServer, ERR_USERNOTINCHANNEL, params, 2);
		return 1;
	}

    // Success
    char nickname[fig_Configuration.nickLen];
    usr_getNickname(nickname, user);
    params[0] = cmd->params[0];

    chat_createMessage(reply, user, nickname, "PART", params, 1);
    clus_sendClusterMessage(reply, cluster);

    return 1;
}

// Force a user to leave a cluster
// TODO - add error checking
int cmd_kick(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user, *otherUser;
    char *params[ARRAY_SIZE(cmd->params)];
	int size = 2;
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];

    struct clus_Cluster *cluster = cmd_checkClusterPerms(reply, cmd->params[0], user, 2);
    if(cluster == NULL){
        chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 1);
        return -1;
    }

    otherUser = usr_getUserByName(cmd->params[1], cmd->sLists);
    if(clus_removeUser(cluster, otherUser) < 0) { // Not in the channel
		params[1] = ":Not on the selected channel!";
		chat_createMessage(reply, user, thisServer, ERR_USERNOTINCHANNEL, params, 2);
		return -1;
	}

    // Success
    char nickname[fig_Configuration.nickLen];
    usr_getNickname(nickname, user);
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];
	if(cmd->params[2][0] != '\0'){ // Optional reason
		params[2] = cmd->params[2];
		size = 3;
	}

    chat_createMessage(reply, otherUser, nickname, "KICK", params, size);
    clus_sendClusterMessage(reply, cluster);

    return 1;
}

// Edit modes for channels, groups and users
int cmd_mode(struct chat_Message *cmd, struct chat_Message *reply){
	struct usr_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];

	// Default values
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];

	int type = TYPE_USER;
	// Doesn't matter chan or group, as long as its not TYPE_USER
    if(cmd->params[0][0] == '#' || cmd->params[0][0] == '&')
		type = TYPE_CHAN;

	// Make sure all modes are valid
	char op = cmd->params[1][0];
	int hasOp = op == '-' || op == '+' ? 1 : 0; // If there is an operation dont check it
	if(hasOp == 0){
		op = '+'; // If no op given, default to +
	}

	for(int i = hasOp; i < (int) strlen(cmd->params[1]); i++){
		if(mode_isValidMode(cmd->params[1][i], type) == -1){
			char rpl[20];
			snprintf(rpl, ARRAY_SIZE(rpl), ":No such mode: %c", cmd->params[1][i]);

			params[1] = rpl;
			chat_createMessage(reply, user, thisServer, ERR_UNKNOWNMODE, params, 2);
			return -1;
		}
	}

	//Check whether mode is for channel or user
	switch(type){
		case TYPE_CHAN:
		case TYPE_GROUP:
			return cmd_modeCluster(cmd, reply, op, hasOp);
	}

	return cmd_modeUser(cmd, reply, op, hasOp);
}

// Used by cmd_mode specifically for the user
int cmd_modeUser(struct chat_Message *cmd, struct chat_Message *reply, char op, int hasOp){
    struct usr_UserData *user = cmd->user, *otherUser = NULL;
    char *params[ARRAY_SIZE(cmd->params)];

	// Default values
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];

	otherUser = usr_getUserByName(cmd->params[0], cmd->sLists);
	if(otherUser == NULL){
		params[1] = ":Nick not found!";
		chat_createMessage(reply, user, thisServer, ERR_NOSUCHNICK, params, 2);
		return -1;
	} else if (otherUser != user){
		params[1] = ":You may not MODE a user other than yourself";
		chat_createMessage(reply, user, thisServer, ERR_USERSDONTMATCH, params, 2);
		return -1;
	}

	// Set all the modes
	for(int i = hasOp; i < (int) strlen(cmd->params[1]); i++){
		usr_changeUserMode(user, op, cmd->params[1][i]);		
	}

	chat_createMessage(reply, user, cmd->params[0], "MODE", params, 2);
	return 1;
}

// Used by cmd_mode specifically for a channel or group
int cmd_modeCluster(struct chat_Message *cmd, struct chat_Message *reply, char op, int hasOp){
    struct usr_UserData *user = cmd->user;
	struct clus_Cluster *cluster = NULL;
    char *params[ARRAY_SIZE(cmd->params)];
	char nickname[fig_Configuration.nickLen];
	usr_getNickname(nickname, user);

	// Default values
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];

	cluster = cmd_checkClusterPerms(reply, cmd->params[0], user, 2);
	if(!cluster){
		return -1;
	}

	// Set all the modes
	int index = 0;
	for(int i = hasOp; i < (int) strlen(cmd->params[1]); i++){
		params[2+index] = cmd->params[2+index]; // Fill in extra parameters
		char *ret = clus_executeClusterMode(op, cmd->params[1][i], cluster, cmd->params[2+index], &index);		

		if(ret != NULL){
			params[0] = nickname;
			params[1] = params[2]; // Problematic value
			chat_createMessage(reply, user, thisServer, ret, params, 1);
			return -1;
		}
	}

	chat_createMessage(reply, NULL, nickname, "MODE", params, 2+index);
    clus_sendClusterMessage(reply, cluster);
	return 2;
}


// Send back a PONG
int cmd_ping(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];
	int size = 0;

	if(cmd->paramCount > 0){
		params[0] = cmd->params[0];
		size = 1;
	}

	chat_createMessage(reply, user, NULL, "PONG", params, size);
	return 1;
}

// Response to PONG = do nothing
int cmd_pong(UNUSED(struct chat_Message *cmd), UNUSED(struct chat_Message *reply)){
	return 2;
}

// Used by either the server or the user to signal a user disconnect
int cmd_quit(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
	char *params[] = {cmd->params[0]};

	char nick[fig_Configuration.nickLen];
	usr_getNickname(nick, user);

	chat_createMessage(reply, NULL, nick, "QUIT", params, 1);
	usr_sendContactMessage(reply, user);
	usr_deleteUser(user);

	return 2;
}

// Used to forcefully remove a user
int cmd_kill(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user, *otherUser;
	char *params[] = {cmd->params[0], cmd->params[1]};

	otherUser = usr_getUserByName(cmd->params[0], cmd->sLists);
	if(otherUser == NULL){
		params[1] = ":Nick not found!";
		chat_createMessage(reply, user, thisServer, ERR_NOSUCHNICK, params, 2);
		return -1;
	}

	char nick[fig_Configuration.nickLen];
	usr_getNickname(nick, user);

	// Log it: prevent abuse
	char buff[1024];
	snprintf(buff, ARRAY_SIZE(buff), "%s KILLed %s", nick, cmd->params[0]);
	log_logMessage(buff, MESSAGE);

	chat_createMessage(reply, user, nick, "KILL", params, 2);
	// Everybody sees it to prevent abuse
	chat_sendServerMessage(reply); 
	usr_deleteUser(otherUser);

	return 2;
}

// Used to promote a user to an OPER
int cmd_oper(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
	char *params[] = {cmd->params[0], cmd->params[1]};

	if(auth_checkOper(cmd->params[0], cmd->params[1]) == -1){
		chat_createMessage(reply, user, thisServer, ERR_PASSWDMISMATCH, params, 1);
		return -1;
	}

	// Successfully authenticated
	usr_changeUserMode(user, '+', 'o');		

	char nick[fig_Configuration.nickLen];
	usr_getNickname(nick, user);

	// Log it: prevent abuse
	char buff[1024];
	snprintf(buff, ARRAY_SIZE(buff), "%s OPER %s", nick, cmd->params[0]);
	log_logMessage(buff, MESSAGE);

	params[0] = ":Welcome operator";
	chat_createMessage(reply, user, nick, RPL_YOUREOPER, params, 1);
	return 1;
}

int cmd_auth(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;

	int isReg = usr_userHasMode(user, 'r');
	if(isReg != 1) { // Already registered
		chat_createMessage(reply, user, thisServer, ERR_ALREADYREGISTERED, NULL, 0);
		return -1;
	}

	char nick[fig_Configuration.nickLen];
	usr_getNickname(nick, user);

	int ret = save_loadUser(nick, user, cmd->params[0]);
	if(ret == -1){
		chat_createMessage(reply, user, thisServer, ERR_UNKNOWNERROR, NULL, 0);
		return -1;
	} else if (ret == -2){ // Create the user
		if(save_createUser(user, cmd->params[0]) == -1){
			chat_createMessage(reply, user, thisServer, ERR_UNKNOWNERROR, NULL, 0);
			return -1;
		}

		// Load it after creation
		if(save_loadUser(nick, user, cmd->params[0]) != 1){
			chat_createMessage(reply, user, thisServer, ERR_UNKNOWNERROR, NULL, 0);
			return -1;
		}
	}

	usr_changeUserMode(user, '-', 'r'); // They are now registered
	char *params[] = {nick, fig_Configuration.welcomeMessage};
	chat_createMessage(reply, user, thisServer, RPL_WELCOME, params, 2);
	return 1;
}
