#ifndef ARACHNYAA_HTTP
#define ARACHNYAA_HTTP

#define MAX_REQUEST_LENGTH 4096
#define MAX_RESPONSE_LENGTH 8192


int createHttpRequest(char *method, char *url, char *headers, char *body, char *requestBuffer);

int sendHttpRequest(int socketFd, char *requestBuffer, int requestLength);

int receiveHttpResponse(int socketFd, char *responseBuffer, int responseLength);

int parseHttpResponse(char *responseBuffer, char *headers, char *body);

#endif // !ARACHNYAA_HTTP
