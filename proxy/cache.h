#ifndef __CACHE_H__
#define __CACHE_H__

#define MAX_FILE_NAME_SIZE 100
#define MAX_URI_SIZE 500
//#define MAX_CACHE_SIZE 100	// Defining maximum number of entries within the cache to be 100.


#include "md5.h"
#include <arpa/inet.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>

#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <list>

#define cacheDirPath "cacheDir/"
#define W_P 1                   // Weight required in the Greedy_DUAL_SIZE eviction policy 
#define THRESHOLD_LRU 1024      // The size threshold for LRU_THRESHOLD eviction policy 

using namespace std;

// proxy mode TODO: Implement missing SEQUENTIAL

enum MODE {
  SEQUENTIAL, MULTI_THREADING, MULTI_PROCESSING
};

// Structure to pass paremeters to the proxy threads

struct args {
  int clientfd;
  FILE *logfilefd;
  char clientsAddr[INET6_ADDRSTRLEN];
};

typedef struct hash_entry {
  // time to live is for implementing the time to live 
  struct timeval last_access_time;
  // in_cache_time basically deletes all the files that have exceeded their time to live value
  struct timeval in_cache_time;
  // Size of the object in bytes
  double size_in_bytes;
  // Latency to retrieve the document
  double download_time;
  // Hits received by an object
  long int cache_hits;
} cacheBucket;

// We keep a global in memory data structure for keeping the meta data
typedef map < string, cacheBucket > cacheDirectory;

enum cache_eviction_strategy {
  LRU, LFU, LEAST_HIT, LOWEST_LATENCY_FIRST, LOG_SIZE_LRU, 
  LRU_THRESHOLD, SIZE, GREEDY_DUAL_SIZE, 
};

string RetreiveFromCache(string uri);
bool insert_cache(string filename, cacheBucket cBucket,
        cache_eviction_strategy strategy);
bool evict_policy(cache_eviction_strategy strategy);
bool evict_cache(cache_eviction_strategy strategy);
bool compare_less(struct timeval t1, struct timeval t2);
bool compare_more(struct timeval t1, struct timeval t2);
void do_gettimeofday(struct timeval *);

// Required in the simulator code
typedef struct obj_bucket
{
  string obj_url;
  cacheBucket params;
} bucket;

typedef map < string, vector < bucket > >container;
 
#endif

