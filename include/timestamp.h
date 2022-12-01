#ifndef TIMESTAMP_HEADER
#define TIMESTAMP_HEADER

#include "time.h"

//get time
const char *ntpServer = "ptbtime2.ptb.de";              // changed for compatibility, pool.ntp.org takes too much time sometimes

//timestamp function
unsigned long initial_timestamp;
unsigned long timestamp;
unsigned long getTimestamp()
{
    time_t now;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        return (0);
    }
    time(&now);
    return now;
}

#endif