/* 
 * proxy_server
 *
 * TEAM MEMBERS:
 *     Rishav Anand, rishav@iitg.ernet.in 
 *     Ravi Tandon,  r.tandon@iitg.ernet.in
 * 
 *  TODO :-
 *         Add a couple of cool features-see the resource file
 */


#include "cache.h"


#define MULTI_THREAD 1
#define PERSISTENCE 0
#define PORT "5000"                // the port users will be connecting to.
#define BUFFER_LEN (1024*10)        // max size of a browser request.
#define BACKLOG 500                // how many pending connections queue will hold.
#define MAXLINE 1024                // array size containing uri info.
#define uDELAY  0                // microsecond delay for the select call.
#define CLIENT_TIMEOUT        30        // Timeout for client response, seconds.
#define SERVER_TIMEOUT  2        // Timeout for server response, seconds.
#define SERVER_TIMEOUT_INIT  30        // Timeout for server response, seconds.
#define PROXY_SLEEPS (500000000L/(1024*1024*10))        // Proxy sleeps for this time after dispatching a thread.
#define MAX_OUTSTANDING 7        // Limits the maximum no. of outstanding client connection with proxy server.
#define PARENT_PROXY 1                // Is a parent proxy present?
#define PARENT_PROXY_ADDR "172.16.25.55"        // Address of a parent proxy.
#define EVICTION_STRATEGY LRU        // The eviction polict used.
#define LOG_FILE_NAME "access_log.dat"


// Global variables


MODE mode = MULTI_THREADING;        // Mode of operation of the proxy 
int num_connections, num_disconnections;
extern cacheDirectory cache;


// Mutex to allow shared resource sharing amoung server threads
pthread_mutex_t mutex_connections = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_thread_entry = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_log = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;


// Here I define semaphore_map to limit the outstanding client
// connections with the proxy server
map < string, sem_t > clientSema;


// CacheMisses till now
long int cache_misses;


string
replaceConnectionHeader (string ori_str)
{
  string s1 = "Connection: close";
  //  string s2 = "Connection: open";
  //  string s2 = "Connection:";
  string s2 = "Keep-Alive: 115\r\nProxy-Connection: keep-alive";
  int pos = ori_str.find (s1);
  if (pos == -1)
    {
      return ori_str;
    }
  ori_str.replace (pos, s1.length (), s2);
  return ori_str;
}


void *
get_in_addr (struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
    {
      return &(((struct sockaddr_in *) sa)->sin_addr);
    }


  return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int
parse_uri (char *uri, char *hostname, char *pathname, int *port)
{
  char *hostbegin;
  char *hostend;
  char *pathbegin;
  int len;


  if (strncasecmp (uri, "http://", 7) != 0)
    {
      hostname[0] = '\0';
      return -1;
    }


  /* Extract the host name */
  hostbegin = uri + 7;
  hostend = strpbrk (hostbegin, " :/\r\n\0");
  len = hostend - hostbegin;
  strncpy (hostname, hostbegin, len);
  hostname[len] = '\0';


  /* Extract the port number */
  *port = 80;                        /* default */
  if (*hostend == ':')
    *port = atoi (hostend + 1);


  /* Extract the path */
  pathbegin = strchr (hostbegin, '/');
  if (pathbegin == NULL)
    {
      pathname[0] = '\0';
    }
  else
    {
      pathbegin++;
      strcpy (pathname, pathbegin);
    }


  return 0;
}


// Returns 1 if all is well


int
readNecho (int sockfd, char *buf, int browser_fd, string * response,
           string clientaddr, cacheBucket * object_meta_data)
{
  cout << "I am inside readNecho." << endl;
  cout << "sockfd: " << sockfd << endl;
  cout << "browser_fd: " << browser_fd << endl;
  bool once_not_over = true;
  bool running = true;
  double elapsed_time_total = 0.0;
  while (running)                // loop for select 
    {
      struct timeval tv;
      timeval t_init, t_final;
      // Start the timer to compute the total download time for this object
      gettimeofday (&t_init, NULL);


      fd_set readfds;


      if (once_not_over)
        {
          tv.tv_sec = SERVER_TIMEOUT_INIT;
          once_not_over = false;
        }
      else
        {
          tv.tv_sec = SERVER_TIMEOUT;
        }
      tv.tv_usec = 0;


      string responseBuffer = "";
      int offset = 0, numbytes = 0, retval;


      FD_ZERO (&readfds);
      FD_SET (sockfd, &readfds);
      retval = select ((sockfd + 1) * 500, &readfds, NULL, NULL, &tv);
      if (retval < 0)
        {
          fprintf (stderr, "Error In select < 0\n");
          perror ("select: read&echo: ");
          continue;
        }                        /* else if (retval == 0) {
                                   fprintf(stderr, "Server Not Responding\n");
                                   fprintf(stderr, "Error In select = 0\n");
                                   fflush(stderr);
                                   close(browser_fd);
                                   close(sockfd);
                                   sem_post(&clientSema[clientaddr]);
                                   pthread_exit(0);
                                   } */
      memset (buf, 0, BUFFER_LEN * sizeof (buf[0]));
      int bytesRead;
      if ((bytesRead = read (sockfd, buf, BUFFER_LEN * sizeof (buf[0]))) <= 0)
        {
          printf ("End reading.\n");
          break;
        }
      // stop timer
      gettimeofday (&t_final, NULL);
      if (strstr (buf, "404") != NULL)
        {
          fprintf (stderr, "404 Error\n");
          send (browser_fd, "HTTP/1.0 500 Server Error\r\n\r\n", 29, 0);        // EXPERIMENT
          //        perror ("select: read&echo: ");
          fflush (stderr);
          close (browser_fd);
          close (sockfd);
          sem_post (&clientSema[clientaddr]);
          pthread_exit (0);
        }
      responseBuffer = string (buf, bytesRead);


      // compute and print the elapsed time in millisec
      double elapsed_time_chunk = (t_final.tv_sec - t_init.tv_sec) * 1000.0;        // sec to ms
      elapsed_time_chunk += (t_final.tv_usec - t_init.tv_usec) / 1000.0;        // us to ms
      elapsed_time_total += elapsed_time_chunk;


      if (PERSISTENCE)
        {
          responseBuffer = replaceConnectionHeader (responseBuffer);
        }


      *response += responseBuffer;
      //cout << "Receives  HTTP response from the webserver: numbytes" << numbytes << " : Client Fd " << browser_fd << ": Server FD " << sockfd << endl;
      int numBytesSent = 0;
      int leftToRead = bytesRead;
      cout << "Chunk length: " << bytesRead << endl;
      while (numBytesSent =
             send (browser_fd, buf + numBytesSent, leftToRead, 0) < bytesRead)
        {
          if (numBytesSent != 1)
          cout << "Sent: " << numBytesSent << endl;
          // int numBytesSent = write(browser_fd, buf, bytesRead);      
          leftToRead -= numBytesSent;
          if (numBytesSent == -1)
            {
              perror ("send_read&echo: ");
              printf
                ("Dying because failed to send the response back to browser\n");
              close (browser_fd);
              close (sockfd);
              sem_post (&clientSema[clientaddr]);
              pthread_exit (0);
            }
        }
      //cout << "Now Sends the  HTTP response to the browser: Numbytes: " << numBytesSent << endl;
      // cout << "Sending Response To The WebBrowser : NumBytes" << numBytesSent <<  " : Client Fd " << browser_fd << ": Server FD " << sockfd << endl;


    }


  //  Update the object metadata with total download time
  object_meta_data->download_time = elapsed_time_total;


  //shutdown (browser_fd, SHUT_WR);
  shutdown (browser_fd, SHUT_RDWR);
  close (sockfd);
  return 1;
}


bool
AllOnes (int arr[], int size)
{
  for (int j = 0; j < size; j++)
    {
      if (arr[j] == 0)
        return false;
    }
  return true;
}


/*
 * ProxyAsClient
 * 
 * Forwards the modified HTTP request message to the original host server. 
 * Return 1 if "ALL IS WELL".
 *
 */
int
ProxyAsClient (char *hostname, int serverPort, string proxyReq,
               string * response, int browser_fd, string clientaddr,
               cacheBucket * object_meta_data)
{
  cout << "I am inside ProxyAsClient." << endl;
  if (PARENT_PROXY)
    {
      strcpy (hostname, PARENT_PROXY_ADDR);
      serverPort = 3128;
    }
  int websiteFd, numbytes;
  char buf[BUFFER_LEN], serverPortStr[10];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];


  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;


  sprintf (serverPortStr, "%d", serverPort);
  if ((rv = getaddrinfo (hostname, serverPortStr, &hints, &servinfo)) != 0)
    {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));
      return -1;
    }
  cout << "Before resolving host name" << endl;
    // loop through all the results to find the number of results obtained
    int desired_maximum = 0;


    for (p = servinfo; p != NULL; p = p->ai_next)
      {
        desired_maximum++;
      }
    // Randomly pick one of the address to connect - for load balancing
    // Iterate till all the addresses have been seen and none of them could connect
    // For keeping track of seen addresses  a bit map array has been maintained
    int *tracker = new int[desired_maximum];    // bitmap array
    for (int k = 0; k < desired_maximum; k++)
      tracker[k] = 0;
    bool connected = false;
    //  Trivial Load balancer
    while (AllOnes (tracker, desired_maximum) == false)
      {
        int r = (((double) rand ()) / RAND_MAX) * desired_maximum;
        tracker[r] = 1;
        p = servinfo;
        while (r > 0)
        {
          p = p->ai_next;
          r--;
        }
        if ((websiteFd = socket (p->ai_family, p->ai_socktype,
                               p->ai_protocol)) == -1)
        {
          perror ("Proxy client: socket");
          continue;
        }
        if (connect (websiteFd, p->ai_addr, p->ai_addrlen) == -1)
        {
          close (websiteFd);
          perror ("Proxy client: connect");
          continue;
        }
        break;
      }


    if (p == NULL)
      {
        fprintf (stderr, "Proxy client: failed to connect\n");
        return -2;
      }
//   loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next)
    {
      cout << "Looping to resolve host name." << endl;
      if ((websiteFd = socket (p->ai_family, p->ai_socktype,
                               p->ai_protocol)) == -1)
        {
          perror ("Proxy client: socket");
          continue;
        }
      int x = fcntl (websiteFd, F_GETFL, 0);        // Get socket flags
      fcntl (websiteFd, F_SETFL, x | O_NONBLOCK);        // Add non-blocking flag


      fd_set myset;
      struct timeval tv;
      int valopt;
      socklen_t lon;
      int res;
      bool has_timed_out = false;
      // Connecting on a non-blocking socket
      if (res = connect (websiteFd, p->ai_addr, p->ai_addrlen) < 0)
        {
          if (errno == EINPROGRESS)
            {
              fprintf (stderr, "EINPROGRESS in connect() - selecting\n");
              do
                {
                  tv.tv_sec = 300;
                  tv.tv_usec = 0;
                  FD_ZERO (&myset);
                  FD_SET (websiteFd, &myset);
                  res = select (websiteFd + 1, NULL, &myset, NULL, &tv);
                  if (res < 0 && errno != EINTR)
                    {
                      fprintf (stderr, "Error connecting %d - %s\n", errno,
                               strerror (errno));
                      exit (0);
                    }
                  else if (res > 0)
                    {
                      // Socket selected for write 
                      lon = sizeof (int);
                      if (getsockopt
                          (websiteFd, SOL_SOCKET, SO_ERROR,
                           (void *) (&valopt), &lon) < 0)
                        {
                          fprintf (stderr, "Error in getsockopt() %d - %s\n",
                                   errno, strerror (errno));
                          exit (0);
                        }
                      // Check the value returned... 
                      if (valopt)
                        {
                          fprintf (stderr,
                                   "Error in delayed connection() %d - %s\n",
                                   valopt, strerror (valopt));
                          exit (0);
                        }
                      break;
                    }
                  else
                    {
                      fprintf (stderr, "Timeout in select() - Cancelling!\n");
                      close (websiteFd);
                      has_timed_out = true;
                      break;
                    }
                }
              while (1);
            }
          else
            {
              fprintf (stderr, "Error connecting %d - %s\n", errno,
                       strerror (errno));
              close (websiteFd);
              perror ("Proxy client: connect");
              continue;
            }
        }
      if (has_timed_out) {
        continue;
      }
      break;
    }


  if (p == NULL)
    {
      fprintf (stderr, "Proxy client: failed to connect\n");
      return -2;
    }
  cout << "Above inet_top." << endl;
  inet_ntop (p->ai_family, get_in_addr ((struct sockaddr *) p->ai_addr),
             s, sizeof s);
  printf ("Proxy client: connecting to %s\t on socket %d\n", s, websiteFd);


  freeaddrinfo (servinfo);        // all done with this structure


  int numbytesSent =
    send (websiteFd, proxyReq.c_str (), proxyReq.length (), 0);
  if (numbytesSent == -1)
    {
      perror ("Send: ");
      printf ("Error In send\n");
      send (browser_fd, "HTTP/1.0 500 Server Error\r\n\r\n", 29, 0);
      close (browser_fd);
      close (websiteFd);
      sem_post (&clientSema[clientaddr]);
      pthread_exit (0);
    }


  //  Receive HTTP response from the web server
  cout << "Request Sent To The Web-Server : Request Length " << numbytesSent
    << " : socket FD " << websiteFd << endl;


  return readNecho (websiteFd, buf, browser_fd, response, clientaddr, object_meta_data);        //  reads all the available data from the socket


}


/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void
Write_format_log_entry (FILE * logFileFd, char *clientsAddr, char *uri,
                        int size, unsigned int pid, char *logString, long int misses)
{
  time_t now;
  char time_str[MAXLINE];
  unsigned long host;
  unsigned char a, b, c, d;


  /* Get a formatted time string */
  now = time (NULL);
  strftime (time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime (&now));


  /* Return the formatted log entry string */


  int rc = pthread_mutex_lock (&mutex_log);
  /********** Critical Section begins*******************/
  //printf("This is the log entry: %s: %s %s %d %u\n", time_str, clientsAddr, uri, size, pid);
  if (logString != NULL)
    fprintf (logFileFd, "%s: %s %s %d %u %s %ld\n", time_str, clientsAddr, uri,
             size, pid, logString, misses);
  else
    fprintf (logFileFd, "%s: %s %s %d %u %ld\n", time_str, clientsAddr, uri, size,
             pid, misses);
  fflush (logFileFd);  // flushing or repositioning required
  /********** Critical Section ends*********************/
  rc = pthread_mutex_unlock (&mutex_log);
}


// TODO This part of the code is not up to date -- instead the function WorkAsProxy is up to date


int
WorksAsProxy_sequential (int browser_fd, struct args *argStruct)
{
  return (0);                        //pthread_exit(0);
}


void
sigchld_handler (int s)
{
  while (waitpid (-1, NULL, WNOHANG) > 0);
}


void *
WorksAsProxy (void *argStruct)
{
  // Copy the arguments in thread's local variables
  int browser_fd = ((struct args *) (argStruct))->clientfd;
  FILE *log_fd = ((struct args *) (argStruct))->logfilefd;
  string clientaddr = string (((struct args *) (argStruct))->clientsAddr);
  pthread_mutex_unlock (&(mutex_thread_entry));


  char buff[BUFFER_LEN], buf[BUFFER_LEN];        // buff stores the browser's request
  int receivedBytes;
  fd_set readset;
  int retval;
  struct timeval timeout;
  // Set up for select, to read initial client request. 
  memset (&timeout, 0, sizeof (timeout));
  timeout.tv_sec = CLIENT_TIMEOUT;
  timeout.tv_usec = 0;
  FD_ZERO (&readset);
  FD_SET (browser_fd, &readset);


  if ((retval =
       select (browser_fd * 100 + 1, &readset, NULL, NULL, &timeout)) < 0)
    {
      fprintf (stderr, "Error on select: %s\n", strerror (errno));
      send (browser_fd, "HTTP/1.0 500 Server Error\r\n\r\n", 29, 0);
      close (browser_fd);        //  destroy the client connection after serving it
      pthread_exit (0);
    }
  else if (retval == 0)
    {
      fprintf (stderr, "Client not responding, disconnecting\n");
      send (browser_fd, "HTTP/1.0 404 Server Error\r\n\r\n", 29, 0);
      close (browser_fd);        //  destroy the client connection after serving it
      pthread_exit (0);
    }


  // Here the proxy accepts browser's request
  memset (buff, 0, BUFFER_LEN);
  if ((receivedBytes = recv (browser_fd, buff, BUFFER_LEN - 1, 0)) < 0)
    {
      send (browser_fd, "HTTP/1.0 500 Server Error\r\n\r\n", 29, 0);
      close (browser_fd);        //  destroy the client connection after serving it
      perror ("Recv_WorkAsProxy: ");
      //sem_post (&clientSema[clientaddr]);
      pthread_exit (0);
    }
  else
    {
      buff[receivedBytes] = '\0';
      cout << "Proxy Server Received Request From WebBrowser On Socket : " <<
        browser_fd << endl;


      // Check Semaphore status - regulate service to client request
      cout << " S_later: " << clientaddr << endl;
      sem_wait (&clientSema[clientaddr]);
      int value;
      sem_getvalue (&clientSema[clientaddr], &value);
      printf ("The value of the semaphors is %d\n", value);


      //printf("Browser request :\n'%s'\n", buff);
      //  Parse the request to extract the first line of the header           
      string firstLine = string (buff).substr (0, string (buff).find ("\n"));


      //  initialize variables to extract from request
      char method[MAXLINE], uri[MAXLINE], version[MAXLINE], hostname[MAXLINE],
        pathname[MAXLINE];


      //  parse the request
      int rs;
      if ((rs =
           sscanf (firstLine.c_str (), "%s %s %s", method, uri,
                   version)) != 0)
        {
          if (strcmp ("GET", method))
            {
              send (browser_fd, "HTTP/1.0 500 Server Error\r\n\r\n", 29, 0);
              close (browser_fd);
              sem_post (&clientSema[clientaddr]);
              pthread_exit (0);
            }
          //  change the version to HTTP/1.0 if necessary.
          if (strcmp (version, "HTTP/1.1") == 0)
            {
              sprintf (buf, "%s %s %s", method, uri, "HTTP/1.0");        //EXPERIMENT
            }


          int serverPort;
          if (parse_uri (uri, hostname, pathname, &serverPort) != -1)
            {
              //printf("URI : %s\n", uri);
              //printf("Hostname : %s\n", hostname);
              //printf("Pathname : %s\n", pathname);
              //printf("ServerPort : %d\n", serverPort);
              //printf("Remaining request : \n%s\n", buff + firstLine.length());
              string proxyReq =
                string (buf) + string (buff + firstLine.length ());
              //printf("Proxy request : \n'%s'\n", proxyReq.c_str());
              //  Act as a client and send the modified request to the serverPort
              string response;
              int responseSize;        // required to log connection info
              /*
               * First check if the data is available in the cache before making an
               * explicit request to the web server
               */
              
              pthread_mutex_lock (&(cache_mutex));
              cacheDirectory::iterator cacheIter = cache.find (string (uri));
              bool found = (cacheIter != cache.end ());
              cacheBucket objectMetaData;        // To store the info characterstic of the html object
              long int misses_now;  //  Number of misses till this request - to prevent race condition on cache_misses
              if (found)
                {
                  // Key exists, simply respond
                  //sprintf(logstring, "Response Key Found");
                  cout << "Response Key Found" << endl;
                  response = RetreiveFromCache (uri);        // Reads cached web-pages
                  printf ("Received Response\n");
                  responseSize = response.length ();
                  // Updating the last access time - Set Every Time A Request To Access The Cache Item Is Made
                  gettimeofday (&(((*cacheIter).second).last_access_time), NULL);        // adds present time
                  (cacheIter->second).cache_hits++;        //  Found hence increment hit count


                  pthread_mutex_unlock (&(cache_mutex));
                  int numBytesSent =
                    send (browser_fd, response.c_str (), responseSize, 0);
                  if (numBytesSent == -1)
                    {
                      printf ("Error In send\n");
                      perror ("WorksAsProxy: ");
                      close (browser_fd);
                      sem_post (&clientSema[clientaddr]);
                      pthread_exit (0);
                    }


                  shutdown (browser_fd, SHUT_RDWR);
                }
              else
                {
                  misses_now = ++cache_misses;  // Records the total numbers of cache-misses
                  
                  // Open the result file in append mode
                  FILE *fp_cache;
                  fp_cache = fopen ("Result.dat", "w");
                  fprintf(fp_cache, "%ld", misses_now);
                  fclose(fp_cache);
                  // printf ("Response Not Found\n");
                  fflush (stdout);
                  pthread_mutex_unlock (&(cache_mutex));
                  //  Act as a client and send the modified request to the serverPort
                  if ((responseSize =
                       ProxyAsClient (hostname, serverPort, proxyReq,
                                      &response, browser_fd, clientaddr,
                                      &objectMetaData)) < 0)
                    {
                      send (browser_fd, "HTTP/1.0 500 Server Error\r\n\r\n",
                            29, 0);
                      close (browser_fd);
                      printf ("Error In ProxyAsClient\n");
                      sem_post (&clientSema[clientaddr]);
                      pthread_exit (0);
                    }
		   // Check whether the request has 
                   int pos_Req = proxyReq.find("Cache-Control: no-cache");
                   if (pos_Req != -1)
                          {
                             pos_Req = proxyReq.find ("Pragma: no-cache");
			     if (pos_Req != -1)
			     {
				pos_Req = response.find ("Pragma: no-cache");
			     }
			  }
                   
                  if (pos_Req != -1)
                  {
                  // Update the metadata info with the object file size
                  objectMetaData.size_in_bytes = response.length ();


                  // now cache the object for later users
                  cout << "Now writing into the cache." << endl;
                  gettimeofday (&(objectMetaData.last_access_time), NULL);        // adds present time
                  // In Cache Time Set At The Time Of Fetching The Object In The Cache 
                  gettimeofday (&(objectMetaData.in_cache_time), NULL);        // adds present time


                  // create the file with name md5 hash of URI
                  string md5hashFilename = md5 (uri);
                  string hashFilePath =
                    string (cacheDirPath) + md5hashFilename;
                  FILE *p = NULL;
                  p = fopen (hashFilePath.c_str (), "w");
                  if (p == NULL)
                    {
                      printf ("Error in opening file.. '%s' \n",
                              hashFilePath.c_str ());
                      close (browser_fd);
                      sem_post (&clientSema[clientaddr]);
                      pthread_exit (0);
                    }
                  fwrite (response.c_str (), response.length (), 1, p);
                  fclose (p);
                  printf
                    ("\nWritten Successfuly in the file, Response of size %d\n",
                     (int) response.length ());
                  pthread_mutex_lock (&(cache_mutex));
                  insert_cache (string (uri), objectMetaData,
                                EVICTION_STRATEGY);
                  pthread_mutex_unlock (&(cache_mutex));
                }
		}


              //  log Connection details
              Write_format_log_entry (log_fd,
                                      ((struct args *)
                                       argStruct)->clientsAddr, uri,
                                      responseSize, getpid (), NULL, misses_now);
            }
          else
            {
              printf ("URI unrecognized\n");
              send (browser_fd, "HTTP/1.0 500 Server Error\r\n\r\n", 29, 0);
              close (browser_fd);
              sem_post (&clientSema[clientaddr]);
              pthread_exit (0);
            }
        }
    }
  close (browser_fd);                //  destroy the client connection after serving it
  //pthread_mutex_lock (&mutex);
  num_disconnections++;
  printf ("Number of DisConnections %d\n", num_disconnections);
  //pthread_mutex_unlock (&mutex);
  printf ("Thread Exiting Successfully after forwarding traffic.\n");
  sem_post (&clientSema[clientaddr]);
  pthread_exit (0);
}


int
sleepnano ()
{
  struct timespec tim, tim2;
  tim.tv_sec = 0;
  tim.tv_nsec = PROXY_SLEEPS;


  if (nanosleep (&tim, &tim2) < 0)
    {
      printf ("Nano sleep system call failed \n");
      return -1;
    }


  printf ("Nano sleep successfull \n");


  return 0;
}


int
main (void)
{
  pthread_mutex_lock (&(mutex_thread_entry));
  cache_misses = 0;
  num_connections = 0;
  num_disconnections = 0;
  string logString;                //  Contains a proxy log entry


  //  register signal handler to ignore SIGPIPE
  if (signal (SIGPIPE, SIG_IGN) == SIG_ERR)
    {
      printf ("Unable to register SIG_IGN to SIGPIPE. Ending...");
      return -1;
    }
  int proxyFd, browser_fd;        // listen on sock_fd, new connection on browser_fd
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;        // connector's address information
  socklen_t sin_size;
  struct sigaction sa;
  int yes = 1;
  char s[INET6_ADDRSTRLEN];
  int rv;


  memset (&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;        // use my IP


  if ((rv = getaddrinfo (NULL, PORT, &hints, &servinfo)) != 0)
    {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (rv));
      return 1;
    }


  // loop through all the results and bind to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next)
    {
      if ((proxyFd = socket (p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
          perror ("server: socket");
          continue;
        }


      if (setsockopt (proxyFd, SOL_SOCKET, SO_REUSEADDR, &yes,
                      sizeof (int)) == -1)
        {
          perror ("setsockopt");
          exit (1);
        }


      if (bind (proxyFd, p->ai_addr, p->ai_addrlen) == -1)
        {
          close (proxyFd);
          perror ("server: bind");
          continue;
        }


      break;
    }


  if (p == NULL)
    {
      fprintf (stderr, "server: failed to bind\n");
      return 2;
    }


  freeaddrinfo (servinfo);        // all done with this structure


  if (listen (proxyFd, BACKLOG) == -1)
    {
      perror ("listen");
      exit (1);
    }
  printf ("server: waiting for connections...\n");
  // Done with all socket stuff


  // Open the log file in append mode
  FILE *fp;
  fp = fopen (LOG_FILE_NAME, "w");


  while (1)
    {                                // main accept() loop
      sin_size = sizeof (their_addr);
      browser_fd =
        accept (proxyFd, (struct sockaddr *) &their_addr, &sin_size);
      if (browser_fd == -1)
        {
          perror ("accept");
          continue;
        }


      inet_ntop (their_addr.ss_family,
                 get_in_addr ((struct sockaddr *) &their_addr), s, sizeof s);
      printf ("server: got connection from %s\t File Desciptor = %d\n", s,
              browser_fd);


      // initialize the semaphore if connection from a new client is received
      if (clientSema.find (string (s)) == clientSema.end ())
        {
          // not found
          sem_t t_semaphore;
          clientSema[string (s)] = t_semaphore;
          cout << " S: " << string (s) << endl;
          sem_init (&(clientSema[string (s)]), 0, MAX_OUTSTANDING);
        }
      else
        {
          // found - do nothing
        }


      struct args threadArgs;
      // Set the argument values
      threadArgs.clientfd = browser_fd;
      threadArgs.logfilefd = fp;
      strcpy (threadArgs.clientsAddr, s);


      if (mode == SEQUENTIAL)
        {
          WorksAsProxy_sequential (browser_fd, &threadArgs);
        }
      else if (mode == MULTI_THREADING)
        {
          // This mutex remove race condition on the thread arguments


          num_connections++;
          printf ("Current Number of Connections %d\n", num_connections);
          pthread_t thread_id;
          if (pthread_create
              (&thread_id, NULL, WorksAsProxy, (void *) &(threadArgs)) != 0)
            {
              perror ("Server: pthread_create");
              send (browser_fd, "HTTP/1.0 500 Server Error\r\n\r\n", 29, 0);
              pthread_mutex_unlock (&(mutex_thread_entry));
              close (browser_fd);
            }
        }
      pthread_mutex_lock (&(mutex_thread_entry));
      /*      else if (mode == MULTI_PROCESSING)
         {
         if (fork () == 0)
         {
         fprintf (fp,
         "New Connection Requested, Handling Process pid=%u\n",
         getpid ());
         fflush (fp);
         WorksAsProxy_sequential (browser_fd, &threadArgs);
         }
         } */
      sleepnano ();
    }
  return 0;
}
