#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include "hstring.h"
#include "logging.h"
#include "config.h"
#include "communication.h"
#include "linkedlist.h"
#include "chat.h"
#include "commands.h"
#include "ssl.h"

void cleanUpServer(){
	log_logMessage("Server is now quitting.", INFO);

	log_close();
	com_close();
	chat_close();
	events_close();
}

int main(){
	log_logMessage("Now starting boundless.chat server: V1.2.0.", INFO);
    atexit(cleanUpServer);

    if(init_config(CONFIG_FILE) == -1) /* config.h */
		return -1;

    if(init_logging() == -1) /* logging.h */
		return -1;

	sqlite3 *db = init_save(fig_Configuration.saveDataFile);
	if(db == NULL) /* save.h */
		return -1;

	struct chat_ServerLists *sLists = init_chat();
    if(sLists == NULL) /* chat.h */
		return -1;
	sLists->db = db;

	struct com_ConnectionList *cList = init_server();
    if(cList == NULL) /* communication.h */
		return -1;
	cList->sLists = sLists;

    if(init_commands() == -1) /* commands.h */
		return -1;

    if(init_events() == -1) /* events.h */
		return -1;

	evt_userTimeout(cList);
	evt_executeEvents();

    return 0;
}

int strToInt(char *str){
	int val = strtol(str, NULL, 10);

	if(errno != 0){
		log_logError("Error converting string to int.", WARNING);
		return -1;
	}

	return val;
}
