#include "save.h"

// TODO thread_local
sqlite3 *database;

sqlite3 *init_save(char *saveFile){
	sqlite3 *db;
	int setup = -1;

	int ret = sqlite3_open_v2(saveFile, &db, SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX, NULL);
	if(ret != SQLITE_OK){
		save_logError("Error opening db...creating", ret, ERROR);

		ret = sqlite3_open_v2(saveFile, &db, SQLITE_OPEN_READWRITE|SQLITE_OPEN_FULLMUTEX|SQLITE_OPEN_CREATE, NULL);
		if(ret != SQLITE_OK){
			save_logError("Error creating database", ret, ERROR);
			return NULL;
		}

		setup = 1; // Perform inital setup
	}

	if(setup == 1){
		char *errMsg;
		char *cmd = "CREATE TABLE IF NOT EXISTS USERS(" \
					"ID		integer		PRIMARY KEY	NOT NULL," \
					"NICK	text		NOT NULL	UNIQUE," \
					"PASS	text);";

		if(sqlite3_exec(db, cmd, NULL, NULL, &errMsg) != SQLITE_OK){
			log_logMessage(errMsg, ERROR);
			sqlite3_free(errMsg);
			sqlite3_close(db);
			return NULL;
		}
	}

	log_logMessage("Setup local save database.", INFO);
	database = db;
	return db;
}

int save_updateUserNick(struct usr_UserData *user, char *nick){
	if(user == NULL || user->id == -1)
		return -1;

	char cmd[BUFSIZ];
	snprintf(cmd, ARRAY_SIZE(cmd), "UPDATE USERS SET NICK = \"%s\" WHERE ID is %d;", nick, user->id);

	char *errMsg;
	if(sqlite3_exec(database, cmd, NULL, NULL, &errMsg) != SQLITE_OK){
		log_logMessage(errMsg, ERROR);
		sqlite3_free(errMsg);
		return -1;
	}



	return 1;
}

int save_verifyPassword(char *nick, char *pass){
	char cmd[BUFSIZ];
	snprintf(cmd, ARRAY_SIZE(cmd), "SELECT PASS FROM USERS WHERE NICK is \"%s\";", nick);

	sqlite3_stmt *statement;
	int ret = sqlite3_prepare_v2(database, cmd, ARRAY_SIZE(cmd), &statement, NULL);
	if(ret != SQLITE_OK){
		save_logError("Error opening database", ret, ERROR);
		return -1;
	}

	// Only once to prevent data leaks
	ret = sqlite3_step(statement);
	if(ret == SQLITE_DONE) { // No such user
		sqlite3_finalize(statement);
		return -2;
	} else if(ret != SQLITE_ROW) { // Invalid data
		sqlite3_finalize(statement);
		return -1;
	}

	char *hashPass = (char *) sqlite3_column_text(statement, 0);

	if(auth_verifyPassword(pass, hashPass, fig_Configuration.salt) == -1)
		return -1;

	sqlite3_finalize(statement);
	return 1;
}

int save_saveUserPassword(struct usr_UserData *user, char *password){
	if(user == NULL || password == NULL)
		return -1;

	char hashPass[SHA256_DIGEST_LENGTH_HEX];
	if(auth_hashStringHex(password, fig_Configuration.salt, hashPass) == NULL){
		log_logMessage("Error with hashing", ERROR);
		return -1;
	}

	char nick[fig_Configuration.nickLen];
	usr_getNickname(nick, user);

	char cmd[BUFSIZ];
	snprintf(cmd, ARRAY_SIZE(cmd), "UPDATE USERS SET PASS = \"%s\" WHERE NICK is \"%s\";", hashPass, nick);

	char *errMsg;
	if(sqlite3_exec(database, cmd, NULL, NULL, &errMsg) != SQLITE_OK){
		log_logMessage(errMsg, ERROR);
		sqlite3_free(errMsg);
		return -1;
	}

	return 1;
}

int save_createUser(struct usr_UserData *user, char *password){
	if(user == NULL)
		return -1;

	char nick[fig_Configuration.nickLen];
	usr_getNickname(nick, user);

	char cmd[BUFSIZ];
	snprintf(cmd, ARRAY_SIZE(cmd), "INSERT INTO USERS (ID,NICK) VALUES(NULL, \"%s\");", nick);

	char *errMsg;
	if(sqlite3_exec(database, cmd, NULL, NULL, &errMsg) != SQLITE_OK){
		log_logMessage(errMsg, ERROR);
		sqlite3_free(errMsg);
		return -1;
	}

	save_saveUserPassword(user, password);

	return 1;
}

int save_loadUser(char *name, struct usr_UserData *user){
	if(user == NULL || name == NULL)
		return -1;

	char cmd[BUFSIZ];
	snprintf(cmd, ARRAY_SIZE(cmd), "SELECT ID, NICK FROM USERS WHERE NICK is \"%s\";", name);

	sqlite3_stmt *statement;
	int ret = sqlite3_prepare_v2(database, cmd, ARRAY_SIZE(cmd), &statement, NULL);
	if(ret != SQLITE_OK){
		save_logError("Error opening database", ret, ERROR);
		return -1;
	}

	// Only once to prevent data leaks
	ret = sqlite3_step(statement);
	if(ret == SQLITE_DONE) { // No such user
		sqlite3_finalize(statement);
		return -2;
	} else if(ret != SQLITE_ROW) { // Invalid data
		sqlite3_finalize(statement);
		return -1;
	}

	int numCols = sqlite3_column_count(statement);
	int id = -1;
	char *nick = NULL;
	for (int i = 0; i < numCols; i++){
		switch(i){
			case 0: // ID
				id = sqlite3_column_int(statement, i);
				break;

			case 1: // NICK
				nick = (char *) sqlite3_column_text(statement, i);
				break;
		}
	}

	// Make sure name matches input 
	if(strncmp(nick, name, strlen(nick)) != 0){
		sqlite3_finalize(statement);
		return -1;
	}

	// Fill in user data
	pthread_mutex_lock(&user->mutex);
	user->id = id;
    strhcpy(user->nickname, name, fig_Configuration.nickLen);
	pthread_mutex_unlock(&user->mutex);

	sqlite3_finalize(statement);
	return 1;
}

void save_logError(char *message, int code, int type){
	char buff[BUFSIZ];
	snprintf(buff, ARRAY_SIZE(buff), "%s: %s", message, sqlite3_errstr(code));
	log_logMessage(buff, type);
}
