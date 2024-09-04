#include <asm-generic/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "socket.h"
#include <sys/socket.h>
#include "url.h"
  
#include "http.h"



int createHttpRequest(char *method, char *url, char *headers, char *body, char *requestBuffer) {
  URLComponents *components = parseUrl(url);
  // construct the HTTP request string
  snprintf(requestBuffer, MAX_REQUEST_LENGTH, "%s %s HTTP/1.1\r\n", method, url);
  
  // Add default headers (for HTTP/1.1)
  strcat(requestBuffer, "Host: ");
  strcat(requestBuffer, components->host);
  strcat(requestBuffer, "\r\n");

  strcat(requestBuffer, "User-Agent: Arachnyaa/0.1\r\n");
  strcat(requestBuffer, "Connection: close");

  if (headers) {
    strcat(requestBuffer, headers);
  }
  if (body) {
    strcat(requestBuffer, "Content-Length: ");
    char contentLength[10];
    snprintf(contentLength, 10, "%zu", strlen(body));
    strcat(requestBuffer, contentLength);
    strcat(requestBuffer, "\r\n\r\n");
  }
  strcat(requestBuffer, "\r\n\r\n");
  
  if (body) {
    strcat(requestBuffer, body);
  }
  freeURLComponents(components);
  
  return strlen(requestBuffer);
}

int sendHttpRequest(int socketFd, char *requestBuffer, int requestLength) {
  return sendData(socketFd, requestBuffer, requestLength);
}

int receiveHttpResponse(int socketFd, char *responseBuffer, int bufferSize) {
   return receiveData(socketFd, responseBuffer, bufferSize);
}

int parseHttpResponse(char *responseBuffer, char *headers, char *body) {
  char *headerEnd = strstr(responseBuffer, "\r\n\r\n");
  if (headerEnd == NULL) {
    return -1; //Malformed response
  }
  strncpy(headers, responseBuffer, headerEnd - responseBuffer);
  headers[headerEnd - responseBuffer] = '\0';
  strcpy(body, headerEnd + 4);
  return 0;
}
