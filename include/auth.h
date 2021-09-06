#ifndef auth_h
#define auth_h

#include <openssl/sha.h>
#include <pthread.h>
#include <stdio.h>
#include <user.h>
#include "boundless.h"
#include "user.h"
#include "cluster.h"
#include "hstring.h"
#include "logging.h"
#include "config.h"
#include "ssl.h"

/*	This header provides methods to authenticate a user */

// Checks if the given username and password will validate an operator
int auth_checkOper(char *user, char *pass);

unsigned char *auth_hashString(char *str, char *salt, unsigned char ret[SHA256_DIGEST_LENGTH]);

#endif
