#include "utility.hpp"

#include <sstream>
#include <iomanip>
#include <cmath>
#include <ctime>


using namespace std;


TRandom3 randGen(0);


string GetRandomName(bool useTime /*= true*/, unsigned postfixLength /*= 3*/)
{
    ostringstream result;
    
    
    if (useTime)
    {
        time_t rawtime;
        struct tm *timeinfo;
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        
        result.fill('0');
        result << setw(2) << timeinfo->tm_year - 100 << setw(2) << timeinfo->tm_mon + 1 <<
         setw(2) << timeinfo->tm_mday << "_";
        result << setw(2) << timeinfo->tm_hour << setw(2) << timeinfo->tm_min << setw(2) <<
         timeinfo->tm_sec << "_";
    }
    
    
    for (unsigned i = 0; i < postfixLength; ++i)
    {
        if (randGen.Rndm() > 0.5)
            result << char('A' + ('Z' - 'A') * randGen.Rndm());
        else
            result << char('a' + ('z' - 'a') * randGen.Rndm());
    }
    
    
    return result.str();
}


long RandomInt(long maximum)
{
    return floor(randGen.Rndm() * maximum);
}
