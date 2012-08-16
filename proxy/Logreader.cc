#include "cache.h"

#define LINE_SIZE 2048
#define MAX_LINE 1024

container
HashOfObjects (string filename)
{
  container url_container;
  char line[LINE_SIZE];
  FILE *fr = fopen (filename.c_str (), "rt");	/* open the file for reading */
  string parent_url = "";
  while (fgets (line, LINE_SIZE, fr) != NULL)
    {
      string temp (line);
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

int
main ()
{
  container url_container = HashOfObjects ("accessLog.txt");
  container::iterator container_ii = url_container.begin ();
  vector < bucket >::iterator vec_ii;

  for (; container_ii != url_container.end (); container_ii++)
    { 
      cout << "Parent_url: _" << container_ii->first << "_" << endl;
      for (vec_ii = (container_ii->second).begin ();
	   vec_ii != (container_ii->second).end (); vec_ii++)
	{
	  string url = vec_ii->obj_url;
	  double size = (vec_ii->params).size_in_bytes;
	  double latency = (vec_ii->params).download_time;
	  // perform the magic
	  cout << url << "  " << size << "  " << latency << endl;
	}
    }
}
