#include "communication.h"

struct com_Thread *threads;

extern struct chat_ServerLists serverLists;

struct com_ConnectionList *init_server(){
	struct com_ConnectionList *cList = calloc(1, sizeof(struct com_ConnectionList));
	if(cList == NULL){
        log_logError("Error initalizing connection list.", ERROR);
        return NULL;
	}
	cList->max = fig_Configuration.clients + 50;

	cList->cons = calloc(cList->max, sizeof(struct com_Connection));
	if(cList == NULL){
        log_logError("Error initalizing connection list.", ERROR);
		free(cList);
        return NULL;
	}

	// Init connections
	for(int i = 0; i < cList->max; i++){
		if(pthread_mutex_init(&cList->cons[i].mutex, NULL) < 0){
			log_logError("Error initalizing mutex", ERROR);
			free(cList);
			return NULL;
		}
		
		cList->cons[i].type = -1;
	}

	if(fig_Configuration.numSSLPorts > 0) { // Don't start if not needed
		init_ssl(); 
		cList->ctx = ssl_getCtx(fig_Configuration.sslCert, fig_Configuration.sslKey, fig_Configuration.sslPass);
		if(cList->ctx == NULL){
			free(cList);
			return NULL;
		}
		log_logMessage("Started SSL", INFO);	
	}

	// Create the epoll
	cList->epollfd = epoll_create1(0);
	if(cList->epollfd == -1){
		log_logError("Error setting up epoll", FATAL);
		free(cList);
		return NULL;
	}

	// Init all specified ports
	int size = fig_Configuration.numPorts + fig_Configuration.numSSLPorts;
	if(size < 1){
		log_logMessage("No ports specified", ERROR);
		free(cList);
		return NULL;
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
				free(cList);	
				return NULL;
			}
		}

		struct com_Connection *servPort = com_createConnection(PORT, &serverSockAddr, cList);
		if(servPort == NULL)
			return NULL;

		// Setup listening socket in the epoll	
		struct epoll_event ev;
		ev.events = EPOLLIN|EPOLLONESHOT;
		ev.data.ptr = servPort;
		if(epoll_ctl(cList->epollfd, EPOLL_CTL_ADD, socketfd, &ev) == -1){
			log_logError("Error adding listening socket to epoll", FATAL);
			free(cList);
			return NULL;
		}
	}

    //Setup threads for listening
    com_setupIOThreads(&fig_Configuration, cList);

    return cList;
}

// TODO - proper close
void com_close(){
    free(threads);
}

// Make a new job and insert it into the queue for sending
int com_sendStr(struct com_Connection *con, char *msg){
	if(con == NULL || con->cList == NULL)
		return -1;
		
    pthread_mutex_lock(&con->mutex);
	struct com_ConnectionList *cList = con->cList;

	int len = strlen(msg);
	char *data = malloc(len+1);
	strhcpy(data, msg, len + 1);

	// Insert into a connections's sendQ
	struct link_Node *ret = link_add(&con->sendQ, data);
	if(ret == NULL){
		log_logMessage("Error adding job to queue", WARNING);
		free(data);
	}

	// Setup to allow for a write
	struct epoll_event ev = {.events = EPOLLOUT|EPOLLONESHOT};
	ev.data.ptr = con;
	if(epoll_ctl(cList->epollfd, EPOLL_CTL_MOD, con->sockInfo.socket2, &ev) == -1){
		log_logError("Error rearming write socket", WARNING);
		com_deleteConnection(con);
		return -1;
	}
    pthread_mutex_unlock(&con->mutex);

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
int com_readFromSocket(struct epoll_event *conEvent, int epollfd){
	struct com_Connection *con = conEvent->data.ptr;
	if(con == NULL){
		log_logMessage("No connection associated with socket", ERROR);
		return -1;
	}

	pthread_mutex_lock(&con->mutex);
	if(con->type == -1){ // Done
		pthread_mutex_unlock(&con->mutex);
		return -1;
	}
		
	int sockfd = con->sockInfo.socket;
		
	char buff[MAX_MESSAGE_LENGTH+1] = {0};
	int bytes;

	// Read with or without SSL
	if(con->sockInfo.useSSL == 1){
		bytes = SSL_read(con->sockInfo.ssl, buff, ARRAY_SIZE(buff)-1);
	} else {
		bytes = read(sockfd, buff, ARRAY_SIZE(buff)-1);
	}

	pthread_mutex_unlock(&con->mutex);

	switch(bytes){
		case 0:
			log_logMessage("Client disconnect.", INFO);
			com_deleteConnection(con);
			break;
		case 1:
			log_logError("Error reading from client.", WARNING);
			com_deleteConnection(con);
			break;

		default: ; 
			con->pinged = -1; // Reset ping

			// Make sure that the user isn't flooding the server
			if(com_handleFlooding(con) == 1)
				return -1;

			// Split up each line into its own job
			int loc = 0;
			while(loc >= 0 && con->type != -1){
				int oldLoc = loc;
				char line[1024];

				// Split each line into its own job
				loc = chat_findEndLine(buff, ARRAY_SIZE(buff), loc);
				if(loc > -1)
					buff[loc - 1] = '\0';
				strhcpy(line, &buff[oldLoc], ARRAY_SIZE(line));
				log_logMessage(line, MESSAGE);
				chat_processInput(line, con);
			}

			// Rearm the fd after data done processing
			struct epoll_event ev = {.events = EPOLLIN|EPOLLONESHOT, .data.ptr = con};
			if(epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev) == -1){
				log_logError("Error rearming socket", ERROR);
				return -1;
			}
	}

	return 1;
}

// Writes avaliable queue data to socket
int com_writeToSocket(struct epoll_event *conEvent, int epollfd){
	struct com_Connection *con = conEvent->data.ptr;
	if(con == NULL){
		log_logMessage("No connection associated with socket", ERROR);
		return -1;
	}

	// Remove the first msg
	char *data = NULL;
	if(link_isEmpty(&con->sendQ) == -1){
		data = link_removeNode(&con->sendQ, con->sendQ.head);
	}
	int socket = con->sockInfo.socket2;

	if(data == NULL)
		return -1;

	char buff[BUFSIZ];
	strhcpy(buff, data, ARRAY_SIZE(buff));
	free(data);

	if(socket < 0)
		return -1;

	log_logMessage(buff, MESSAGE);

	// Ignore SIGPIPE for this thread temporarily
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	pthread_sigmask(SIG_SETMASK, &set, NULL);

	int ret;
	if(con->sockInfo.useSSL == 1){
		ret = SSL_write(con->sockInfo.ssl, buff, strlen(buff));
	} else {
		ret = write(con->sockInfo.socket2, buff, strlen(buff));
	}

	// Reenable SIGPIPE
	struct timespec time = {0}; // No waiting
	sigtimedwait(&set, NULL, &time);

	if(ret <= 0){
		log_logError("Error writing to client", ERROR);
		com_deleteConnection(con);
		return -1;
	}
	
	// Setup to allow for another write if avaliable
	struct epoll_event ev = {.events = EPOLLOUT|EPOLLONESHOT};
	ev.data.ptr = con;
	if(epoll_ctl(epollfd, EPOLL_CTL_MOD, con->sockInfo.socket2, &ev) == -1){
		log_logError("Error rearming write socket", WARNING);
		return -1;
	}
	
	return 0;
}

int com_handleFlooding(struct com_Connection *con){
	if(con == NULL || con->type == -1)
		return 1;

	// Check time inbetween messages (too fast = quit)
	double timeDifference = difftime(time(NULL), con->lastMsg);
	con->lastMsg = time(NULL);

	// If past the flood interval, reset counters
	con->timeElapsed += (atomic_int) timeDifference;
	if(con->timeElapsed > fig_Configuration.floodInterval){
		con->timeElapsed = 0;
		con->req = 0;
	}

	con->req++;

	if(con->req > fig_Configuration.floodNum){
		log_logMessage("User sending messages too fast.", INFO);
		return 1;
	}

	return -1;
}

// Searches for and kicks users that surpassed their message timeouts
int com_timeOutConnections(int timeOut, struct com_ConnectionList *cList){
	for(int i = 0; i < cList->max; i++){
		struct com_Connection *con = &cList->cons[i];
		if(con == NULL)
			continue;

		pthread_mutex_lock(&con->mutex);
		int type = con->type;
		int diff = (int) difftime(time(NULL), con->lastMsg);
		int pinged = con->pinged;
		pthread_mutex_unlock(&con->mutex);

		if(type == USER || type == SERVER){ // Ports can't be timed out
			if(diff > timeOut){
				log_logMessage("User timeout.", INFO);
				com_sendStr(con, "QUIT :Connection timeout.\n");
			} else if(pinged == -1 && diff > timeOut/2){ // Ping user
				com_sendStr(con, "PING :Timeout imminent.\n");

				con->pinged = 1;
			}
		}
    }

    return 1;
}

void *com_communicateWithClients(void *param){
	struct com_ConnectionList *cList = param;
    int *epollfd = &cList->epollfd;
	struct com_Connection *con;

	struct epoll_event events[10];
    int num;

	// Find this thread's condition
	pthread_cond_t *finishAction;
	for(int i = 0; i < fig_Configuration.threads; i++){
		if(threads[i].thread == pthread_self()){
			finishAction = &threads[i].finishAction;
			break;
		}
	}
	
    while(1){
        num = epoll_wait(*epollfd, events, ARRAY_SIZE(events), 50); 
		if(num == -1){
			if(num == EINTR)
				continue; // OK to continue

			log_logError("epoll_wait", ERROR);
			exit(EXIT_FAILURE);
		} else if (num == 0) { // timeout try again
			pthread_cond_broadcast(finishAction);
			continue;
		}

		for(int i = 0; i < num; i++){
			con = events[i].data.ptr;
			if(con->type == PORT){
				com_acceptClient(con, *epollfd);
				continue;
			}

			pthread_mutex_lock(&con->mutex);
			int canUse = con->type;
			pthread_mutex_unlock(&con->mutex);

			if(canUse == -1) // doesn't exist anymore
				continue;

			if(events[i].events & EPOLLIN){
				com_readFromSocket(&events[i], *epollfd);
			} else if (events[i].events & EPOLLOUT){
				com_writeToSocket(&events[i], *epollfd);
			} // Add disconnection/error

			pthread_cond_broadcast(finishAction);
		}
    }
    
    return NULL;
}

int com_setupIOThreads(struct fig_ConfigData *config, struct com_ConnectionList *cList){
    char buff[BUFSIZ];
    int numThreads = config->threads;

	threads = calloc(numThreads, sizeof(struct com_Thread));
	if(threads == NULL){
		log_logError("Error allocating space for IO threads", FATAL);
		exit(EXIT_FAILURE);
	}

    int ret = 0;
    for(int i = 0; i < numThreads; i++){
        ret = pthread_create(&threads[i].thread, NULL, com_communicateWithClients, cList);
        if(ret != 0){
            snprintf(buff, ARRAY_SIZE(buff), "Error with pthread_create: %d", ret);
            log_logMessage(buff, ERROR);
            return -1;
        }

		ret = pthread_mutex_init(&threads[i].mutex, NULL);
		if (ret < 0){
			log_logError("Error initalizing pthread_mutex.", ERROR);
			return -1;
		}

		ret = pthread_cond_init(&threads[i].finishAction, NULL);
		if (ret < 0){
			log_logError("Error initalizing pthread_cond.", ERROR);
			return -1;
		}
    }
	snprintf(buff, ARRAY_SIZE(buff), "Successfully listening on %d threads", numThreads);
	log_logMessage(buff, INFO);

    return numThreads;
}

struct com_Connection *com_createConnection(int type, struct com_SocketInfo *sockInfo, struct com_ConnectionList *cList){
	struct com_Connection *con;
	for(int i = 0; i < cList->max; i++){
		con = &cList->cons[i];

		pthread_mutex_lock(&con->mutex);
		int avaliable = con->type;
		pthread_mutex_unlock(&con->mutex);

		if(avaliable == -1) // It is avaliable
			break;
			
		if(i == cList->max - 1) // None found
			return NULL;
	}

	con->type = type;
	con->cList = cList;
	if(sockInfo != NULL)
		memcpy(&con->sockInfo, sockInfo, sizeof(struct com_SocketInfo));

	con->lastMsg = time(NULL); // Starting time
	con->pinged = 0; // Dont ping on registration, but still kick if idle
	con->req = 0;
	con-> timeElapsed = 0;

	return con;
}

void com_deleteConnection(struct com_Connection *con){
	if(con == NULL)
		return;

	pthread_mutex_lock(&con->mutex);
	// Remove from user if needed
	if(con->user && con->type == USER)
		con->user->con = NULL;
	con->user = NULL;
	con->type = -1;

	close(con->sockInfo.socket);
	con->sockInfo.socket = -1;
	close(con->sockInfo.socket2);
	con->sockInfo.socket2 = -1;

	// Remove queue items
	link_empty(&con->sendQ, 1);
	pthread_mutex_unlock(&con->mutex);
}

int com_acceptClient(struct com_Connection *servPort, int epoll_sock){
	char buff[BUFSIZ];

	struct com_SocketInfo *serverSock = &servPort->sockInfo;
	struct sockaddr_storage cliAddr;
	socklen_t cliAddrSize = sizeof(cliAddr);

	//Accept client's connection and log its IP
	struct com_SocketInfo newCli;
	int client = accept(serverSock->socket, (struct sockaddr *)&cliAddr, &cliAddrSize);

	// Reset listening socket
	struct epoll_event ev = {.events = EPOLLIN | EPOLLONESHOT}; 
	ev.data.ptr = servPort;
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
		ssl = SSL_new(servPort->cList->ctx);
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

	/* Wait for other threads to finish before creating connection
	   so that newCon isn't accidentally reused incorrectly */

	// Get mutex
	pthread_mutex_t *mutex;
	for (int i = 0; i < fig_Configuration.threads; i++){
		if(threads[i].thread == pthread_self()){
			mutex = &threads[i].mutex;
			break;
		}
	}

	// Wait for other threads
	for (int i = 0; i < fig_Configuration.threads; i++){
		// Dont wait for self
		if(threads[i].thread == pthread_self())
			continue;
		
		pthread_cond_wait(&threads[i].finishAction, mutex);
	}

	struct com_Connection *newCon = com_createConnection(USER, &newCli, servPort->cList);

	// Give a user struct to this connection
	struct usr_UserData *user = usr_createUser(UNREGISTERED_NAME, servPort->cList->sLists, newCon);
	if(user == NULL){
		snprintf(buff, ARRAY_SIZE(buff), "Server is full, try again later.");
		send(client, buff, strlen(buff), 0);
		close(client);
		close(newCli.socket2);
		com_deleteConnection(newCon);
		return -1;
	}
	ev.data.ptr = newCon;

	pthread_mutex_lock(&newCon->mutex);
	newCon->user = user;
	pthread_mutex_unlock(&newCon->mutex);

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
