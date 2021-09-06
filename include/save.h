#ifndef save_h
#define save_h

#include <stdio.h>
#include <sqlite3.h>
#include "logging.h"
#include "user.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))

#define ID_COL 0
#define NICK_COL 1
#define PASS_COL 2

/*	The USERS table
	ID | NICK | PASS
	int| text | text
	----------------
	01 | name | hash
*/

// TODO - premade statements

sqlite3 *init_save(char *saveFile);

int save_saveUserPassword(struct usr_UserData *user, char *password);

int save_createUser(struct usr_UserData *user, char *password);

// Will load a user from the SQL table, pass is optional verification
int save_loadUser(char *name, struct usr_UserData *user, char *pass);

void save_logError(char *message, int code, int type);

#endif
