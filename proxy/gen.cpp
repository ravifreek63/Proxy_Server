#include <iostream>
#include <fstream>
#include <string>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

fstream urlFile;
fstream powerLawFile;
fstream outFile;
int spreadFactor = 6;
int numSamples = 100;

int
main ()
{
  urlFile.open ("urlFile.txt", fstream::in);
  powerLawFile.open ("PowerLaw.txt", fstream::in);
  outFile.open ("outFile", fstream::out);
  int i = 0, val, randomNum, initPosition, j, pos, startPos, endPos;
  int sum = 0;
  string URL;
  for (; i < numSamples; i++)

    {
      powerLawFile >> val;
      sum += val;
    }

  //cout << "Number of Values = " << sum << endl;
  int y;

  //cin >> y;
  int totalValues = sum;
  bool isPresent[totalValues];
  for (i = 0; i < totalValues; i++)

    {
      isPresent[i] = false;
    }
  string buffer[totalValues];
  powerLawFile.seekg (0, ios::beg);
  for (i = 0; i < numSamples; i++)

    {
      powerLawFile >> val;
      if (val == 0)
	continue;
      urlFile >> URL;
      randomNum = rand () % totalValues;
      initPosition = randomNum;
      while (isPresent[initPosition])

	{
	  initPosition = (initPosition + 1) % totalValues;
	}

// initPosition = 0, number of values are val
      isPresent[initPosition] = true;
      buffer[initPosition] = URL;
      startPos = (initPosition - (spreadFactor / 2 * val));
      endPos = (initPosition + (spreadFactor / 2 * val));
      if (startPos < 0)

	{
	  startPos = 0;
	  endPos = startPos + spreadFactor * val;
	  if (endPos >= totalValues)
	    endPos = totalValues - 1;
	}

      else if (endPos >= totalValues)

	{
	  endPos = totalValues - 1;
	  startPos = (endPos - spreadFactor / 2 * val);
	  if (startPos < 0)
	    startPos = 0;
	}
      for (j = 1; j < val; j++)

	{
	  randomNum = (rand () % (2 * val)) + startPos;
	  pos = randomNum % totalValues;
	  while (isPresent[pos])

	    {
	      pos = (pos + 1) % (totalValues);

	      //cout << pos << endl;
	    }
	  isPresent[pos] = true;
	  buffer[pos] = URL;
	}
    }
  for (i = 0; i < totalValues; i++)

    {
      outFile << buffer[i] << endl;
    }
  urlFile.close ();
  powerLawFile.close ();
  outFile.close ();
  return 0;
}
