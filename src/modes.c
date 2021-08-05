#include "modes.h"

char mode_groupModes[] = {'o', 's', 'i', 'b', 'v', 'm', 'k'};
char mode_chanModes[] = {'o', 's', 'i', 'b', 'v', 'm', 'k'};
char mode_userModes[] = {'i', 'o', 'r', 'a'};

int mode_arrayHasMode(struct clus_Cluster *c, char mode){
	int ret = -1;

	pthread_mutex_lock(&c->mutex);
	for (int i = 0; i < NUM_MODES; i++){
		if(c->modes[i] == mode) {
			ret = 1;
		}
	}
	pthread_mutex_unlock(&c->mutex);

	return ret;
}

void mode_editArray(struct clus_Cluster *c, char op, char mode){
	if(op == '+' && mode_arrayHasMode(c, mode) == 1)
		return; // No duplicates

	pthread_mutex_lock(&c->mutex);
	for(int i = 0; i < NUM_MODES; i++){
		if(op == '-' && c->modes[i] == mode){
			c->modes[i] = '\0';
			break;
		} else if (op == '+' && c->modes[i] == '\0'){
			c->modes[i] = mode;
			break;
		}
	}
	pthread_mutex_unlock(&c->mutex);
}

int mode_isValidMode(char mode, int type){
	char *modes;
	int size;

	switch(type){
		case TYPE_USER:
			modes = mode_userModes;
			size = ARRAY_SIZE(mode_userModes);
			break;

		case TYPE_CHAN:
			modes = mode_chanModes;
			size = ARRAY_SIZE(mode_chanModes);
			break;

		case TYPE_GROUP:
			modes = mode_groupModes;
			size = ARRAY_SIZE(mode_groupModes);
			break;
	}

	for(int i = 0; i < size; i++){
		if(mode == modes[i])
			return 1;
	}

	return -1;
}

char *mode_setKey(struct clus_Cluster *c, char *key){
	if(c == NULL)
		return ERR_UNKNOWNERROR;

	if(key == NULL)
		return ERR_NEEDMOREPARAMS;

	if(mode_arrayHasMode(c, 'k') == 1)
		return ERR_KEYSET;

	pthread_mutex_lock(&c->mutex);
	strhcpy(c->key, key, KEY_LEN);
	pthread_mutex_unlock(&c->mutex);

	mode_editArray(c, '+', 'k');

	return NULL;
}

char *mode_removeKey(struct clus_Cluster *c, char *key){
	if(key == NULL)
		return ERR_NEEDMOREPARAMS;

	// If keys match, remove channel's key
	pthread_mutex_lock(&c->mutex);
	int res = sec_constantStrCmp(c->key, key, KEY_LEN-1);
	pthread_mutex_unlock(&c->mutex);

	if(res == 1){
		c->key[0] = '\0';
	} else {
		return ERR_BADCHANNELKEY;
	}

	mode_editArray(c, '-', 'k');

	return NULL;
}

int mode_checkKey(struct clus_Cluster *c, char *key){
	// No need to check if there is no key to validate
	if(mode_arrayHasMode(c, 'k') == -1)
		return 1;

	if(key == NULL)
		return -1;

	int res = -1;
	pthread_mutex_lock(&c->mutex);
	res = sec_constantStrCmp(c->key, key, KEY_LEN-1);
	pthread_mutex_unlock(&c->mutex);

	return res;
}

