#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <fcntl.h>

#include "socket.h"

long TIMEOUT_SEC = 10;

int createSocket(int domain, int type, int protocol) {
  int sockfd = socket(domain, type, protocol);
  if (sockfd == -1) {
    perror("socket creation error");
    exit(1);
  }
  return sockfd;
}

int connectToServer(const char *hostname, const char *port) {
  printf("Connecting to host:%s on port:%s\n", hostname, port);
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; // IPv4
  hints.ai_socktype = SOCK_STREAM; // TCP
  int addrInfo = getaddrinfo(hostname, port
              , &hints, &res);
  int socketFd = createSocket(res->ai_family, res->ai_socktype, 0);

  if (connect(socketFd, res->ai_addr, res->ai_addrlen) == -1) {
    perror("connect error");
    freeaddrinfo(res);
    exit(1);
  }

  freeaddrinfo(res);
  return socketFd;
}

int sendData(int socketFd, const char *buffer, size_t length) {
  int byteSent = send(socketFd, buffer, length, 0);
  if (byteSent == -1) {
    perror("send error");
    exit(1);
  }
  return byteSent;
}

int receiveData(int socketFd, char *buffer, size_t bufferSize) {
  struct timeval timeout;
  timeout.tv_sec = TIMEOUT_SEC;
  timeout.tv_usec = 0;


  int flags = fcntl(socketFd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl error");
    return -1;
  }
  

  if (fcntl(socketFd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl error");
    return -1;
  }

  // set timeout for socket
  if (setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
    perror("setsockopt error");
    return -1;
  }

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(socketFd, &readfds);

  int result = select(socketFd + 1, &readfds, NULL, NULL, &timeout);

  if (result == -1) {
    perror("select error");
    return -1;
  } else if (result == 0) {
    fprintf(stderr, "Request timed out.\n");
    return -2;
  } else if (FD_ISSET(socketFd, &readfds)) {

    int bytesReceived = recv(socketFd, buffer, bufferSize, 0);
    if (bytesReceived == -1) {
      perror("recv error");
      return -1;
    } else if (bytesReceived == 0) {
      // connection closed by server
      return 0;
    }
    return bytesReceived;
  } else {
    // should not happend
    return -1;
  }

}

void closeSocket(int socketFd) {
  if(close(socketFd) == -1) {
    perror("close error");
  }
}
