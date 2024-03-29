#ifndef config_h
#define config_h

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "logging.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))
#define MAX_STRLEN (128)


//struct that will store all configuration data
struct fig_ConfigData {
	char logDirectory[BUFSIZ];
	char serverName[BUFSIZ];
	char defaultGroup[BUFSIZ];
	char welcomeMessage[BUFSIZ];
	char oper[2][BUFSIZ];
	char sslCert[BUFSIZ];
	char sslKey[BUFSIZ];
	char sslPass[BUFSIZ];

	int port[10], sslPort[10];
	int numPorts, numSSLPorts;
		
	int useFile;
	int threadsIO, threadsDATA;
	int clients;
	int nickLen, chanNameLength, groupNameLength;
	int timeOut, floodInterval, floodNum;
	int maxChannels, maxUserGroups;
};	

// Struct to store all config data
extern struct fig_ConfigData fig_Configuration;

int init_config(char *dir);

//split a line into its individual words
//return value is the number of words
//will stop splitting if detects a '#' character
int fig_splitWords(char *line, char words[10][MAX_STRLEN]);

void fig_parseLine(char *line, int lineNo);

//read a configuration file into the fig_ConfigData struct
int fig_readConfig(char *path);

int fig_boolToInt(char *str);

//make sure all config values are valid, if not set them to default
int fig_editConfigInt(int *orig, char *str, int lineNo);

#endif
