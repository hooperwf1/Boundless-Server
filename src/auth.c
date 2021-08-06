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
