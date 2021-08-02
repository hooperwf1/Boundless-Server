#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "numerics.h"
#include "communication.h"
#include "logging.h"
#include "chat.h"
#include "linkedlist.h"
#include "commands.h"

struct chat_ServerLists serverLists = {0};
struct chat_DataQueue dataQueue = {0};

int init_chat(){
	char buff[100];

	// Compensate for null byte '\0'
	fig_Configuration.nickLen++; 
	fig_Configuration.groupNameLength++;
	fig_Configuration.chanNameLength++;
	serverLists.max = fig_Configuration.clients;

    // Allocate threads for processing user input 
    dataQueue.threads = calloc(fig_Configuration.threadsDATA, sizeof(pthread_t)); 
    if (dataQueue.threads == NULL){
        log_logError("Error initalizing dataQueue.threads.", ERROR);
        return -1;
    }
    
    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&dataQueue.queueMutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex.", ERROR);
        return -1;
    }

	// Allocate users array
	serverLists.users = calloc(fig_Configuration.clients, sizeof(struct usr_UserData));
	if(serverLists.users == NULL){
        log_logError("Error initalizing users list.", ERROR);
        return -1;
	}
	snprintf(buff, ARRAY_SIZE(buff), "Maximum user count: %d.", serverLists.max);
	log_logMessage(buff, INFO);

	// Set id of all users to -1 and init their mutexes
	for (int i = 0; i < serverLists.max; i++){
		serverLists.users[i].id = -1;

		// Initalize mutex to prevent locking issues
		ret = pthread_mutex_init(&serverLists.users[i].userMutex, NULL);
		if (ret < 0){
			log_logError("Error initalizing pthread_mutex.", ERROR);
			return -1;
		}
	}

	serverLists.groups = grp_createGroupArray(MAX_GROUPS);
	grp_createGroup("&General-Chat", &serverLists.users[0], serverLists.max);

    return chat_setupDataThreads(&fig_Configuration); 
}

void chat_close(){
    free(dataQueue.threads);
	free(serverLists.users);
}

int chat_serverIsFull(){
	if(serverLists.connected >= serverLists.max)
		return 1;

	return -1;
}

int chat_setupDataThreads(struct fig_ConfigData *config){
    char buff[BUFSIZ];
    int numThreads = config->threadsDATA;
    int ret = 0;

    for (int i = 0; i < numThreads; i++){
        ret = pthread_create(&dataQueue.threads[i], NULL, chat_processQueue, &dataQueue);
        if (ret < 0){
            log_logError("Error initalizing thread.", ERROR);
            return -1;
        }
    }

    snprintf(buff, ARRAY_SIZE(buff), "Successfully processing data on %d threads.", numThreads);
    log_logMessage(buff, INFO);

    return numThreads;
}

int chat_insertQueue(struct usr_UserData *user, int type, char *str, struct chat_Message *msg){
	struct com_QueueJob *job;
	job = calloc(1, sizeof(struct com_QueueJob));
	if(job == NULL){
		log_logError("Error allocating job", ERROR);
		if(msg != NULL)
			free(msg);
		return -1;
	}

	job->type = type;
	job->user = user;
	job->msg = msg;
	if(str != NULL)
		strncpy(job->str, str, ARRAY_SIZE(job->str)-1);

    pthread_mutex_lock(&dataQueue.queueMutex); 
    link_add(&dataQueue.queue, job);
    pthread_mutex_unlock(&dataQueue.queueMutex); 

    return 1;
}

void *chat_processQueue(void *param){
    struct chat_DataQueue *dataQ = param;
    struct timespec delay = {.tv_nsec = 1000000}; // 1ms

    while(1) { 
        // Make sure to set as null to prevent undefined behavior
        struct com_QueueJob *job = NULL;

        // grab from first item in linked list expecting link_Node of user
        pthread_mutex_lock(&dataQ->queueMutex);
        if(link_isEmpty(&dataQ->queue) < 0){
            job = link_remove(&dataQ->queue, 0);
        }
        pthread_mutex_unlock(&dataQ->queueMutex);

        // Nothing to process
        if(job == NULL){
            // Allow other threads time to access mutex
            nanosleep(&delay, NULL); 
            continue;
        }

        if(!job->user){
            log_logMessage("Job user is NULL", DEBUG);
            free(job);
            continue;
        }

        if(job->user->id >= 0){ // Make sure user is valid
            switch (job->type) {
                case 0: // Text to cmd
                    chat_parseInput(job); 
                    break;

                case 1: // Run the cmd
                    cmd_runCommand(job->msg);
                    free(job->msg);
                    break;                
            }
        }

        free(job);
    }

    return NULL;
}

// TODO - Support multiple cmds in one read
// TODO - Handle null bytes? also handle MAJOR issues with memcpy (size of copied)
int chat_parseInput(struct com_QueueJob *job){
    struct usr_UserData *user = job->user;
    struct chat_Message *cmd = malloc(sizeof(struct chat_Message));

    if(cmd == NULL){
            log_logError("Error creating cmd struct", DEBUG);
            return -1;
    }
    memset(cmd, 0, sizeof(struct chat_Message)); // Set all memory to 0

    // Find where the message ends (\r or \n); if not supplied just take the very end of the buffer
    int length = 0;
    int currentPos = 0, loc = 0; // Helps to keep track of where string should be copied
    for (int i = 0; i < ARRAY_SIZE(job->str)-1; i++){
        if(job->str[i] == '\n' || job->str[i] == '\r' || job->str[i] == '\0'){
            length = i+1;
            job->str[i] = ' ';
            break;
        }

		// Remove any non-printable characters
		if(iscntrl(job->str[i]) == 1){
			job->str[i] = ' ';
		}
    }

    log_logMessage(job->str, MESSAGE);

    if(job->str[0] == ':'){
       loc = chat_findNextSpace(0, length, job->str);
       if (loc > -1){
           memcpy(cmd->prefix, &job->str[1], loc - 1); // Copy everything except for the ':'
           currentPos = loc + 1;
       }
    }

    loc = chat_findNextSpace(currentPos, length, job->str); 
    if (loc >= 0){
        memcpy(cmd->command, &job->str[currentPos], loc - currentPos);
        currentPos = loc + 1;
    }

    // Fill in up to 15 params
    while (loc > -1 && cmd->paramCount < 15){
       loc = chat_findNextSpace(currentPos, length, job->str);
       if (loc >= 0){
            if(job->str[currentPos] == ':'){ // Colon means rest of string is together
                memcpy(cmd->params[cmd->paramCount], &job->str[currentPos], length - currentPos);
                cmd->paramCount++;
                break;
            }

            memcpy(cmd->params[cmd->paramCount], &job->str[currentPos], loc - currentPos);
            currentPos = loc +1;
            cmd->paramCount++;
       }
    }

    cmd->user = user;

    return chat_insertQueue(user, 1, NULL, cmd);
}

int chat_sendMessage(struct chat_Message *msg) {
    if(msg == NULL){
        return -1;
    }

    char str[BUFSIZ];
    chat_messageToString(msg, str, ARRAY_SIZE(str));
    com_sendStr(msg->user, str);

    return 1;
}

int chat_sendServerMessage(struct chat_Message *cmd){
    struct usr_UserData *user;
    char str[BUFSIZ];
    chat_messageToString(cmd, str, ARRAY_SIZE(str));

    for(int i = 0; i < serverLists.max; i++){
        user = &serverLists.users[i];

		pthread_mutex_lock(&user->userMutex);
		int id = user->id;
		pthread_mutex_unlock(&user->userMutex);

		if(user == cmd->user){ // Dont send to sender
			continue;
		}

		if(id != -1){
			com_sendStr(user, str);
		}
    }

    return 1;
}

int chat_createMessage(struct chat_Message *msg, struct usr_UserData *user, char *prefix, char *cmd, char **params, int paramCount) {
    msg->user = user;

	msg->prefix[0] = '\0';
    if(prefix != NULL){ // Automatically insert a ':' infront
        msg->prefix[0] = ':';
        strncpy(&msg->prefix[1], prefix, ARRAY_SIZE(msg->prefix)-1);
    }
    strncpy(msg->command, cmd, ARRAY_SIZE(msg->command));

    for (int i = 0; i < paramCount; i++){
		if(params[i] != NULL)
			strncpy(msg->params[i], params[i], ARRAY_SIZE(msg->params[i]));
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
    
    int newLen = sizeStr - strlen(str);
    for (int i = 0; i < msg->paramCount && newLen > 0; i++){
            strncat(str, " ", newLen);
            strncat(str, msg->params[i], newLen - 1);
            newLen -= strlen(str);
    }

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

// General character location
int chat_findCharacter(char *str, int size, char key){
	for(int i = 0; i < size; i++){
		if(str[i] == key)
			return i;
	}

	return -1;
}

// Divide a string into groupname and channelname
int chat_divideChanName(char *str, int size, char data[2][1000]){
	if(str[0] == '#'){ // Only channel
		data[0][0] = '\0';
		strncpy(data[1], str, ARRAY_SIZE(data[1]));
		return 1;
	}

	int divide = chat_findCharacter(str, size, '/');
	if(divide == -1){ // Only group or neither
		if(str[0] == '&'){
			strncpy(data[0], str, ARRAY_SIZE(data[1]));
			data[1][0] = '\0';
			return 1;
		}

		return -1;
	}

	// Normal, both
	strncpy(data[0], str, divide);
	strncpy(data[1], &str[divide+1], ARRAY_SIZE(data[1]));
	return 1;
}
