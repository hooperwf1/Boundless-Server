#include "communication.h"

struct link_List serverUsers;
pthread_t *threads;
int com_epollfd = -1;
int com_numThreads = -1;

SSL_CTX *com_ctx;

extern struct chat_ServerLists serverLists;

int init_server(){
	if(fig_Configuration.numSSLPorts > 0) { // Don't start if not needed
		init_ssl(); 
		com_ctx = ssl_getCtx(fig_Configuration.sslCert, fig_Configuration.sslKey, fig_Configuration.sslPass);
		if(com_ctx == NULL)
			return -1;
		log_logMessage("Started SSL", INFO);	
	}

	// Create the epoll
	com_epollfd = epoll_create1(0);
	if(com_epollfd == -1){
		log_logError("Error setting up epoll", FATAL);
		return -1;
	}

	// Init all specified ports
	int size = fig_Configuration.numPorts + fig_Configuration.numSSLPorts;
	if(size < 1){
		log_logMessage("No ports specified", ERROR);
		return -1;
	}

	for (int i = 0; i < size; i++){
		int useSSL, port;
		if(i > fig_Configuration.numPorts - 1){ // SSL
			useSSL = 1;
			port = fig_Configuration.sslPort[i-fig_Configuration.numPorts];
		} else { // Not secure
			useSSL = 0;
			port = fig_Configuration.port[i];
		}


		// Initalize the server socket
		struct com_SocketInfo serverSockAddr;
		int socketfd;
		socketfd = com_startServerSocket(port, &serverSockAddr, 0, useSSL);
		if(socketfd < 0){
			log_logMessage("Retrying with IPv4...", INFO);
			socketfd = com_startServerSocket(port, &serverSockAddr, 1, useSSL);
			if(socketfd < 0){
					return -1;
			}
		}

		// TODO deal with random users sending data to this user
		struct usr_UserData *serverUser;
		serverUser = usr_createUser(&serverSockAddr, fig_Configuration.serverName);
		if(!serverUser){
			log_logMessage("Unable to create the SERVER user", ERROR);
			return -1;
		}

		// Setup listening socket in the epoll	
		struct epoll_event ev;
		ev.events = EPOLLIN|EPOLLONESHOT;
		ev.data.ptr = serverUser;
		if(epoll_ctl(com_epollfd, EPOLL_CTL_ADD, socketfd, &ev) == -1){
			log_logError("Error adding listening socket to epoll", FATAL);
			return -1;
		}

		link_add(&serverUsers, serverUser);
	}

    //Setup threads for listening
    com_numThreads = com_setupIOThreads(&fig_Configuration);

    return 1;
}

// TODO - proper close
void com_close(){
    free(threads);
}

// Make a new job and insert it into the queue for sending
int com_sendStr(struct usr_UserData *user, char *msg){
	if(user == NULL || user == &serverLists.users[0])
		return -1;

    struct com_QueueJob *job = calloc(1, sizeof(struct com_QueueJob));
    job->user = user;

    snprintf(job->str, ARRAY_SIZE(job->str), "%s\r\n", msg);
    com_insertQueue(job);

	// Safely get user's socket
	pthread_mutex_lock(&user->mutex);
	int sock = user->socketInfo.socket2;
	pthread_mutex_unlock(&user->mutex);

	// Setup to allow for a write
	struct epoll_event ev = {.events = EPOLLOUT|EPOLLONESHOT};
	ev.data.ptr = user;
	if(epoll_ctl(com_epollfd, EPOLL_CTL_MOD, sock, &ev) == -1){
		log_logError("Error rearming write socket", WARNING);
		usr_generateQuit(user, ":Socket error");
		return -1;
	}

    return 1;
}

int com_insertQueue(struct com_QueueJob *job){
    struct usr_UserData *user = job->user;

	if(job == NULL){
		log_logMessage("Invalid job.", DEBUG);
		return -1;
	}

    if(!user || user->id == -1){ // User is disconnecting; nothing new to be sent
        log_logMessage("User no longer valid", TRACE);
        return -1; 
    }

    pthread_mutex_lock(&user->mutex);
	struct link_Node *ret = link_add(&user->sendQ, job);
	if(ret == NULL){
		log_logMessage("Error adding job to queue", WARNING);
	}
    pthread_mutex_unlock(&user->mutex);

    return 1;
}

int getHost(char ipstr[INET6_ADDRSTRLEN], struct sockaddr_storage addr, int protocol){
	void *addrSrc;

	if(protocol == AF_INET){
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		addrSrc = &s->sin_addr;
	} else if (protocol == AF_INET6){
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		addrSrc = &s->sin6_addr;
	} else {
		log_logMessage("Invalid socket family", WARNING);
		return -1;
	}

	if(inet_ntop(protocol, addrSrc, ipstr, INET6_ADDRSTRLEN) == NULL){
		log_logError("Error getting hostname", WARNING);
		return -1;
	}

	return 0;
}

// Will read data from the socket and properly send it for processing
int com_readFromSocket(struct epoll_event *userEvent, int epollfd){
	struct usr_UserData *user = userEvent->data.ptr;
	if(user == NULL){
		log_logMessage("No user associated with socket", ERROR);
		return -1;
	}
	int sockfd = user->socketInfo.socket;
		
	char buff[MAX_MESSAGE_LENGTH+1] = {0};
	int bytes;

	// Read with or without SSL
	if(user->socketInfo.useSSL == 1){
		bytes = SSL_read(user->socketInfo.ssl, buff, ARRAY_SIZE(buff)-1);
	} else {
		bytes = read(sockfd, buff, ARRAY_SIZE(buff)-1);
	}

	switch(bytes){
		case 0:
			log_logMessage("Client disconnect.", INFO);
			usr_generateQuit(user, ":Disconnect");
			break;
		case 1:
			log_logError("Error reading from client.", WARNING);
			usr_generateQuit(user, ":Socket error");
			break;

		default: ; 
			// Rearm the fd because data has already been read
			struct epoll_event ev = {.events = EPOLLIN|EPOLLONESHOT, .data.ptr = user};
			if(epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev) == -1){
				usr_generateQuit(user, ":Socket error");
				log_logError("Error rearming socket", ERROR);
				return -1;
			}

			user->pinged = -1; // Reset ping

			// Make sure that the user isn't flooding the server
			if(usr_handleFlooding(user) == 1)
				return -1;

			// Split up each line into its own job
			int loc = 0;
			while(loc >= 0){
				int oldLoc = loc;
				char line[1024];

				// Split each line into its own job
				loc = chat_findEndLine(buff, ARRAY_SIZE(buff), loc);
				if(loc > -1)
					buff[loc - 1] = '\0';
				strhcpy(line, &buff[oldLoc], ARRAY_SIZE(line));
				chat_insertQueue(user, 0, line, NULL);
			}
	}

	return 1;
}

// Writes avaliable queue data to socket
int com_writeToSocket(struct epoll_event *userEvent, int epollfd){
	struct usr_UserData *user = userEvent->data.ptr;
	if(user == NULL){
		log_logMessage("No user associated with socket", ERROR);
		return -1;
	}

	// Remove the first job
	struct com_QueueJob *job = NULL;
	pthread_mutex_lock(&user->mutex);
	if(link_isEmpty(&user->sendQ) == -1){
		job = link_removeNode(&user->sendQ, user->sendQ.head);
	}
	pthread_mutex_unlock(&user->mutex);

	if(job == NULL)
		return -1;

	char buff[ARRAY_SIZE(job->str)];
	strhcpy(buff, job->str, ARRAY_SIZE(buff));
	free(job);

	pthread_mutex_lock(&user->mutex);
	int socket = user->socketInfo.socket2;
	if(user->id < 0)
		socket = -1;
	pthread_mutex_unlock(&user->mutex);

	if(socket < 0)
		return -1;

	log_logMessage(buff, MESSAGE);
	int ret;
	if(user->socketInfo.useSSL == 1){
		ret = SSL_write(user->socketInfo.ssl, buff, strlen(buff));
	} else {
		ret = write(user->socketInfo.socket2, buff, strlen(buff));
	}
	if(ret == -1){
		log_logError("Error writing to client", ERROR);
		usr_generateQuit(user, ":Socket error");
		return -1;
	}
	
	// Setup to allow for another write if avaliable
	struct epoll_event ev = {.events = EPOLLOUT|EPOLLONESHOT};
	ev.data.ptr = user;
	if(epoll_ctl(epollfd, EPOLL_CTL_MOD, user->socketInfo.socket2, &ev) == -1){
		log_logError("Error rearming write socket", WARNING);
		usr_generateQuit(user, ":Socket error");
		return -1;
	}
	
	return 0;
}

void *com_communicateWithClients(void *param){
    int *epollfd = param;
	struct usr_UserData *user;

	// First is options, second is storage
	struct epoll_event events[10];
    int num;
	
    while(1){
        num = epoll_wait(*epollfd, events, ARRAY_SIZE(events), -1); 
		if(num == -1){
			if(num == EINTR)
				continue; // OK to continue

			log_logError("epoll_wait", ERROR);
			exit(EXIT_FAILURE);
		}

		for(int i = 0; i < num; i++){
			user = events[i].data.ptr;
			if(link_contains(&serverUsers, user) == 1){
				com_acceptClient(user, *epollfd, com_ctx);
				continue;
			}

			if(events[i].events & EPOLLIN){
				com_readFromSocket(&events[i], *epollfd);
			} else if (events[i].events & EPOLLOUT){
				com_writeToSocket(&events[i], *epollfd);
			} // Add disconnection/error
		}
    }
    
    return NULL;
}

int com_setupIOThreads(struct fig_ConfigData *config){
    char buff[BUFSIZ];
    int numThreads = config->threadsIO;

	threads = calloc(numThreads, sizeof(pthread_t));
	if(threads == NULL){
		log_logError("Error allocating space for IO threads", FATAL);
		exit(EXIT_FAILURE);
	}

    int ret = 0;
    for(int i = 0; i < numThreads; i++){
        ret = pthread_create(&threads[i], NULL, com_communicateWithClients, &com_epollfd);
        if(ret != 0){
            snprintf(buff, ARRAY_SIZE(buff), "Error with pthread_create: %d", ret);
            log_logMessage(buff, ERROR);
            return -1;
        }
    }
	snprintf(buff, ARRAY_SIZE(buff), "Successfully listening on %d threads", numThreads);
	log_logMessage(buff, INFO);

    return numThreads;
}

int com_acceptClient(struct usr_UserData *serverUsr, int epoll_sock, SSL_CTX *ctx){
	char buff[BUFSIZ];

	struct com_SocketInfo *serverSock = &serverUsr->socketInfo;
	struct sockaddr_storage cliAddr;
	socklen_t cliAddrSize = sizeof(cliAddr);

	//Accept client's connection and log its IP
	struct com_SocketInfo newCli;
	int client = accept(serverSock->socket, (struct sockaddr *)&cliAddr, &cliAddrSize);

	// Reset listening socket
	struct epoll_event ev = {.events = EPOLLIN | EPOLLONESHOT}; 
	ev.data.ptr = serverUsr;
	if(epoll_ctl(epoll_sock, EPOLL_CTL_MOD, serverSock->socket, &ev) == -1){
		log_logError("epoll_ctl rearming server", ERROR);
		return -1;
	}

	if(client < 0){
		log_logError("Error accepting client", WARNING);
		return -1;
	}

	// Enable SSL
	if(serverSock->useSSL == 1){
		SSL *ssl;
		ssl = SSL_new(ctx);
		SSL_set_fd(ssl, client);
		if(SSL_accept(ssl) != 1){
			close(client);
			log_logMessage("Error initialzing SSL for client", ERROR);
			return -1;
		}
		newCli.useSSL = 1;
		newCli.ssl = ssl;
	}

	char ipstr[INET6_ADDRSTRLEN];
	if(!getHost(ipstr, cliAddr, serverSock->addr.ss_family)){
		snprintf(buff, ARRAY_SIZE(buff), "New connection from: %s", ipstr);
		log_logMessage(buff, INFO);
	}

	// Fill in newCli struct
	newCli.socket = client;
	memcpy(&newCli.addr, &cliAddr, cliAddrSize);
	newCli.socket2 = dup(client);
	if(newCli.socket2 == -1){
		log_logError("Error creating 2nd fd for client", WARNING);
		return -1;
	}

	if(chat_serverIsFull() == 1){
		snprintf(buff, ARRAY_SIZE(buff), "Server is full, try again later.");
		send(client, buff, strlen(buff), 0);
		close(client);
		close(newCli.socket2);
		
		return -1;
	} else {
		struct usr_UserData *user = usr_createUser(&newCli, UNREGISTERED_NAME);
		if(user == NULL){
			close(client);
			close(newCli.socket2);
			return -1;
		} 
		ev.data.ptr = user;

		// Add to epoll
		if(epoll_ctl(epoll_sock, EPOLL_CTL_ADD, client, &ev) == -1){
			log_logError("epoll_ctl accepting client", WARNING);
			return -1;
		}

		// Setup to allow for a write
		ev.events = EPOLLOUT|EPOLLONESHOT;
		if(epoll_ctl(epoll_sock, EPOLL_CTL_ADD, newCli.socket2, &ev) == -1){
			log_logError("epoll_ctl writing to client", WARNING);
			return -1;
		}
	}

	return 0;
}

int com_startServerSocket(int portNum, struct com_SocketInfo* sockAddr, int forceIPv4, int useSSL){
	int sock;
	struct addrinfo hints;
	struct addrinfo *res, *rp;

	memset(&hints, 0, sizeof(hints));
	if(forceIPv4){
		hints.ai_family = AF_INET;
	} else {
		hints.ai_family = AF_INET6;
	}
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_V4MAPPED;

	//convert int PORT to string
	char port[6];
	snprintf(port, ARRAY_SIZE(port), "%d", portNum);

	int ret = getaddrinfo(NULL, port, &hints, &res);
	if(ret != 0){
		char msg[BUFSIZ];
		snprintf(msg, ARRAY_SIZE(msg), "getaddrinfo: %s", gai_strerror(ret));
		log_logMessage(msg, ERROR);
		return -1;
	}

	// Go through all possible options given by getaddrinfo
	for(rp = res; rp != NULL; rp = rp->ai_next){
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if(sock < 0){
			continue;
		}

		//successful
		if(!bind(sock, rp->ai_addr, rp->ai_addrlen)){
			char msg[BUFSIZ];
			snprintf(msg, ARRAY_SIZE(msg), "Binded server to port %s", port);
			log_logMessage(msg, 2);
			break; 
		}

		close(sock);
	}	

	// No address succeeded
	if(rp == NULL){
		log_logError("No addresses sucessfully binded", ERROR);
		return -1;
	}

	//Copy sockaddr struct into the sockAddr struct for later use
	if(sockAddr != NULL){
		memcpy(&sockAddr->addr, &rp->ai_addr, sizeof(rp->ai_addr));
		sockAddr->addr.ss_family = rp->ai_family;
		sockAddr->useSSL = useSSL;
	}

	freeaddrinfo(res);

	ret = listen(sock, 128);
	if(ret != 0){
		log_logError("Error listening on socket", ERROR);
		close(sock);
		return -1;
	} else {
		char msg[BUFSIZ];
		snprintf(msg, ARRAY_SIZE(msg), "Listening to port: %s.", port);
		log_logMessage(msg, INFO);
	}

	sockAddr->socket = sock;
	return sock;
}
