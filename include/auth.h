#ifndef auth_h
#define auth_h

#include "boundless.h"
#include "user.h"
#include "cluster.h"
#include "hstring.h"
#include "logging.h"
#include "config.h"

// Checks if the given username and password will validate an operator
int auth_checkOper(char *user, char *pass);

#endif
