#ifndef boundless_h
#define boundless_h

#define NUM_MODES 15
#define KEY_LEN 20
#define CONFIG_FILE "/etc/boundless-server/settings.conf"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include "logging.h"
#include "config.h"
#include "communication.h"
#include "linkedlist.h"
#include "modes.h"
#include "chat.h"
#include "commands.h"
#include "security.h"
#include "events.h"
#include "user.h"
#include "channel.h"
#include "group.h"

#endif
