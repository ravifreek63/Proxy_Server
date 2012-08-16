/*
 * RetriveFromCache takes the uri and finds the cached object associated with 
 * this file and returns it as a string.
 * TODO(rishav) : exception handling
 */

#include "cache.h"
cacheDirectory cache;
double cacheSizeLimit;
double currentCacheSize;

string
RetreiveFromCache(string uri) {
  cout << "In RetreiveFromCache " << endl;
  string md5hashFilename = md5(uri);
  string filePath = string(cacheDirPath) + md5hashFilename;
  // read the file
  FILE *fp;
  long len;
  char *buf;
  fp = fopen(filePath.c_str(), "rb");
  if (fp == NULL) {
    cout << "BUG :: File with path " << filePath <<
            "  Not Found  In Function RetreiveFromCache() " << endl;
  }
  fseek(fp, 0, SEEK_END); // go to file end
  len = ftell(fp); // get position at end (length)
  cout << "File Length=" << len << endl;
  fseek(fp, 0, SEEK_SET); // go to beginning.
  buf = (char *) malloc(len + 1); // malloc buffer
  memset(buf, 0, len + 1);
  fread((void *) buf, len, 1, fp); // read into buffer
  cout << buf << endl;
  fclose(fp);
  return string(buf, len); // careful the buf may contain /0 in case
  // the retreived object is a bin file
}

bool
insert_cache(string filename, cacheBucket cBucket,
        cache_eviction_strategy strategy) {
  currentCacheSize += cBucket.size_in_bytes;
  //cout << currentCacheSize << endl;
  // check if strategy is LRU_THRESHOLD and the size is above the THRESHOLD_LRU
  if (strategy == LRU_THRESHOLD && cBucket.size_in_bytes > THRESHOLD_LRU)
    return true; // Pretend inserted
  cacheDirectory::iterator lru_it;
  pair < map < string, cacheBucket >::iterator, bool > ret;
  if (currentCacheSize > cacheSizeLimit) {
    if (!evict_cache(strategy)) {
      cout << "Evict failed :: BUG" << endl;
      return false;
    }
  }
  //  Adding To The Cache The cache bucket that stores the mapping from the file
  cBucket.cache_hits = 0;
  ret = cache.insert(pair < string, cacheBucket > (filename, cBucket));
  if (ret.second == false) {
    cout << "Cache Insert Failed Due To Duplication :: BUG" << endl;
    return false;
  } else {
    //cout << "Insertion Done To The File Successfully" << endl;
    return true;
  }
}

bool
evict_cache(cache_eviction_strategy strategy) {

  switch (strategy) {
    case (LRU):
      if (evict_policy(LRU))
        return true;
      break;

    case (LFU):
      if (evict_policy(LFU))
        return true;
      break;

    case (LOWEST_LATENCY_FIRST):
      if (evict_policy(LOWEST_LATENCY_FIRST))
        return true;
      break;

    case (LOG_SIZE_LRU):
      if (evict_policy(LOG_SIZE_LRU))
        return true;
      break;

    case (SIZE):
      if (evict_policy(SIZE))
        return true;
      break;

    case (LRU_THRESHOLD):
      if (evict_policy(LRU_THRESHOLD))
        return true;
      break;

    case (GREEDY_DUAL_SIZE):
      if (evict_policy(GREEDY_DUAL_SIZE))
        return true;

    case (LEAST_HIT):
      if (evict_policy(LEAST_HIT))
        return true;
      break;

    default:
      cout << strategy << endl;
      return false;
  }
  return false;
}

bool
evict_policy(cache_eviction_strategy strategy) {
 // cout << "On Eviction " << currentCacheSize << endl;
  cacheDirectory::iterator hashmap_it = cache.begin();
  cacheDirectory::iterator lru_it = cache.begin();
  cacheBucket victim_hash_entry = (*(cache.begin())).second;
  cacheBucket current_hash_entry;
  bool compare;
  for (hashmap_it = cache.begin(); hashmap_it != cache.end(); hashmap_it++) {
    //cout << "eviction loop" << endl;
    current_hash_entry = (*hashmap_it).second;

    switch (strategy) {
      case LRU:
        compare =
                compare_less(current_hash_entry.last_access_time,
                victim_hash_entry.last_access_time);
        break;

      case LOG_SIZE_LRU:
        compare = log(current_hash_entry.size_in_bytes) ==
                log(victim_hash_entry.size_in_bytes);
        if (compare) {
          compare =
                  compare_less(current_hash_entry.last_access_time,
                  victim_hash_entry.last_access_time);
        } else {
          compare = log(current_hash_entry.size_in_bytes) <
                  log(victim_hash_entry.size_in_bytes);
        }
        break;

      case LRU_THRESHOLD:
        compare =
                compare_less(current_hash_entry.last_access_time,
                victim_hash_entry.last_access_time);
        break;

      case LOWEST_LATENCY_FIRST:
        compare =
                current_hash_entry.download_time <
                victim_hash_entry.download_time;
        break;

      case SIZE:
        compare =
                current_hash_entry.size_in_bytes <
                victim_hash_entry.size_in_bytes;
        break;

      case GREEDY_DUAL_SIZE:
        compare =
                (pow(current_hash_entry.cache_hits, W_P) *
                current_hash_entry.size_in_bytes <
                pow(victim_hash_entry.cache_hits,
                W_P) * victim_hash_entry.size_in_bytes);
        break;

      case LFU:
        compare = current_hash_entry.cache_hits < victim_hash_entry.cache_hits;
                //compare_more(current_hash_entry.last_access_time,
                //victim_hash_entry.last_access_time);
        break;

      case LEAST_HIT:
        compare =
                (current_hash_entry.cache_hits < victim_hash_entry.cache_hits);
        break;
    }
    if (compare) {
     lru_it = hashmap_it;
     victim_hash_entry = current_hash_entry;
    }
    
  }
    currentCacheSize -= victim_hash_entry.size_in_bytes;
   // string filename = (*lru_it).first;
   // string filepath = string(cacheDirPath) + filename;
  //  unlink(filename.c_str());
    cache.erase(lru_it); 
    //cout << "eviction done" << endl;
    return true;
}

bool
compare_less(struct timeval t1, struct timeval t2) {
  return (t1.tv_sec < t2.tv_sec);
}

bool
compare_more(struct timeval t1, struct timeval t2) {
  return (t1.tv_sec > t2.tv_sec);
}

