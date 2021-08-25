#ifndef communication_h
#define communication_h

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <limits.h>
#include <time.h>
#include <stdatomic.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include "ssl.h"
#include "linkedlist.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))
#define MAX_MESSAGE_LENGTH 2048

#define SERVER	0
#define PORT	1
#define USER	2

// Jobs for queues
struct com_QueueJob {
    int type;
    struct com_Connection *con;
	union {
		char str[1024];
		struct chat_Message *msg; 
	};
};

//struct to store data about the socket, and its file descriptor
struct com_SocketInfo {
    atomic_int socket;
	atomic_int socket2; // Used for filtering epoll writing events
	atomic_int useSSL;
	SSL *ssl;
    struct sockaddr_storage addr;
};

// Store data about a connection
struct com_Connection {
	atomic_int type; // SERVER, PORT, or USER
	struct link_List sendQ;
	struct com_SocketInfo sockInfo;
	struct com_ConnectionList *cList; // Head of the connections

	// Keep track of time, too fast = kick, too slow = kick
	time_t lastMsg; 
	atomic_int req, timeElapsed;
	atomic_int pinged; // Send only one ping to prevent spam from server

	union {
		struct usr_UserData *user;
	};

	pthread_mutex_t mutex;
};

struct com_ConnectionList {
	struct link_List cons;
	struct chat_ServerLists *sLists; // Location of server lists
	atomic_int max;
	int epollfd;
	SSL_CTX *ctx;
	pthread_mutex_t mutex;
};

#include "hstring.h"
#include "logging.h"
#include "config.h"
#include "chat.h"
#include "user.h"

/* This header defines functions that handle all of the
 * Sending and receiving of data that the server will handle
 */

// Setup the server's socket
struct com_ConnectionList *init_server();

// close server socket
void com_close();

// Send a string to a specified connection
int com_sendStr(struct com_Connection *con, char *msg);

// Insert selected job into the queue
int com_insertQueue(struct com_QueueJob *job);

// Convert sockaddr to a string to display the client's IP in string form
int getHost(char ipstr[INET6_ADDRSTRLEN], struct sockaddr_storage addr, int protocol);

// Will read data from the socket and properly send it for processing
int com_readFromSocket(struct epoll_event *userEvent, int epollfd);

// Writes avaliable queue data to socket
int com_writeToSocket(struct epoll_event *userEvent, int epollfd);

// Too many messages = kick
int com_handleFlooding(struct com_Connection *con);

// Searches for and kicks users that surpassed their message timeouts
int com_timeOutConnections(int timeOut, struct com_ConnectionList *cList);

// Handle all incoming data from the client
void *com_communicateWithClients(void *param);

// Setup the threads to start listening for incoming communication and send
// outbound data to clients
int com_setupIOThreads(struct fig_ConfigData *config, int *epollfd);

// Allocates a new connection
struct com_Connection *com_createConnection(int type, struct com_SocketInfo *sockInfo, struct com_ConnectionList *cList);

void com_deleteConnection(struct com_Connection *con);

//accept communication with clients
int com_acceptClient(struct com_Connection *servPort, int epoll_sock);

//start server socket based on configuration
int com_startServerSocket(int portNum, struct com_SocketInfo* sockAddr, int forceIPv4, int useSSL);

#endif
