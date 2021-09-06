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
