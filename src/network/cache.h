#ifndef ARACHNYAA_CACHE
#define ARACHNYAA_CACHE

#include <time.h>


#ifdef _WIN32
  #include <direct.h> // Windows mkdir()
  #define mkdir(path, mode) _mkdir(path)
  #define CACHE_DIR "\\AppData\\Local\\arachnyaa\\cache\\"
#else 
  #include <sys/stat.h> // POSIX mkdir()
  #define CACHE_DIR "/.cache/arachnyaa/"
#endif

typedef struct {
  char *url;
  char *content;
time_t lastModified;
  int cached; // 1 if cached, 0 otherwise
} CachedResource;

extern CachedResource *cache; // cache array
extern int cacheCount; // current cache index
extern int cacheCapacity; // current cache capacity

void cacheInit(int capacity);

void cacheDestroy();

void createCacheDirectory();

char *getCacheDirectory();

void saveCacheToFile();

void loadCacheFromFile();
int cacheLookUp(const char *url, CachedResource *resource);

int oldestCachedIndex();

void cacheAdd(const char *url, char *content, time_t lastModified);

void cachePop(int index);

void cacheRemove(const char *url);

#endif // !ARACHNYAA_CACHE
