/* 
 * proxy_server_simulator
 *
 * TEAM MEMBERS:
 *     Rishav Anand, rishav@iitg.ernet.in 
 *     Ravi Tandon,  r.tandon@iitg.ernet.in
 * 
 */

#include "cache.h"


#define BUFFER_LEN (1024*10)	// max size of a browser request.
#define MAX_LINE 1024		// array size containing uri info.
//#define EVICTION_STRATEGY LRU	// The eviction polict used.
#define LOG_FILE_NAME "access_log.dat"
#define LINE_SIZE 2048
#define RANGE 50.00
// Global variables
cache_eviction_strategy evictionStrategy;
extern cacheDirectory cache;
extern double currentCacheSize;
extern double cacheSizeLimit;
// CacheMisses till now
long int cache_misses;
double cacheMissesBytes;
long int downloadLatency;
double totalSize = 2000000;

container
HashOfObjects (string filename)
{
  container url_container;
  char line[LINE_SIZE];
  FILE *fr = fopen (filename.c_str (), "rt");	/* open the file for reading */
  string parent_url = "";
  while (fgets (line, LINE_SIZE, fr) != NULL)
    {
      string temp (line, strlen (line) - 2);    // check
      if ((temp.length() > 0)  && (temp[temp.length() - -1] == '/')) // To remove the '/' for the URLs ending in '/'
      {
		temp[temp.length() -1] = '\0';
      }
      //cout << "temp-" << temp << endl;
      int pos = temp.find ("http://");
      if (pos == 0)
	{
	  parent_url = temp;
	  continue;
	}
      temp =
	temp.substr (temp.find ("http://"),
		     temp.size () - temp.find ("http://"));
//log entry now:- [http://202.141.80.14/sites/all/themes/salamanderskins/style.css?x:1:6985:8:5758.000000:19.771000:]         
      char char_string[LINE_SIZE];
      strcpy (char_string, temp.c_str ());
      char *p = strtok (char_string, ":");
      string http = string (p);
      p = strtok (NULL, ":");
      string later_url = string (p);
      strtok (NULL, ":");
      strtok (NULL, ":");
      strtok (NULL, ":");
      p = strtok (NULL, ":");
      string size = string (p);
      p = strtok (NULL, ":");
      string latency = string (p);
      string url = http + ":" + later_url;

      bucket url_bucket;
      url_bucket.obj_url = url;
      cacheBucket parameters;
      parameters.size_in_bytes = atoi (size.c_str ());
      //totalSize += parameters.size_in_bytes;
      parameters.download_time = atof (latency.c_str ());
      url_bucket.params = parameters;
      container::iterator it = url_container.find (parent_url);
      if (it != url_container.end ())
	{
	  //element found;
	  url_container[parent_url].push_back (url_bucket);

	}
      else
	{
	  vector < bucket > temp_vec;
	  temp_vec.push_back (url_bucket);
	  url_container[parent_url] = temp_vec;
	}
    }
  fclose (fr);			/* close the file prior to exiting the routine */
  return url_container;
}


void *
WorksAsProxy (string url, double size, double latency)
{
  cacheDirectory::iterator cacheIter = cache.find (url);
  bool found = (cacheIter != cache.end ());
  cacheBucket objectMetaData;	// To store the info characterstic of the html object
  long int misses_now;		//  Number of misses till this request - to prevent race condition on cache_misses
  if (found)
    {
      // Key exists, simply respond
    //  cout << "Response Key Found" << endl;
      // Updating the last access time - Set Every Time A Request To Access The Cache Item Is Made
      gettimeofday (&(((*cacheIter).second).last_access_time), NULL);	// adds present time
      (cacheIter->second).cache_hits++;	//  Found hence increment hit count
    }
  else
    {
    //cout << "Miss on" << url << endl;
      misses_now = ++cache_misses;	// Records the total numbers of cache-misses
      // Open the result file in append mode
      downloadLatency += latency;
      // Update the metadata info with the object file size
      objectMetaData.size_in_bytes = size;
      cacheMissesBytes += size;

      // now cache the object for later users
      //cout << "Now writing into the cache." << endl;
      gettimeofday (&(objectMetaData.last_access_time), NULL);	// adds present time
      // In Cache Time Set At The Time Of Fetching The Object In The Cache 
      gettimeofday (&(objectMetaData.in_cache_time), NULL);	// adds present time
      //  Update the object metadata with total download time
      objectMetaData.download_time = latency;
      // create the file with name md5 hash of URI
      string md5hashFilename = md5 (url.c_str ());
      string hashFilePath = string (cacheDirPath) + md5hashFilename;
      insert_cache (url, objectMetaData, evictionStrategy);
 
    }
}

// Read the accessed objects in a list
list <string> accessList;  
list<string>::iterator accessListIterator;

void buildAccessList (string fileName, container url_container)
{
 FILE *fp_url_list = fopen ("outFile", "rt");	/* open the file for reading */
 char line[MAX_LINE];
 string lineStr;
 size_t foundNl;    
  while (fgets (line, MAX_LINE, fp_url_list) != NULL)
    {      
      if (strlen (line) == 0)
	continue;      
      lineStr = string (line);
      foundNl = lineStr.find ('\n', 0);
      if (foundNl != string::npos)
         lineStr = lineStr.substr (0, lineStr.length()-1);      
     if (lineStr[lineStr.length()-1] == '/')
          lineStr = lineStr.substr (0, lineStr.length()-1);  
     accessList.push_back (lineStr);
    }  
  fclose (fp_url_list);
}


int
main (void)
{
//totalSize=0;
  container url_container = HashOfObjects ("accessLog.txt");

  // Building the access List 
  buildAccessList ("outFile", url_container);
  FILE *fp_cache;
  fp_cache = fopen ("Result2.dat", "w");
  container::iterator container_ii;
  vector < bucket >::iterator vec_ii;
  string lineStr;  
  double cacheSizeIncrement = totalSize/RANGE;
  int numEvictionStrategies = 8;
  int evictionStrategyNum = 0;
  while (evictionStrategyNum < numEvictionStrategies)
  {
  // changing 
  //cout << "EvictionStrategyNum : " << evictionStrategyNum << endl;
  evictionStrategy = cache_eviction_strategy (evictionStrategyNum);
  cacheSizeLimit = 0;
  // changing cache size 
  while (cacheSizeLimit < totalSize)
  {
  cacheSizeLimit += cacheSizeIncrement;
 // cout << cacheSizeLimit << endl;
  // Empty the cache   
  cache.clear();
  cache_misses = 0;   // Set Cache Misses = 0
  cacheMissesBytes = 0;
  downloadLatency = 0;
  currentCacheSize=0;
   for (accessListIterator = accessList.begin(); accessListIterator != accessList.end(); accessListIterator++)
   {
      lineStr = *(accessListIterator);
     // printf("Requesting: _%s\n", lineStr.c_str());
      container_ii = url_container.find (lineStr);
      if (container_ii != url_container.end ())
	{
	 // cout << "Found" << endl;	// found
	 // cout << "Requesting: _" << string (line, strlen (line) - 2) << "_" << endl;
	  for (vec_ii = (container_ii->second).begin ();
	       vec_ii != (container_ii->second).end (); vec_ii++)
	    {
	      // fetch all the objects
	      string url = vec_ii->obj_url;
	      double size = (vec_ii->params).size_in_bytes;
	      double latency = (vec_ii->params).download_time;
	      // perform the magic
	     // printf ("Requesting child: %s\n", url.c_str ());
	      WorksAsProxy (url, size, latency);
	      //cout << url << "  " << size << "  " << latency << endl;
	    }
	}
      else
	{
	  //printf("Not found\n");
	}
    }
     fprintf (fp_cache, "%lf\t%d\t%ld\t%ld\t%lf\n", cacheSizeLimit, evictionStrategyNum, cache_misses, downloadLatency, cacheMissesBytes);
//     fprintf ("%lf\t%d\t%ld\t%ld\t%lf\n", cacheSizeLimit, evictionStrategyNum, cache_misses, downloadLatency, cacheMissesBytes);
     fflush (fp_cache);
  }
    evictionStrategyNum++;
    fprintf(fp_cache, "\n\n");
  }
  fclose (fp_cache);
  return 0;
}
