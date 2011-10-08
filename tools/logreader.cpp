#include "logreader.h"

int LogReader::nextEntry(char* buffer, int maxSize)
{
    struct net_log_header* header=(struct net_log_header*)buffer;
    _in.read(buffer, sizeof(*header));
    if (_in.gcount() != sizeof(*header) || header->length > maxSize)
	return false;
    _in.read(buffer+sizeof(*header), header->length-sizeof(*header));
    if (_in.gcount() != header->length - sizeof(*header))
	return false;
    return true;
}

LogReader::LogReader(istream& in) : _in(in)
{
}

LogReader::~LogReader()
{
}

int LogReader::rewind()
{
    _in.seekg(0,ios::beg);
    return !_in.fail();
}
