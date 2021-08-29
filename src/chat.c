#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "numerics.h"
#include "communication.h"
#include "logging.h"
#include "chat.h"
#include "linkedlist.h"
#include "commands.h"

struct chat_ServerLists *init_chat(){
	struct chat_ServerLists *sLists = calloc(1, sizeof(struct chat_ServerLists));
	if(sLists == NULL){
		log_logError("Error initalizing serverList", ERROR);
		return NULL;
	}
	sLists->max = fig_Configuration.clients;

	// Allocate users array
	sLists->users = usr_createUserArray(fig_Configuration.clients);

	sLists->groups = grp_createGroupArray(MAX_GROUPS);
	grp_createGroup(fig_Configuration.defaultGroup, NULL, sLists);

    return sLists; 
}

void chat_close(){
}

int chat_serverIsFull(struct chat_ServerLists *serverLists){
	if(serverLists->connected >= serverLists->max)
		return 1;

	return -1;
}

void chat_processInput(char *str, struct com_Connection *con){
	if(con == NULL)
		return;

	struct chat_Message cmd;
	chat_parseStr(str, &cmd); 
	cmd.user = con->user;
	cmd.sLists = con->cList->sLists;

	cmd_runCommand(&cmd);
}

// TODO - Handle null bytes? also handle MAJOR issues with memcpy (size of copied)
int chat_parseStr(char *str, struct chat_Message *cmd){
    memset(cmd, 0, sizeof(struct chat_Message)); // Set all memory to 0

    // Find where the message ends (\r or \n); if not supplied just take the very end of the buffer
    int length = strlen(str);
    int currentPos = 0, loc = 0; // Helps to keep track of where string should be copied
    for (int i = 0; i < (int) strlen(str); i++){
        if(str[i] == '\n' || str[i] == '\r'){
            length = i+1;
            str[i] = ' ';
            break;
        }

		// Remove any non-printable characters
		if(iscntrl(str[i]) == 1){
			str[i] = ' ';
		}
    }

    if(str[0] == ':'){
       loc = chat_findNextSpace(0, length, str);
       if (loc > -1){
           memcpy(cmd->prefix, &str[1], loc - 1); // Copy everything except for the ':'
           currentPos = loc + 1;
       }
    }

    loc = chat_findNextSpace(currentPos, length, str); 
    if (loc >= 0){
        memcpy(cmd->command, &str[currentPos], loc - currentPos);
        currentPos = loc + 1;
    }

    // Fill in up to 15 params
    while (loc > -1 && cmd->paramCount < 15){
       loc = chat_findNextSpace(currentPos, length, str);
       if (loc >= 0){
            if(str[currentPos] == ':'){ // Colon means rest of string is together
                memcpy(cmd->params[cmd->paramCount], &str[currentPos], length - currentPos);
                cmd->paramCount++;
                break;
            }

            memcpy(cmd->params[cmd->paramCount], &str[currentPos], loc - currentPos);
            currentPos = loc +1;
            cmd->paramCount++;
       }
    }

    return 1;
}

int chat_sendMessage(struct chat_Message *msg) {
    if(msg == NULL || msg->user == NULL){
        return -1;
    }

    char str[BUFSIZ];
    chat_messageToString(msg, str, ARRAY_SIZE(str));
    com_sendStr(msg->user->con, str);

    return 1;
}

int chat_sendServerMessage(struct chat_Message *cmd){
	if(cmd == NULL || cmd->user == NULL)
		return -1;

	struct chat_ServerLists *sLists = cmd->user->con->cList->sLists;

    char str[BUFSIZ];
    chat_messageToString(cmd, str, ARRAY_SIZE(str));

	for(int i = 0; i < sLists->max; i++){
		struct usr_UserData *user = &sLists->users[i];

		if(usr_userHasMode(user, 'r') == 1) // Not registered
			continue;

		com_sendStr(user->con, str);
    }

    return 1;
}

int chat_createMessage(struct chat_Message *msg, struct usr_UserData *user, char *prefix, char *cmd, char **params, int paramCount) {
    msg->user = user;

	msg->prefix[0] = '\0';
    if(prefix != NULL){ // Automatically insert a ':' infront
        msg->prefix[0] = ':';
        strhcpy(&msg->prefix[1], prefix, ARRAY_SIZE(msg->prefix));
    }
    strhcpy(msg->command, cmd, ARRAY_SIZE(msg->command));

    for (int i = 0; i < paramCount; i++){
		if(params[i] != NULL){
			strhcpy(msg->params[i], params[i], ARRAY_SIZE(msg->params[i]));
		}
    }

    msg->paramCount = paramCount;

    return 1;
}

int chat_messageToString(struct chat_Message *msg, char *str, int sizeStr) {
	if(msg->prefix[0] != '\0'){
	    snprintf(str, sizeStr, "%s %s", msg->prefix, msg->command);
	} else {
	    snprintf(str, sizeStr, "%s", msg->command);
	}
    
    for (int i = 0; i < msg->paramCount; i++){
        strhcat(str, " ", sizeStr);
        strhcat(str, msg->params[i], sizeStr);
    }

	strhcat(str, "\n", sizeStr);
	str[sizeStr - 2] = '\n';

    return 1;
}

// Locate the next space character 
int chat_findNextSpace(int starting, int size, char *str){
    for(int i = starting; i < size; i++){
        if(str[i] == ' ' || str[i] == '\n' || str[i] == '\r'){
            return i;
        }
    }

    return -1;
}

// Returns the ending location using either \n or \r
int chat_findEndLine(char *str, int size, int starting){
	int found = -1;
	for(int i = starting; i < size; i++){
		if(str[i] == '\0'){
			return -1; // It is done
		} else if(str[i] == '\n' || str[i] == '\r'){
			found = 1; // Keep looping until reach the next 'normal' character
		} else if(found == 1) {
			return i;	
		}
	}

	return -1;
}

// Divide a string into groupname and channelname
int chat_divideChanName(char *str, int size, char data[2][1000]){
	if(str[0] == '#'){ // Only channel
		strhcpy(data[0], fig_Configuration.defaultGroup, ARRAY_SIZE(data[0]));
		strhcpy(data[1], str, ARRAY_SIZE(data[1]));
		return 1;
	}

	int divide = findCharacter(str, size, '/');
	if(divide == -1){ // Only group or neither
		if(str[0] == '&'){
			strhcpy(data[0], str, ARRAY_SIZE(data[1]));
			data[1][0] = '\0';
			return 1;
		}

		return -1;
	}

	// Normal, both
	strhcpy(data[0], str, ARRAY_SIZE(data[0]));
	data[0][divide] = '\0';
	strhcpy(data[1], &str[divide+1], ARRAY_SIZE(data[1]));
	return 1;
}
