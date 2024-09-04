#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cache.h"



CachedResource *cache;
int cacheCount = 0; // current cache index
int cacheCapacity = 0; // current cache capacity

void cacheInit(int capacity) {
  if (capacity <= 0) {
    fprintf(stderr, "Invalid cache capacity\n");
    exit(2);
  }
  cacheCapacity = capacity;
  cache = malloc(capacity * sizeof(CachedResource));
  if (cache == NULL) {
    fprintf(stderr, "Memory allocation error.\n");
    exit(2);
  }

  createCacheDirectory();

  loadCacheFromFile();

}

void cacheDestroy() {
  for (int i = 0; i < cacheCount; i++) {
    if (cache[i].cached) {
      free(cache[i].url);
      free(cache[i].content);
    }
  }
  free(cache);
}

char* getCacheDirectory() {
  char *cacheDir = malloc(sizeof(char)*256);

#ifdef _WIN32
  char *home = getenv("USERPROFILE");
#else 
  char *home = getenv("HOME");
#endif
  if (home == NULL) {
    fprintf(stderr, "Error: Unable to determine home directory");
    return(NULL);
  }

  snprintf(cacheDir, sizeof(char)*256, "%s%s", home, CACHE_DIR);
  return cacheDir;
}

void createCacheDirectory() {
  char *cacheDir = getCacheDirectory();

  // Construct the cache directory path
  printf("attempt creating cache directory at %s...\n", cacheDir);

  if (mkdir(cacheDir, 0700) != 0) {
    if (errno != EEXIST) {
      fprintf(stderr, "Error creating cache directory: %s\n", cacheDir);
      exit(2);
    }
    printf("directory already exist.\n");
  }
  free(cacheDir);
}

void saveCacheToFile() {
  char *cacheFile = getCacheDirectory();
  if (cacheFile == NULL) {
    fprintf(stderr, "Error: cannot get cache directory.");
    return;
  }
  strcat(cacheFile, "cache.dat");
  FILE *fp = fopen(cacheFile, "w");
  if (fp == NULL) {
    fprintf(stderr, "Error opening cache file for writing.\n");
    return;
  }

  for (int i = 0; i < cacheCount; i++) {
    if (cache[i].cached) {
      printf("saved cache %i \"%s\" at %ld\n",i ,cache[i].url, cache[i].lastModified);
      fprintf(fp, "%s\n", cache[i].url);
      fprintf(fp, "%ld\n", cache[i].lastModified);
      fwrite(cache[i].content, 1, strlen(cache[i].content), fp);
      fprintf(fp, "\n");
    }
  }
  fclose(fp);
  free(cacheFile);
}

void loadCacheFromFile() {
  char *cacheFile = getCacheDirectory();
  if (cacheFile == NULL) {
    fprintf(stderr, "Error: cannot get cache directory.");
    return;
  }
  strcat(cacheFile, "cache.dat");

  FILE *fp = fopen(cacheFile, "r");
  if (fp == NULL) {
    return;
  }

  char url[2048];
  char content[8192];
  time_t lastModified;

  while (fscanf(fp, "%s\n", url) != EOF) {
    fscanf(fp, "%ld\n", &lastModified);
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    content[0] = '\0';
    while ((read = getline(&line, &len, fp)) != -1) {
      if (line[0] == '\n'|| line[0] == '\r') {
        break;
      }
      strcat(content, line);
    }
    cacheAdd(url, content, lastModified);
    free(line);
  }
  
    
  fclose(fp);
}

int cacheLookUp(const char *url, CachedResource *resource) {
  for (int i = 0; i < cacheCount; i++) {
    if (strcmp(cache[i].url, url) == 0 && cache[i].cached) {
      if (time(NULL) - cache[i].lastModified > 150) { //discard old cache
        cachePop(i);
        printf("Cache for this site is too old!\n");
        return 0;
      }
      *resource = cache[i];
      return 1; // cache found
    }
  }
  return 0; // cache not found
}

int oldestCachedIndex() {
  time_t oldest = 0;
  int index = -1;
  for (int i = 0; i < cacheCount; i++) {
    if (cache[i].cached && cache[i].lastModified < oldest) {
      oldest = cache[i].lastModified;
      index = i;
    }
  }
  if (index == -1) {
    return -1; // not found
  } else {
    return index;
  }
}

void cacheAdd(const char *url, char *content, time_t lastModified) {
  CachedResource *cacheDest;
  if (cacheCount < cacheCapacity) {
    cacheDest = cache + cacheCount;
    cacheCount++;
  } else {
    cacheDest = cache + oldestCachedIndex();
    //cacheRemove()
  }
    cacheDest->url = strdup(url);
    cacheDest->content = strdup(content);
    cacheDest->lastModified = lastModified;
    cacheDest->cached = 1;
}

void cachePop(int index) {
    free(cache[index].url);
    free(cache[index].content);
    cache[index].cached=0;
}

void cacheRemove(const char *url) {
  for (int i = 0; i < cacheCount; i++) {
    if (strcmp(cache[i].url, url) == 0) {
      cachePop(i);
      memmove(&cache[i], &cache[i+1], (cacheCount - i + 1) * sizeof(CachedResource));
      cacheCount--;
      break;
    }
  }
}
