#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "url.h"

void freeURLComponents(URLComponents *url) {
  free(url->scheme);
  free(url->host);
  free(url->port);
  free(url->path);
  free(url->query);
  free(url->fragment);
  free(url);
}

URLComponents *parseUrl(const char *url) {
  URLComponents *components = malloc(sizeof(URLComponents));
  if (components == NULL) {
    return NULL; 
  }

  components->scheme = NULL;
  components->host = NULL;
  components->port = NULL;
  components->path = NULL;
  components->query = NULL;
  components->fragment = NULL;

  char *schemeEnd = strchr(url, ':');
  if (schemeEnd != NULL) {
    components->scheme = strndup(url, schemeEnd - url);
  }

  char *hostStart = (components->scheme) ? schemeEnd + 3 : (char*) url; // skip "://"
  char *hostEnd = strchr(hostStart, ':');
  if (hostEnd == NULL) {
    hostEnd = strchr(hostStart, '/');
  }
  if (hostEnd == NULL) {
    hostEnd = strchr(hostStart, '?');
  }
  if (hostEnd == NULL) {
    hostEnd = strchr(hostStart, '#');
  }
  if (hostEnd == NULL) {
  hostEnd = hostStart + strlen(hostStart);
  }
  components->host = strndup(hostStart, hostEnd - hostStart);


  char *portStart = strchr(hostStart, ':');
  if (portStart != NULL) {
    components->port = strndup(portStart + 1, hostEnd - portStart - 1);
  }


  char *pathStart = (components->port) ? hostEnd : strchr(hostStart, '/');
  if (pathStart != NULL) {
    char *pathEnd = strchr(pathStart, '?');
    if (pathEnd == NULL) {
      pathEnd = strchr(pathStart, '#');
    }
    if (pathEnd == NULL) {
      pathEnd = pathStart + strlen(pathStart);
    }
    components->path = strndup(pathStart, pathEnd - pathStart);
  }


  char *queryStart = strchr(url, '?');
  if (queryStart != NULL) {
    char *queryEnd = strchr(queryStart, '#');
    if (queryEnd == NULL) {
      queryEnd = queryStart + strlen(queryStart);
    }
    components->query = strndup(queryStart + 1, queryEnd - queryStart - 1);
  }


  char *fragmentStart = strchr(url, '#');
  if (fragmentStart != NULL) {
    components->fragment = strdup(fragmentStart);
  }

  return components;
  
}
