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
#include "hstring.h"
#include "ssl.h"

#define SHA256_DIGEST_LENGTH_HEX (SHA256_DIGEST_LENGTH * 2) + 1

/*	This header provides methods to authenticate a user */

// Checks if the given username and password will validate an operator
int auth_checkOper(char *user, char *pass);

unsigned char *auth_hashString(char *str, char *salt, unsigned char ret[SHA256_DIGEST_LENGTH]);

// Hex is the result hex string
char *auth_hashStringHex(char *str, char *salt, char hex[20]);

// NOTE: this expects hashPass to be derived from auth_hashStringHex not auth_hashString
int auth_verifyPassword(char *pass, char *hashPass, char *salt);

#endif
