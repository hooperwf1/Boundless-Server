#include "auth.h"

int auth_checkOper(char *user, char *pass){
	int ret;

	// Username
	ret = sec_constantStrCmp(fig_Configuration.oper[0], user, ARRAY_SIZE(fig_Configuration.oper[0]));
	if(ret != 1)
		return -1;

	// Password
	ret = sec_constantStrCmp(fig_Configuration.oper[1], pass, ARRAY_SIZE(fig_Configuration.oper[1]));
	if(ret != 1)
		return -1;

	return 1;
}

unsigned char *auth_hashString(char *str, char *salt, unsigned char ret[SHA256_DIGEST_LENGTH]){
	if(str == NULL)
		return NULL;

	SHA256_CTX c;

	if(SHA256_Init(&c) == 0)
		return NULL;

	// Append salt
	char buff[BUFSIZ];
	snprintf(buff, ARRAY_SIZE(buff), "%s%s", salt, str);
	
	if(SHA256_Update(&c, buff, strlen(buff)) == 0)
		return NULL;

	if(SHA256_Final(ret, &c) == 0)
		return NULL;

	return ret;
}

char *auth_hashStringHex(char *str, char *salt, char hex[SHA256_DIGEST_LENGTH_HEX]){
	if(str == NULL)
		return NULL;

	unsigned char md[SHA256_DIGEST_LENGTH];
	if(auth_hashString(str, salt, md) == NULL)
		return NULL;

	// Convert to hex chars
	hex[0] = '\0';
	for(int i = 0; i < SHA256_DIGEST_LENGTH; i++){
		char tmp[3];
		snprintf(tmp, 3, "%02x", md[i]);
		strhcat(hex, tmp, SHA256_DIGEST_LENGTH_HEX);
	}

	return hex;
}

int auth_verifyPassword(char *pass, char *hashPass, char *salt){
	char newHash[SHA256_DIGEST_LENGTH_HEX];
	auth_hashStringHex(pass, salt, newHash);

	if(!strncmp(newHash, hashPass, ARRAY_SIZE(newHash)))
		return 1;

	return -1;
}
