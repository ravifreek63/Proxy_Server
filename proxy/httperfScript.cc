#include <stdlib.h>
#include <string>
#include <fstream>

#define LOG_FILE_NAME "outfile.dat"
#define BATCH_INT 50
#define PROXY_ADDR "10.9.21.12"
#define PROXY_PORT "5000"

using namespace std;

int
main ()
{

  ifstream ifs (LOG_FILE_NAME);
  string temp;
  long int batch_size = 0;

  while (getline (ifs, temp))
    {
      string system_command =
	"httperf --server " + string (PROXY_ADDR) + " --port " +
	string (PROXY_PORT) + " --uri " + temp +
	" --rate 1 --num-conn 1 --num-call 1 --timeout 30";
      system (system_command.c_str ());

      batch_size++;
      if (batch_size % BATCH_INT == 0)
	sleep (1);
    }
}
