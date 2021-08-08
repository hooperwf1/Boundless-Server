#ifndef ssl_h
#define ssl_h

#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include "logging.h"
#include "hstring.h"

#define UNUSED(x) x __attribute__((unused))

void init_ssl();

// Will init a ctx
SSL_CTX *ssl_getCtx(char *certFile, char *keyFile, char *passwd);

char *ssl_errToString(char *str, int size);

#endif
