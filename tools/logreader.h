#ifndef _LOGREADER_H
#define _LOGREADER_H

//includes
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <net_log_format.h>

class LogReader
{
 public:
    LogReader(istream& in);
    ~LogReader();
    int nextEntry(char* buffer, int maxSize);
    int rewind();
private:
    istream& _in;
};

#endif


