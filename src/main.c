#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "http.h"
#include "cache.h"
#include "socket.h"
#include "url.h"

int main(int argc, char** argv) {
  char *url = "http://httpbin.org/ip"; //dummy default;
  char requestBuffer[MAX_REQUEST_LENGTH];
  char responseBuffer[MAX_RESPONSE_LENGTH];
  char headers[1024];
  char body[2048];
  CachedResource cachedResource;
  
  if (argc > 1) {
    url = argv[1];
  }

  URLComponents *urlComponents = parseUrl(url);
  
  cacheInit(100);

  if (cacheLookUp(url, &cachedResource)) {
    printf("Resouce found in cache!\n");
    printf("Content: %s\n", cachedResource.content);
  } else {
    printf("Resouce not found in cache, fetching from server...\n");
    int requestLength = createHttpRequest("GET", url, NULL, NULL, requestBuffer);
    printf("Request created\n");
    
    printf("Connecting to host:%s on port:%s\n", urlComponents->host, urlComponents->port);
    int socketFd = connectToServer(urlComponents->host,(urlComponents->port) ? urlComponents->port : "80");
    printf("Socket connected\n");
    
    printf("Sending Http request...\n");
    printf("Request:\n %s\n", requestBuffer);
    sendHttpRequest(socketFd, requestBuffer, requestLength);
    int bytesReceived = receiveHttpResponse(socketFd, responseBuffer, sizeof(responseBuffer));
    printf("Recieve %d bytes from %s\n",bytesReceived, url);
    
    parseHttpResponse(responseBuffer, headers, body);
    printf("Parsing Response...\n");

    time_t lastModified = time(NULL);
    cacheAdd(url, body, lastModified);
    
    printf("Content: %s\n", body);
    
    closeSocket(socketFd);
  }
    saveCacheToFile();
    
    cacheDestroy();
    freeURLComponents(urlComponents);

}
