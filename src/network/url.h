#ifndef ARACHNYAA_URL
#define ARACHNYAA_URL

typedef struct {
  char *scheme;
  char *host;
  char *port;
  char *path;
  char *query;
  char *fragment;
} URLComponents;

void freeURLComponents(URLComponents *url);

URLComponents *parseUrl(const char *url);

#endif // !ARACHNYAA_URL
