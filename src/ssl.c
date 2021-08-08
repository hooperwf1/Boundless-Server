#include "ssl.h"

void init_ssl(){
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
}

// Used to specify a private key file password (man pem_password_cb)
char *pass;
int password_cb(char *buf, int size, UNUSED(int rwflag), void *u){
	printf("lel %s\n", (char *) u);
	return strhcpy(buf, pass, size);
}

SSL_CTX *ssl_getCtx(char *certFile, char *keyFile, char *passwd){
	SSL_CTX *ctx;
	char buff[1024];

	ctx = SSL_CTX_new(TLS_method());
	if(ctx == NULL){
		log_logMessage(ssl_errToString(buff, ARRAY_SIZE(buff)), ERROR);
		return NULL;
	}

	// Load certificates
	if(SSL_CTX_use_certificate_chain_file(ctx, certFile) != 1){
		log_logMessage(ssl_errToString(buff, ARRAY_SIZE(buff)), ERROR);
		return NULL;
	}

	// Set password for decryption of private key
	pass = passwd;
	SSL_CTX_set_default_passwd_cb(ctx, password_cb);

	// Get private key
	if(SSL_CTX_use_PrivateKey_file(ctx, keyFile, SSL_FILETYPE_PEM) != 1){
		log_logMessage(ssl_errToString(buff, ARRAY_SIZE(buff)), ERROR);
		return NULL;
	}

	// Make sure private key matches
	if(SSL_CTX_check_private_key(ctx) != 1){
		log_logMessage("Private key does not match the certificate", ERROR);
		return NULL;
	}

	return ctx;
}

char *ssl_errToString(char *str, int size){
	BIO *bio = BIO_new(BIO_s_mem());
	ERR_print_errors(bio);

	char *buff = NULL;
	BIO_get_mem_data(bio, &buff);

	memcpy(str, buff, size-1);
	buff[size-1] = '\0';
	BIO_free(bio);
	return str;
}
