#ifndef ARACHNYAA_SOCKET
#define ARACHNYAA_SOCKET
#include <stdlib.h> // size_t

extern long TIMEOUT_SEC;

int createSocket(int domain, int type, int protocol);

int connectToServer(const char *hostname, const char *port);

int sendData(int socketFd, const char *buffer, size_t length);

int receiveData(int socketFd, char *buffer, size_t bufferSize);

void closeSocket(int socketFd);


#endif // !ARACHNYAA_SOCKET
